#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class AudioToMIDIProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AudioToMIDIProcessorEditor(AudioToMIDIProcessor&);
    ~AudioToMIDIProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    void setNoteAndFrequencyDisplay(const juce::String& note, float frequency);

private:
    AudioToMIDIProcessor& processor;

    juce::Label noteLabel;
    juce::Label frequencyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioToMIDIProcessorEditor)
};
