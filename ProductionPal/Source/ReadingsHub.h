#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <map>

// Collects everything Production Pal can "hear" and "see" without a screenshot:
//  - readings from every Pal Ears plugin instance (UDP on localhost)
//  - FL Studio's mixer / transport state from the FL script (SysEx on a virtual MIDI port)
class ReadingsHub final : private juce::Thread,
                          private juce::MidiInputCallback
{
public:
    ReadingsHub();
    ~ReadingsHub() override;

    struct EarsReading
    {
        juce::var data;
        juce::uint32 receivedMs = 0;
    };

    bool isListening() const noexcept { return listening; }
    bool hasVirtualMidi() const noexcept { return midiIn != nullptr; }

    // Instances heard from in the last few seconds, sorted Master first.
    std::vector<juce::var> getLiveEars() const;
    juce::var getFlState() const;      // void if the FL script hasn't reported recently
    bool isFlConnected() const;

    // Compact text block for the AI: one JSON line per meter + FL state.
    juce::String buildContextForAI() const;

    // Bumped whenever new data arrives; the UI polls it on a timer.
    juce::uint32 getChangeCounter() const noexcept { return changeCounter.load(); }

private:
    void run() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    juce::DatagramSocket socket { false };
    bool listening = false;

    std::unique_ptr<juce::MidiInput> midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;   // gives FL an input port to attach the script to

    mutable juce::CriticalSection lock;
    std::map<juce::String, EarsReading> ears;
    juce::var flState;
    juce::uint32 flReceivedMs = 0;
    std::atomic<juce::uint32> changeCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReadingsHub)
};
