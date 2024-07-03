#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdint>
#include <mach/mach_time.h>
#include <cmath>
#include <numeric>
#include <algorithm>

//==============================================================================
AudioToMIDIProcessor::AudioToMIDIProcessor()
    : sampleRate(0.0), detectedPitch(0.0f), previousPitch(0.0f), noteOn(false)
{
    setupVirtualMIDIPort();
}

AudioToMIDIProcessor::~AudioToMIDIProcessor()
{
    MIDIEndpointDispose(virtualEndpoint);
    MIDIPortDispose(midiOutPort);
    MIDIClientDispose(midiClient);
}

//==============================================================================
const juce::String AudioToMIDIProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioToMIDIProcessor::acceptsMidi() const { return true; }
bool AudioToMIDIProcessor::producesMidi() const { return true; }
bool AudioToMIDIProcessor::isMidiEffect() const { return false; }
double AudioToMIDIProcessor::getTailLengthSeconds() const { return 0.0; }
int AudioToMIDIProcessor::getNumPrograms() { return 1; }
int AudioToMIDIProcessor::getCurrentProgram() { return 0; }
void AudioToMIDIProcessor::setCurrentProgram(int index) {}
const juce::String AudioToMIDIProcessor::getProgramName(int index) { return {}; }
void AudioToMIDIProcessor::changeProgramName(int index, const juce::String& newName) {}

//==============================================================================
void AudioToMIDIProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    audioBuffer.resize(samplesPerBlock);
    DBG("Sample rate set to: " << sampleRate);
}

void AudioToMIDIProcessor::releaseResources() {}

void AudioToMIDIProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    const float* channelData = buffer.getReadPointer(0);

    // Fill audioBuffer with the current block's audio data
    audioBuffer.assign(channelData, channelData + numSamples);

    // Apply a basic noise gate with a lower threshold
    float rms = std::sqrt(std::accumulate(audioBuffer.begin(), audioBuffer.end(), 0.0f,
                 [](float a, float b) { return a + b * b; }) / static_cast<float>(numSamples));
    if (rms < 0.001f) // Lower threshold for better sensitivity
    {
        if (noteOn)
        {
            noteOn = false;
            convertPitchToMIDI(0.0f); // Note off
        }
        detectedPitch = 0.0f;
        DBG("Silence detected. Ignoring.");
    }
    else
    {
        // Detect pitch using YIN Algorithm
        float newPitch = detectPitchUsingYIN(audioBuffer);
        if (newPitch > 0.0f) {
            detectedPitch = 0.8f * detectedPitch + 0.2f * newPitch; // Smoothing filter
            if (detectedPitch != previousPitch)
            {
                previousPitch = detectedPitch;
                convertPitchToMIDI(detectedPitch); // Note on
                DBG("Detected pitch: " << detectedPitch);
            }
        } else {
            detectedPitch = 0.0f; // If no valid pitch is detected
            if (noteOn)
            {
                noteOn = false;
                convertPitchToMIDI(0.0f); // Note off
            }
        }
    }

    // Update the editor with the detected pitch
    updateNoteDisplay(detectedPitch);

    // Send generated MIDI messages
    midiMessages.swapWith(midiBuffer);
}

float AudioToMIDIProcessor::detectPitchUsingYIN(const std::vector<float>& buffer)
{
    const int bufferSize = static_cast<int>(buffer.size());
    std::vector<float> yinBuffer(bufferSize / 2, 0.0f);

    // Step 1: Calculate squared difference
    for (int tau = 1; tau < bufferSize / 2; ++tau)
    {
        for (int j = 0; j < bufferSize / 2; ++j)
        {
            yinBuffer[tau] += std::pow(buffer[j] - buffer[j + tau], 2);
        }
    }

    // Step 2: Cumulative mean normalized difference
    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < bufferSize / 2; ++tau)
    {
        runningSum += yinBuffer[tau];
        yinBuffer[tau] *= tau / runningSum;
    }

    // Step 3: Absolute threshold
    float threshold = 0.10f; // Adjusted for better sensitivity
    int tauEstimate = -1;
    for (int tau = 1; tau < bufferSize / 2; ++tau)
    {
        if (yinBuffer[tau] < threshold)
        {
            tauEstimate = tau;
            while (tau + 1 < bufferSize / 2 && yinBuffer[tau + 1] < yinBuffer[tau])
            {
                tau++;
                tauEstimate = tau;
            }
            break;
        }
    }

    // Step 4: Parabolic interpolation
    if (tauEstimate == -1)
    {
        return 0.0f; // No pitch detected
    }

    float betterTau;
    if (tauEstimate > 0 && tauEstimate < bufferSize / 2 - 1)
    {
        float s0 = yinBuffer[tauEstimate - 1];
        float s1 = yinBuffer[tauEstimate];
        float s2 = yinBuffer[tauEstimate + 1];
        betterTau = tauEstimate + (s2 - s0) / (2.0f * (2.0f * s1 - s2 - s0));
    }
    else
    {
        betterTau = static_cast<float>(tauEstimate);
    }

    return sampleRate / betterTau;
}

