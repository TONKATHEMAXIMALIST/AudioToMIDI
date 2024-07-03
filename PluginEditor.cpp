#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioToMIDIProcessorEditor::AudioToMIDIProcessorEditor(AudioToMIDIProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Note + Octave display
    noteLabel.setFont(juce::Font(48.0f, juce::Font::bold));
    noteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(&noteLabel);

    // Frequency display
    frequencyLabel.setFont(juce::Font(24.0f));
    frequencyLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(&frequencyLabel);

    setSize(400, 300);
}

AudioToMIDIProcessorEditor::~AudioToMIDIProcessorEditor()
{
}

void AudioToMIDIProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Audio to MIDI", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioToMIDIProcessorEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(40);
    noteLabel.setBounds(area.removeFromTop(100));
    frequencyLabel.setBounds(area.removeFromTop(50));
}

void AudioToMIDIProcessorEditor::setNoteAndFrequencyDisplay(const juce::String& note, float frequency)
{
    noteLabel.setText(note, juce::dontSendNotification);
    frequencyLabel.setText(juce::String(frequency, 2) + " Hz", juce::dontSendNotification);
}
