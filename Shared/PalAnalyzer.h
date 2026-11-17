// Production Pal — shared analysis engine (no JUCE dependency, header-only).
//
// PalAnalyzer runs on the AUDIO thread: it only filters and accumulates, never
// allocates after prepare(), and hands finished 100 ms sub-blocks to a lock-free
// FIFO. PalStats runs on the MESSAGE thread: it pops those sub-blocks and does
// the heavier work (ITU-R BS.1770 gating, rolling windows) where allocation is fine.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pal
{

constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0; }

    static Biquad lowPass (double f, double q, double sr)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        Biquad bq;
        bq.b0 = (1.0 - c) / 2.0 / a0;  bq.b1 = (1.0 - c) / a0;  bq.b2 = bq.b0;
        bq.a1 = -2.0 * c / a0;         bq.a2 = (1.0 - al) / a0;
        return bq;
    }

    static Biquad highPass (double f, double q, double sr)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        Biquad bq;
        bq.b0 = (1.0 + c) / 2.0 / a0;  bq.b1 = -(1.0 + c) / a0; bq.b2 = bq.b0;
        bq.a1 = -2.0 * c / a0;         bq.a2 = (1.0 - al) / a0;
        return bq;
    }

    // ITU-R BS.1770 K-weighting, stage 1 (high shelf) and stage 2 (RLB high-pass),
    // re-derived for any sample rate (same formulas as libebur128).
    static Biquad kShelf (double sr)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / sr);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad bq;
        bq.b0 = (Vh + Vb * K / Q + K * K) / a0;
        bq.b1 = 2.0 * (K * K - Vh) / a0;
        bq.b2 = (Vh - Vb * K / Q + K * K) / a0;
        bq.a1 = 2.0 * (K * K - 1.0) / a0;
        bq.a2 = (1.0 - K / Q + K * K) / a0;
        return bq;
    }

    static Biquad kHighPass (double sr)
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / sr);
        const double d = 1.0 + K / Q + K * K;
        Biquad bq;
        bq.b0 = 1.0; bq.b1 = -2.0; bq.b2 = 1.0;
        bq.a1 = 2.0 * (K * K - 1.0) / d;
        bq.a2 = (1.0 - K / Q + K * K) / d;
        return bq;
    }
};

//==============================================================================
// 4x polyphase interpolator for an approximate true-peak (dBTP) reading.
class TruePeak4x
{
public:
    static constexpr int kPhases = 4, kTaps = 12;

    TruePeak4x()
    {
        constexpr int N = kPhases * kTaps;
        const double centre = (N - 1) / 2.0;
        for (int p = 0; p < kPhases; ++p)
        {
            double sum = 0;
            for (int j = 0; j < kTaps; ++j)
            {
                const int k = j * kPhases + p;
                const double t = (k - centre) / kPhases;
                const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double win = 0.5 - 0.5 * std::cos (2.0 * kPi * (k + 0.5) / N);
                coeffs[(size_t) p][(size_t) j] = sinc * win;
                sum += coeffs[(size_t) p][(size_t) j];
            }
            for (auto& c : coeffs[(size_t) p]) c /= sum;
        }
        reset();
    }

    void reset() noexcept { hist.fill (0.0); pos = 0; }

    // Returns the largest absolute interpolated value around this sample.
    inline double push (double x) noexcept
    {
        hist[(size_t) pos] = x;
        double best = std::abs (x);
        for (int p = 0; p < kPhases; ++p)
        {
            double acc = 0;
            int idx = pos;
            for (int j = 0; j < kTaps; ++j)
            {
                acc += coeffs[(size_t) p][(size_t) j] * hist[(size_t) idx];
                idx = (idx == 0 ? kTaps - 1 : idx - 1);
            }
            best = std::max (best, std::abs (acc));
        }
        pos = (pos + 1) % kTaps;
        return best;
    }

private:
    std::array<std::array<double, kTaps>, kPhases> coeffs {};
    std::array<double, kTaps> hist {};
    int pos = 0;
};