void AudioToMIDIProcessor::convertPitchToMIDI(float detectedPitch)
{
    static int previousMidiNoteNumber = -1;
    
    if (detectedPitch > 0.0f)
    {
        // Convert detected pitch to MIDI note number
        int midiNoteNumber = static_cast<int>(69 + 12 * std::log2(detectedPitch / 440.0f));

        if (midiNoteNumber != previousMidiNoteNumber)
        {
            if (previousMidiNoteNumber != -1)
            {
                // Create MIDI message for note off
                juce::MidiMessage noteOff(juce::MidiMessage::noteOff(1, previousMidiNoteNumber));
                noteOff.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
                midiBuffer.addEvent(noteOff, 0);
                sendMIDIMessage(noteOff);
            }

            // Create MIDI message for note on
            juce::MidiMessage noteOn(juce::MidiMessage::noteOn(1, midiNoteNumber, static_cast<uint8_t>(127)));
            noteOn.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
            midiBuffer.addEvent(noteOn, 0);
            sendMIDIMessage(noteOn);

            previousMidiNoteNumber = midiNoteNumber;
        }
    }
    else
    {
        if (previousMidiNoteNumber != -1)
        {
            // Send MIDI note off message
            juce::MidiMessage noteOff(juce::MidiMessage::noteOff(1, previousMidiNoteNumber));
            noteOff.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
            midiBuffer.addEvent(noteOff, 0);
            sendMIDIMessage(noteOff);
            previousMidiNoteNumber = -1;
        }
    }
}

void AudioToMIDIProcessor::updateNoteDisplay(float frequency)
{
    // Update the editor with the detected pitch
    if (auto* editor = dynamic_cast<AudioToMIDIProcessorEditor*>(getActiveEditor()))
    {
        if (frequency > 0)
        {
            int midiNoteNumber = static_cast<int>(69 + 12 * std::log2(frequency / 440.0f));
            juce::String noteName = juce::MidiMessage::getMidiNoteName(midiNoteNumber, true, true, 4);
            editor->setNoteAndFrequencyDisplay(noteName, frequency);
        }
        else
        {
            editor->setNoteAndFrequencyDisplay("N/A", 0.0f);
        }
    }
}

void AudioToMIDIProcessor::setupVirtualMIDIPort()
{
    MIDIClientCreate(CFSTR("AudioToMIDIVirtualClient"), nullptr, nullptr, &midiClient);

    // Attempt to create a new virtual source
    if (MIDISourceCreate(midiClient, CFSTR("AudioToMIDIVirtualSource"), &virtualEndpoint) != noErr)
    {
        // Fall back to using the IAC Driver if creating a new source fails
        virtualEndpoint = MIDIGetSource(0); // Assuming the first source is the IAC Driver
    }

    MIDIOutputPortCreate(midiClient, CFSTR("AudioToMIDIOutputPort"), &midiOutPort);
}

void AudioToMIDIProcessor::sendMIDIMessage(const juce::MidiMessage& message)
{
    MIDITimeStamp timeStamp = mach_absolute_time();
    std::vector<UInt8> buffer(message.getRawData(), message.getRawData() + message.getRawDataSize());
    MIDIPacketList packetList;
    MIDIPacket* packet = MIDIPacketListInit(&packetList);
    packet = MIDIPacketListAdd(&packetList, sizeof(packetList), packet, timeStamp, buffer.size(), buffer.data());
    MIDIReceived(virtualEndpoint, &packetList);
}

//==============================================================================
bool AudioToMIDIProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioToMIDIProcessor::createEditor()
{
    return new AudioToMIDIProcessorEditor(*this);
}

//==============================================================================
void AudioToMIDIProcessor::getStateInformation(juce::MemoryBlock& destData)
{
}

void AudioToMIDIProcessor::setStateInformation(const void* data, int sizeInBytes)
{
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioToMIDIProcessor();
}
