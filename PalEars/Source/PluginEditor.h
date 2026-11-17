#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PalEarsEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit PalEarsEditor (PalEarsProcessor&);
    ~PalEarsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawStat (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                   const juce::String& value, juce::Colour valueColour);

    PalEarsProcessor& proc;
    juce::ComboBox roleBox;
    juce::TextEditor nameEditor;
    juce::TextButton resetButton { "Reset" };
    juce::Label roleLabel { {}, "Role" }, nameLabel { {}, "Name" };

    uint32_t lastCounter = 0;
    bool lastConnected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PalEarsEditor)
};