//==============================================================================
// Everything the audio thread measured during one 100 ms slice.
struct SubBlock
{
    static constexpr int kBands = 5;          // sub, bass, low-mid, high-mid, air

    double kPower = 0;                        // K-weighted mean square, summed over channels
    double peak = 0;                          // sample peak (linear)
    double truePeak = 0;                      // 4x interpolated peak (linear)
    double sumL2 = 0, sumR2 = 0, sumLR = 0;   // for stereo correlation
    double lowL2 = 0, lowR2 = 0, lowLR = 0;   // the same below ~120 Hz (mono check)
    double monoPower = 0;                     // mean square of (L+R)/2
    std::array<double, kBands> band {};       // mean square per band of (L+R)/2
    uint32_t clips = 0;                       // samples above 0 dBFS
    uint32_t samples = 0;
};

//==============================================================================
// Single-producer single-consumer ring buffer (audio thread -> message thread).
template <typename T, int Capacity>
class SpscFifo
{
public:
    bool push (const T& item) noexcept
    {
        const int w = writeIdx.load (std::memory_order_relaxed);
        const int next = (w + 1) % Capacity;
        if (next == readIdx.load (std::memory_order_acquire)) return false; // full: drop
        buffer[(size_t) w] = item;
        writeIdx.store (next, std::memory_order_release);
        return true;
    }

    bool pop (T& out) noexcept
    {
        const int r = readIdx.load (std::memory_order_relaxed);
        if (r == writeIdx.load (std::memory_order_acquire)) return false;
        out = buffer[(size_t) r];
        readIdx.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    void clear() noexcept { readIdx.store (writeIdx.load()); }

private:
    std::array<T, Capacity> buffer {};
    std::atomic<int> writeIdx { 0 }, readIdx { 0 };
};

//==============================================================================
class PalAnalyzer
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0 ? newSampleRate : 48000.0;
        subBlockLength = std::max (1, (int) std::lround (sampleRate * 0.1));

        for (int ch = 0; ch < 2; ++ch)
        {
            kShelf[ch] = Biquad::kShelf (sampleRate);
            kHp[ch]    = Biquad::kHighPass (sampleRate);
            low[ch]    = Biquad::lowPass (120.0, 0.7071, sampleRate);
            tp[ch].reset();
        }

        const double sr = sampleRate;
        const double nyq = sr * 0.45;
        bandA = { Biquad::lowPass  (60.0, 0.7071, sr),  Biquad::highPass (60.0, 0.7071, sr),
                  Biquad::highPass (250.0, 0.7071, sr), Biquad::highPass (2000.0, 0.7071, sr),
                  Biquad::highPass (std::min (6000.0, nyq), 0.7071, sr) };
        bandB = { Biquad::lowPass  (60.0, 0.7071, sr),  Biquad::lowPass  (250.0, 0.7071, sr),
                  Biquad::lowPass  (2000.0, 0.7071, sr), Biquad::lowPass (std::min (6000.0, nyq), 0.7071, sr),
                  Biquad::highPass (std::min (6000.0, nyq), 0.7071, sr) };
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) { kShelf[ch].reset(); kHp[ch].reset(); low[ch].reset(); tp[ch].reset(); }
        for (auto& b : bandA) b.reset();
        for (auto& b : bandB) b.reset();
        current = {};
        counter = 0;
    }

    // Real-time safe. Mono input is analysed as dual-mono.
    void process (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || channels == nullptr) return;
        const float* L = channels[0];
        const float* R = numChannels > 1 ? channels[1] : channels[0];
        const bool stereo = numChannels > 1;

        for (int i = 0; i < numSamples; ++i)
        {
            const double l = L[i], r = R[i];

            const double kl = kHp[0].process (kShelf[0].process (l));
            const double kr = stereo ? kHp[1].process (kShelf[1].process (r)) : kl;
            current.kPower += kl * kl + kr * kr;

            const double al = std::abs (l), ar = std::abs (r);
            current.peak = std::max (current.peak, std::max (al, ar));
            if (al > 1.0) ++current.clips;
            if (stereo && ar > 1.0) ++current.clips;

            current.truePeak = std::max (current.truePeak, tp[0].push (l));
            if (stereo) current.truePeak = std::max (current.truePeak, tp[1].push (r));

            current.sumL2 += l * l; current.sumR2 += r * r; current.sumLR += l * r;

            const double ll = low[0].process (l);
            const double lr = stereo ? low[1].process (r) : ll;
            current.lowL2 += ll * ll; current.lowR2 += lr * lr; current.lowLR += ll * lr;

            const double m = 0.5 * (l + r);
            current.monoPower += m * m;
            for (size_t b = 0; b < (size_t) SubBlock::kBands; ++b)
            {
                const double y = bandB[b].process (bandA[b].process (m));
                current.band[b] += y * y;
            }

            if (++counter >= subBlockLength) finishSubBlock();
        }
    }

    bool popSubBlock (SubBlock& out) noexcept { return fifo.pop (out); }
    double getSampleRate() const noexcept { return sampleRate; }

