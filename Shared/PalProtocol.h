// Production Pal — the tiny local protocol between Pal Ears instances, the
// FL Studio script and the companion app. Everything stays on this computer.
//
//  Pal Ears  --UDP JSON-->  127.0.0.1:kReadingsPort  (companion)
//  companion --UDP "pal-ack"-->  back to the sender  (so the plugin can show "connected")
//  FL script --SysEx F0 7D 50 50 <ascii JSON> F7-->  "Production Pal" virtual MIDI port
#pragma once

#include "PalAnalyzer.h"
#include <cstdio>
#include <string>

namespace pal
{
constexpr int kReadingsPort = 47811;
constexpr const char* kAckMessage = "pal-ack";
constexpr int kProtocolVersion = 1;

// SysEx header used by the FL Studio script: 0x7D = non-commercial/educational ID, then "PP".
constexpr unsigned char kSysExId[] = { 0x7D, 0x50, 0x50 };

inline const char* const* bandNames()
{
    static const char* names[] = { "sub_20_60", "bass_60_250", "lowmid_250_2k", "highmid_2k_6k", "air_6k_up" };
    return names;
}

inline std::string jsonEscape (const std::string& in)
{
    std::string out;
    out.reserve (in.size() + 8);
    for (char ch : in)
    {
        const auto c = static_cast<unsigned char> (ch);
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf (buf, sizeof (buf), "\\u%04x", c); out += buf; }
                else out += ch;
        }
    }
    return out;
}

struct ReadingHeader
{
    std::string instanceId, trackName, role;
    double sampleRate = 48000, bpm = 0;
    bool playing = false;
};

inline std::string readingToJson (const ReadingHeader& h, const Snapshot& s)
{
    auto num = [] (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.1f", v); return std::string (b); };
    auto num2 = [] (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.2f", v); return std::string (b); };

    std::string j = "{";
    j += "\"v\":" + std::to_string (kProtocolVersion);
    j += ",\"type\":\"ears\"";
    j += ",\"id\":\"" + jsonEscape (h.instanceId) + "\"";
    j += ",\"track\":\"" + jsonEscape (h.trackName) + "\"";
    j += ",\"role\":\"" + jsonEscape (h.role) + "\"";
    j += ",\"sr\":" + num (h.sampleRate);
    j += ",\"bpm\":" + num (h.bpm);
    j += ",\"playing\":" + std::string (h.playing ? "true" : "false");
    j += ",\"silent\":" + std::string (s.silent ? "true" : "false");
    j += ",\"lufs_m\":" + num (s.lufsMomentary);
    j += ",\"lufs_s\":" + num (s.lufsShortTerm);
    j += ",\"lufs_i\":" + num (s.lufsIntegrated);
    j += ",\"peak_db\":" + num (s.peakDb);
    j += ",\"peak_max_db\":" + num (s.peakMaxDb);
    j += ",\"true_peak_db\":" + num (s.truePeakDb);
    j += ",\"true_peak_max_db\":" + num (s.truePeakMaxDb);
    j += ",\"rms_db\":" + num (s.rmsDb);
    j += ",\"crest_db\":" + num (s.crestDb);
    j += ",\"correlation\":" + num2 (s.correlation);
    j += ",\"low_correlation\":" + num2 (s.lowCorrelation);
    j += ",\"clips_recent\":" + std::to_string (s.clipsRecent);
    j += ",\"clips_total\":" + std::to_string (s.clipsTotal);
    j += ",\"seconds\":" + num (s.secondsMeasured);
    j += ",\"bands_db\":{";
    for (size_t i = 0; i < s.bandsDb.size(); ++i)
    {
        if (i) j += ",";
        j += "\"" + std::string (bandNames()[i]) + "\":" + num (s.bandsDb[i]);
    }
    j += "}}";
    return j;
}

} // namespace pal
