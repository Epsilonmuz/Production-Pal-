#pragma once

#include <juce_core/juce_core.h>

// Reads answers aloud with the system voice (macOS `say`; `espeak` on Linux if installed).
class Speech final
{
public:
    ~Speech() { stop(); }

    void speak (const juce::String& text, int wordsPerMinute = 190, const juce::String& voice = {});
    void stop();
    bool isSpeaking() const;

    static juce::String cleanForSpeech (const juce::String&);

private:
    std::unique_ptr<juce::ChildProcess> proc;
};