private:
    void finishSubBlock() noexcept
    {
        const double n = (double) counter;
        current.samples = (uint32_t) counter;
        current.kPower /= n;          // per-channel mean squares summed (G = 1 for L/R)
        current.monoPower /= n;
        for (auto& b : current.band) b /= n;
        fifo.push (current);
        current = {};
        counter = 0;
    }

    double sampleRate = 48000.0;
    int subBlockLength = 4800, counter = 0;
    Biquad kShelf[2], kHp[2], low[2];
    TruePeak4x tp[2];
    std::array<Biquad, SubBlock::kBands> bandA {}, bandB {};
    SubBlock current;
    SpscFifo<SubBlock, 512> fifo;
};

//==============================================================================
struct Snapshot
{
    double lufsMomentary = -100, lufsShortTerm = -100, lufsIntegrated = -100;
    double peakDb = -100, peakMaxDb = -100;           // last 3 s / since reset
    double truePeakDb = -100, truePeakMaxDb = -100;   // last 3 s / since reset
    double rmsDb = -100, crestDb = 0;                 // last 3 s
    double correlation = 1, lowCorrelation = 1;       // last 3 s, +1 = mono, 0 = wide, <0 = phase trouble
    std::array<double, SubBlock::kBands> bandsDb {};  // band level relative to the full signal (dB)
    uint32_t clipsRecent = 0, clipsTotal = 0;
    double secondsMeasured = 0;
    bool silent = true;
};

inline double toDb (double linear, double floorDb = -100.0)
{
    return linear > 0 ? std::max (floorDb, 20.0 * std::log10 (linear)) : floorDb;
}

inline double powerToLufs (double power)
{
    return power > 0 ? std::max (-100.0, -0.691 + 10.0 * std::log10 (power)) : -100.0;
}

// Message-thread aggregator. Feed it every sub-block the analyzer produced.
class PalStats
{
public:
    static constexpr int kShortWindow = 30;    // 3 s of 100 ms sub-blocks
    static constexpr size_t kMaxBlocks = 3 * 60 * 60 * 10; // 3 hours of gating blocks

    void resetIntegrated()
    {
        blockPowers.clear();
        peakMax = truePeakMax = 0;
        clipsTotal = 0;
        totalSamples = 0;
    }

    void add (const SubBlock& sb, double sampleRate)
    {
        recent.push_back (sb);
        if ((int) recent.size() > kShortWindow) recent.erase (recent.begin());

        peakMax = std::max (peakMax, sb.peak);
        truePeakMax = std::max (truePeakMax, sb.truePeak);
        clipsTotal += sb.clips;
        totalSamples += sb.samples;
        rate = sampleRate;

        // 400 ms gating blocks with 75 % overlap = mean of the last four sub-blocks.
        if (recent.size() >= 4 && blockPowers.size() < kMaxBlocks)
            blockPowers.push_back (meanKPower (4));
    }

