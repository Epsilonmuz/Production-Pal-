#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Shared/PalAnalyzer.h"
#include "../../Shared/PalProtocol.h"

// Pal Ears: a pass-through meter you drop on any FL Studio mixer track (or the
// Master). It measures loudness, peaks, tone balance and stereo health, and
// streams the readings to the Production Pal companion app on this computer.
class PalEarsProcessor final : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    PalEarsProcessor();
    ~PalEarsProcessor() override;

    static juce::StringArray roleNames();

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pal Ears"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void updateTrackProperties (const TrackProperties&) override;

    //==========================================================================
    // Message-thread API used by the editor.
    const pal::Snapshot& getSnapshot() const noexcept { return snapshot; }
    juce::String getDisplayName() const;
    juce::String getRole() const { const juce::ScopedLock sl (nameLock); return role; }
    void setRole (const juce::String& r) { const juce::ScopedLock sl (nameLock); role = r; }
    juce::String getCustomName() const { const juce::ScopedLock sl (nameLock); return customName; }
    void setCustomName (const juce::String& n) { const juce::ScopedLock sl (nameLock); customName = n; }
    juce::String getHostTrackName() const { const juce::ScopedLock sl (nameLock); return hostTrackName; }
    bool isCompanionConnected() const;
    void resetMeasurements() { resetRequested = true; }
    uint32_t getSnapshotCounter() const noexcept { return snapshotCounter; }

private:
    void timerCallback() override;
    void sendReading();

    pal::PalAnalyzer analyzer;
    pal::PalStats stats;
    pal::Snapshot snapshot;
    uint32_t snapshotCounter = 0;

    std::atomic<double> currentSampleRate { 48000.0 }, hostBpm { 0.0 };
    std::atomic<bool> hostPlaying { false };
    bool resetRequested = false;

    juce::String instanceId = juce::Uuid().toDashedString();
    juce::String role = "Other", customName, hostTrackName;   // guarded by nameLock
    mutable juce::CriticalSection nameLock;

    std::unique_ptr<juce::DatagramSocket> socket;
    juce::uint32 lastAckMs = 0;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PalEarsProcessor)
};
