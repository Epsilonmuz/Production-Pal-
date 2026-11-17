#include "ReadingsHub.h"
#include "../../Shared/PalProtocol.h"

static constexpr juce::uint32 kStaleMs = 4000;

ReadingsHub::ReadingsHub() : juce::Thread ("Pal readings")
{
    listening = socket.bindToPort (pal::kReadingsPort, "127.0.0.1");

   #if JUCE_MAC || JUCE_LINUX
    // Virtual ports appear in FL Studio's MIDI settings as "Production Pal".
    midiIn  = juce::MidiInput::createNewDevice ("Production Pal", this);
    midiOut = juce::MidiOutput::createNewDevice ("Production Pal");
    if (midiIn != nullptr) midiIn->start();
   #endif

    startThread();
}

ReadingsHub::~ReadingsHub()
{
    signalThreadShouldExit();
    socket.shutdown();
    stopThread (2000);
    if (midiIn != nullptr) midiIn->stop();
}

void ReadingsHub::run()
{
    if (! listening) return;

    juce::HeapBlock<char> buf (65536);
    while (! threadShouldExit())
    {
        if (socket.waitUntilReady (true, 250) != 1) continue;

        juce::String senderIp; int senderPort = 0;
        const int n = socket.read (buf.get(), 65535, false, senderIp, senderPort);
        if (n <= 0) continue;
        buf[n] = 0;

        const auto parsed = juce::JSON::parse (juce::String::fromUTF8 (buf.get(), n));
        if (! parsed.isObject() || parsed["type"].toString() != "ears") continue;

        {
            const juce::ScopedLock sl (lock);
            auto& r = ears[parsed["id"].toString()];
            r.data = parsed;
            r.receivedMs = juce::Time::getMillisecondCounter();
        }

        // Let the plugin know we're here.
        socket.write (senderIp, senderPort, pal::kAckMessage, (int) std::strlen (pal::kAckMessage));
        ++changeCounter;
    }
}

void ReadingsHub::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! m.isSysEx()) return;
    const auto* d = m.getSysExData();
    const int size = m.getSysExDataSize();
    if (size < 4 || d[0] != pal::kSysExId[0] || d[1] != pal::kSysExId[1] || d[2] != pal::kSysExId[2]) return;

    const auto json = juce::String (juce::CharPointer_ASCII ((const char*) d + 3), (size_t) (size - 3));
    const auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject()) return;

    {
        const juce::ScopedLock sl (lock);
        flState = parsed;
        flReceivedMs = juce::Time::getMillisecondCounter();
    }
    ++changeCounter;
}

std::vector<juce::var> ReadingsHub::getLiveEars() const
{
    std::vector<juce::var> out;
    const auto now = juce::Time::getMillisecondCounter();
    {
        const juce::ScopedLock sl (lock);
        for (const auto& [id, r] : ears)
            if (now - r.receivedMs < kStaleMs) out.push_back (r.data);
    }
    auto rank = [] (const juce::var& v)
    {
        const auto role = v["role"].toString();
        if (role == "Master") return 0;
        if (role.endsWith ("bus")) return 1;
        return 2;
    };
    std::stable_sort (out.begin(), out.end(), [&] (const juce::var& a, const juce::var& b) { return rank (a) < rank (b); });
    return out;
}

juce::var ReadingsHub::getFlState() const
{
    const juce::ScopedLock sl (lock);
    return isFlConnected() ? flState : juce::var();
}

bool ReadingsHub::isFlConnected() const
{
    const juce::ScopedLock sl (lock);
    return flReceivedMs != 0 && juce::Time::getMillisecondCounter() - flReceivedMs < kStaleMs;
}

juce::String ReadingsHub::buildContextForAI() const
{
    juce::String s;
    const auto live = getLiveEars();
    if (live.empty())
        s << "Pal Ears meters: none running right now.\n";
    else
    {
        s << "Pal Ears meters (live, last 3 s unless noted; lufs_i and *_max since the meter was reset):\n";
        for (const auto& v : live)
            s << juce::JSON::toString (v, true) << "\n";
    }

    const auto fl = getFlState();
    if (fl.isObject())
        s << "FL Studio state (from the Production Pal FL script):\n" << juce::JSON::toString (fl, true) << "\n";
    else
        s << "FL Studio script: not connected.\n";
    return s;
}
