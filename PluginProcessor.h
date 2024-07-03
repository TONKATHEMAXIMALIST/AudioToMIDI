#pragma once

#include <JuceHeader.h>
#include <vector>
#include <CoreMIDI/CoreMIDI.h>

//==============================================================================
class AudioToMIDIProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioToMIDIProcessor();
    ~AudioToMIDIProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#if JucePlugin_IsSynth
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
#else
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
#endif

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Pitch detection and MIDI generation
    void detectPitch(const float* audioData, int numSamples);
    void convertPitchToMIDI(float detectedPitch);
    void updateNoteDisplay(float frequency);

    // Virtual MIDI port setup
    void setupVirtualMIDIPort();
    void sendMIDIMessage(const juce::MidiMessage& message);

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioToMIDIProcessor)

    double sampleRate;
    std::vector<float> audioBuffer;
    juce::MidiBuffer midiBuffer;
    float detectedPitch;
    float previousPitch; // To store previous pitch for smoothing
    bool noteOn; // To track note state

    // Helper functions
    float detectPitchUsingYIN(const std::vector<float>& buffer);
    float getPitchConfidence(const std::vector<float>& buffer, float pitch);

    // CoreMIDI properties
    MIDIClientRef midiClient;
    MIDIPortRef midiOutPort;
    MIDIEndpointRef virtualEndpoint;
};
