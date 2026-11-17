// Engine tests for the Pal Ears analyser (no JUCE needed).
// Build: c++ -std=c++17 -O2 -I../Shared AnalyzerTests.cpp -o analyzer_tests
#include "PalAnalyzer.h"
#include "PalProtocol.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

using namespace pal;

static int failures = 0;

static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("%s  %-52s got %8.2f  want %8.2f\n", ok ? "PASS" : "FAIL", what, got, want);
    if (! ok) ++failures;
}

static Snapshot run (double sr, double seconds, const std::function<void (int, double&, double&)>& gen)
{
    PalAnalyzer an; an.prepare (sr);
    PalStats st;
    const int block = 512;
    std::vector<float> L (block), R (block);
    const float* chans[2] = { L.data(), R.data() };
    const long total = (long) (sr * seconds);
    long n = 0;
    while (n < total)
    {
        const int len = (int) std::min<long> (block, total - n);
        for (int i = 0; i < len; ++i) { double l = 0, r = 0; gen ((int) (n + i), l, r); L[(size_t) i] = (float) l; R[(size_t) i] = (float) r; }
        an.process (chans, 2, len);
        SubBlock sb; while (an.popSubBlock (sb)) st.add (sb, sr);
        n += len;
    }
    return st.snapshot();
}

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        // EBU Tech 3341 style: stereo 1 kHz sine at -23 dBFS reads -23 LUFS.
        const double a = std::pow (10.0, -23.0 / 20.0);
        auto s = run (sr, 20.0, [&] (int i, double& l, double& r) { l = r = a * std::sin (2 * kPi * 1000.0 * i / sr); });
        char label[96];
        std::snprintf (label, sizeof (label), "integrated LUFS, 1 kHz -23 dBFS @ %.0f Hz", sr);
        check (std::abs (s.lufsIntegrated + 23.0) < 0.2, label, s.lufsIntegrated, -23.0);
        std::snprintf (label, sizeof (label), "short-term LUFS, 1 kHz -23 dBFS @ %.0f Hz", sr);
        check (std::abs (s.lufsShortTerm + 23.0) < 0.2, label, s.lufsShortTerm, -23.0);
    }

    {   // Gating: 10 s at -20 then 10 s of near-silence should still read about -20.
        const double sr = 48000, a = std::pow (10.0, -20.0 / 20.0);
        auto s = run (sr, 20.0, [&] (int i, double& l, double& r) { l = r = (i < 480000 ? a : 1e-5) * std::sin (2 * kPi * 1000.0 * i / sr); });
        check (std::abs (s.lufsIntegrated + 20.0) < 0.3, "gated integrated ignores silence", s.lufsIntegrated, -20.0);
    }

    {   // Correlation.
        const double sr = 48000;
        auto mono = run (sr, 4.0, [&] (int i, double& l, double& r) { l = r = 0.3 * std::sin (2 * kPi * 220.0 * i / sr); });
        check (mono.correlation > 0.99, "identical channels -> correlation +1", mono.correlation, 1.0);
        auto inv = run (sr, 4.0, [&] (int i, double& l, double& r) { l = 0.3 * std::sin (2 * kPi * 220.0 * i / sr); r = -l; });
        check (inv.correlation < -0.99, "inverted channel -> correlation -1", inv.correlation, -1.0);
        check (inv.lowCorrelation > -1.01 && inv.lowCorrelation < -0.9, "inverted low end flagged", inv.lowCorrelation, -1.0);
    }

    {   // True peak above sample peak for an fs/4 sine at 45 degrees.
        const double sr = 48000;
        auto s = run (sr, 2.0, [&] (int i, double& l, double& r) { l = r = std::sin (2 * kPi * 0.25 * i + kPi / 4); });
        check (s.peakDb < -2.5, "sample peak of fs/4 sine at 45 deg ~ -3 dBFS", s.peakDb, -3.0);
        check (s.truePeakDb > -0.6, "true peak catches inter-sample peak ~ 0 dBTP", s.truePeakDb, 0.0);
    }

    {   // Bands: a 40 Hz sine lives in the sub band.
        const double sr = 48000;
        auto s = run (sr, 4.0, [&] (int i, double& l, double& r) { l = r = 0.5 * std::sin (2 * kPi * 40.0 * i / sr); });
        check (s.bandsDb[0] > -3.0, "40 Hz sine -> sub band dominant", s.bandsDb[0], 0.0);
        check (s.bandsDb[3] < -40.0, "40 Hz sine -> nothing in 2-6 kHz", s.bandsDb[3], -60.0);
        auto t = run (sr, 4.0, [&] (int i, double& l, double& r) { l = r = 0.5 * std::sin (2 * kPi * 3500.0 * i / sr); });
        check (t.bandsDb[3] > -3.0, "3.5 kHz sine -> high-mid band dominant", t.bandsDb[3], 0.0);
    }

    {   // Clip counting and silence.
        const double sr = 48000;
        auto s = run (sr, 1.0, [&] (int i, double& l, double& r) { l = r = 1.5 * std::sin (2 * kPi * 100.0 * i / sr); });
        check (s.clipsTotal > 1000, "overs above 0 dBFS are counted", (double) s.clipsTotal, 1000);
        auto q = run (sr, 1.0, [&] (int, double& l, double& r) { l = r = 0; });
        check (q.silent, "silence is reported as silent", q.silent ? 1 : 0, 1);
    }

    {   // JSON is well-formed enough to round-trip key fields.
        ReadingHeader h; h.instanceId = "abc"; h.trackName = "Vox \"Lead\""; h.role = "vocal";
        Snapshot s; s.lufsShortTerm = -14.2;
        const auto j = readingToJson (h, s);
        check (j.find ("\"lufs_s\":-14.2") != std::string::npos && j.find ("Vox \\\"Lead\\\"") != std::string::npos,
               "JSON contains escaped name and readings", 1, 1);
    }

    {   // Performance: how much of the real-time budget one 512-sample stereo block uses.
        const double sr = 48000; const int block = 512, blocks = 20000;
        PalAnalyzer an; an.prepare (sr);
        std::vector<float> L (block), R (block);
        for (int i = 0; i < block; ++i) { L[(size_t) i] = (float) std::sin (i * 0.01); R[(size_t) i] = (float) std::cos (i * 0.013); }
        const float* ch[2] = { L.data(), R.data() };
        SubBlock sb;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { an.process (ch, 2, block); while (an.popSubBlock (sb)) {} }
        const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count() / blocks;
        const double budget = 1e6 * block / sr;
        std::printf ("PERF  analyser: %.1f us per 512-sample block, budget %.0f us (%.2f%% of real time)\n", us, budget, 100.0 * us / budget);
        check (us < budget * 0.05, "analyser uses under 5% of the real-time budget", 100.0 * us / budget, 5.0);
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
