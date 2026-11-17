#include "Speech.h"

juce::String Speech::cleanForSpeech (const juce::String& in)
{
    auto s = in.replace ("**", "").replace ("__", "").replace ("`", "").replace ("#", "");
    s = s.replace ("->", " to ").replace ("→", " to ");
    s = s.replace (" dBFS", " dee bee full scale").replace (" dBTP", " dee bee true peak");
    s = s.replace (" LUFS", " loo-fs");
    return s;
}

void Speech::speak (const juce::String& text, int wordsPerMinute, const juce::String& voice)
{
    stop();
    const auto clean = cleanForSpeech (text);
    if (clean.trim().isEmpty()) return;

    juce::StringArray args;
   #if JUCE_MAC
    args.add ("/usr/bin/say");
    args.add ("-r"); args.add (juce::String (wordsPerMinute));
    if (voice.isNotEmpty()) { args.add ("-v"); args.add (voice); }
    args.add (clean);
   #elif JUCE_LINUX
    juce::ignoreUnused (voice);
    args.add ("espeak");
    args.add ("-s"); args.add (juce::String (wordsPerMinute));
    args.add (clean);
   #else
    juce::ignoreUnused (wordsPerMinute, voice);
    return;
   #endif

    proc = std::make_unique<juce::ChildProcess>();
    if (! proc->start (args, 0))
        proc.reset();
}

void Speech::stop()
{
    if (proc != nullptr && proc->isRunning())
        proc->kill();
    proc.reset();
}

bool Speech::isSpeaking() const
{
    return proc != nullptr && proc->isRunning();
}