    Snapshot snapshot() const
    {
        Snapshot s;
        if (recent.empty()) return s;

        s.lufsMomentary = powerToLufs (meanKPower (4));
        s.lufsShortTerm = powerToLufs (meanKPower (kShortWindow));
        s.lufsIntegrated = integrated();

        double pk = 0, tpk = 0, sL2 = 0, sR2 = 0, sLR = 0, lL2 = 0, lR2 = 0, lLR = 0, mono = 0, chPow = 0;
        std::array<double, SubBlock::kBands> bands {};
        uint32_t clips = 0;
        for (const auto& b : recent)
        {
            pk = std::max (pk, b.peak); tpk = std::max (tpk, b.truePeak);
            sL2 += b.sumL2; sR2 += b.sumR2; sLR += b.sumLR;
            lL2 += b.lowL2; lR2 += b.lowR2; lLR += b.lowLR;
            mono += b.monoPower;
            chPow += (b.sumL2 + b.sumR2) / std::max (1.0, 2.0 * b.samples);
            for (size_t i = 0; i < bands.size(); ++i) bands[i] += b.band[i];
            clips += b.clips;
        }
        const double n = (double) recent.size();

        s.peakDb = toDb (pk);
        s.truePeakDb = toDb (tpk);
        s.peakMaxDb = toDb (peakMax);
        s.truePeakMaxDb = toDb (truePeakMax);
        s.rmsDb = toDb (std::sqrt (chPow / n));
        s.crestDb = s.rmsDb > -99 ? s.peakDb - s.rmsDb : 0.0;
        s.silent = s.rmsDb < -70.0;

        const double den = std::sqrt (sL2 * sR2);
        s.correlation = den > 1e-12 ? sLR / den : 1.0;
        const double lden = std::sqrt (lL2 * lR2);
        s.lowCorrelation = lden > 1e-12 ? lLR / lden : 1.0;

        for (size_t i = 0; i < bands.size(); ++i)
            s.bandsDb[i] = mono > 0 && bands[i] > 0 ? std::max (-60.0, 10.0 * std::log10 (bands[i] / mono)) : -60.0;

        s.clipsRecent = clips;
        s.clipsTotal = clipsTotal;
        s.secondsMeasured = rate > 0 ? (double) totalSamples / rate : 0.0;
        return s;
    }

    // ITU-R BS.1770-4 integrated loudness: absolute gate -70 LUFS, relative gate -10 LU.
    double integrated() const
    {
        const double absGate = std::pow (10.0, (-70.0 + 0.691) / 10.0);
        double sum = 0; size_t count = 0;
        for (double p : blockPowers) if (p > absGate) { sum += p; ++count; }
        if (count == 0) return -100.0;

        const double relGate = (sum / (double) count) * std::pow (10.0, -10.0 / 10.0);
        double sum2 = 0; size_t count2 = 0;
        for (double p : blockPowers) if (p > absGate && p > relGate) { sum2 += p; ++count2; }
        return count2 ? powerToLufs (sum2 / (double) count2) : -100.0;
    }

private:
    double meanKPower (int lastN) const
    {
        const int n = std::min<int> (lastN, (int) recent.size());
        if (n <= 0) return 0;
        double s = 0;
        for (int i = (int) recent.size() - n; i < (int) recent.size(); ++i) s += recent[(size_t) i].kPower;
        return s / n;
    }

    std::vector<SubBlock> recent;
    std::vector<double> blockPowers;
    double peakMax = 0, truePeakMax = 0, rate = 48000.0;
    uint32_t clipsTotal = 0;
    uint64_t totalSamples = 0;
};

} // namespace pal
