#include "PluginProcessor.h"
#include "PluginEditor.h"

PalEarsProcessor::PalEarsProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    analyzer.prepare (48000.0);

    socket = std::make_unique<juce::DatagramSocket> (false);
    socket->bindToPort (0, "127.0.0.1");   // any free local port, so the companion can reply

    startTimerHz (10);
}

PalEarsProcessor::~PalEarsProcessor()
{
    stopTimer();
    if (socket != nullptr) socket->shutdown();
}

juce::StringArray PalEarsProcessor::roleNames()
{
    return { "Master", "Mix bus", "Drum bus", "Kick", "Snare / clap", "Hats / perc",
             "808 / bass", "Vocal lead", "Vocal doubles / adlibs", "Vocals bus",
             "Music bus", "Keys / melody", "Beat (2-track)", "Reference track", "Other" };
}

void PalEarsProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    analyzer.prepare (sampleRate);
}

bool PalEarsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PalEarsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pass-through: the audio is never changed.
    analyzer.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;
        }
}

void PalEarsProcessor::timerCallback()
{
    if (resetRequested)
    {
        resetRequested = false;
        stats.resetIntegrated();
    }

    pal::SubBlock sb;
    bool any = false;
    while (analyzer.popSubBlock (sb)) { stats.add (sb, analyzer.getSampleRate()); any = true; }

    if (any)
    {
        snapshot = stats.snapshot();
        ++snapshotCounter;
    }

    // Replies from the companion ("pal-ack") tell us it is listening.
    if (socket != nullptr)
    {
        char buf[64];
        while (socket->waitUntilReady (true, 0) == 1)
        {
            const int n = socket->read (buf, (int) sizeof (buf) - 1, false);
            if (n <= 0) break;
            buf[n] = 0;
            if (juce::String (buf).startsWith (pal::kAckMessage))
                lastAckMs = juce::Time::getMillisecondCounter();
        }
    }

    if (++tick % 5 == 0)   // twice a second
        sendReading();
}

void PalEarsProcessor::sendReading()
{
    if (socket == nullptr) return;

    pal::ReadingHeader h;
    h.instanceId = instanceId.toStdString();
    h.trackName  = getDisplayName().toStdString();
    h.role       = getRole().toStdString();
    h.sampleRate = currentSampleRate.load();
    h.bpm        = hostBpm.load();
    h.playing    = hostPlaying.load();

    const auto json = pal::readingToJson (h, snapshot);
    socket->write ("127.0.0.1", pal::kReadingsPort, json.data(), (int) json.size());
}

bool PalEarsProcessor::isCompanionConnected() const
{
    return lastAckMs != 0 && juce::Time::getMillisecondCounter() - lastAckMs < 3000;
}

juce::String PalEarsProcessor::getDisplayName() const
{
    const juce::ScopedLock sl (nameLock);
    if (customName.isNotEmpty()) return customName;
    if (hostTrackName.isNotEmpty()) return hostTrackName;
    return role;
}

void PalEarsProcessor::updateTrackProperties (const TrackProperties& props)
{
    if (props.name.has_value())
    {
        const juce::ScopedLock sl (nameLock);
        hostTrackName = *props.name;
    }
}

void PalEarsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("PalEars");
    state.setProperty ("role", getRole(), nullptr);
    state.setProperty ("name", getCustomName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PalEarsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            setRole (state.getProperty ("role", "Other").toString());
            setCustomName (state.getProperty ("name", "").toString());
        }
    }
}

juce::AudioProcessorEditor* PalEarsProcessor::createEditor()
{
    return new PalEarsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PalEarsProcessor();
}
