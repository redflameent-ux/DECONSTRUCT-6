#pragma once
#include "Common.h"
#include <vector>

namespace dc6
{
// ============================================================================ Granular scrambler
// Keeps the last couple of seconds, finds the hits, and sprays short (often reversed) grains of
// them across the stereo field.
class Scrambler
{
public:
    struct Settings { float mix = 0, sizeMs = 70, density = 14, reverse = 0.4f, spread = 0.7f; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.5);
        hist[0].assign ((size_t) size, 0.0f);
        hist[1].assign ((size_t) size, 0.0f);
        fast.set (0.0008f, 0.02f, sr);
        slow.set (0.03f, 0.25f, sr);
        reset();
    }

    void reset()
    {
        std::fill (hist[0].begin(), hist[0].end(), 0.0f);
        std::fill (hist[1].begin(), hist[1].end(), 0.0f);
        total = 0; nextGrain = 0.0; numOnsets = 0; onsetW = 0; refractory = 0;
        for (auto& g : grains) g.active = false;
        fast.reset(); slow.reset();
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        const bool running = s.mix > 0.001f;
        const int len = juce::jmax (32, (int) (s.sizeMs * 0.001 * sr));
        const float overlap = juce::jmax (1.0f, s.density * s.sizeMs * 0.001f);
        const float norm = 1.3f / std::sqrt (overlap);

        for (int i = 0; i < n; ++i)
        {
            const int w = (int) (total % size);
            hist[0][(size_t) w] = L[i];
            hist[1][(size_t) w] = R[i];

            // track hits so grains can start on them
            const float m = 0.5f * (L[i] + R[i]);
            const float f = fast.process (m), sl = slow.process (m);
            if (refractory > 0) --refractory;
            else if (f > 0.01f && f > sl * 2.0f)
            {
                onsets[onsetW] = total;
                onsetW = (onsetW + 1) % kOnsets;
                numOnsets = juce::jmin (numOnsets + 1, kOnsets);
                refractory = (int) (0.06 * sr);
            }
            ++total;

            if (! running) continue;

            nextGrain -= 1.0;
            if (nextGrain <= 0.0)
            {
                spawn (len, s);
                nextGrain += sr / juce::jmax (0.5f, s.density) * (0.5 + rng.uni());
            }

            float wl = 0.0f, wr = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float ph = (float) g.age / (float) g.len;
                const float win = std::sin (kPi * ph);
                const double p = g.reverse ? g.start + g.len - g.age : g.start + g.age;
                const double idx = std::fmod ((double) p, (double) size);
                const float vl = readLinear (hist[0].data(), size, idx) * win * win;
                const float vr = readLinear (hist[1].data(), size, idx) * win * win;
                const float mono = 0.5f * (vl + vr);
                wl += mono * g.gl + vl * (1.0f - g.spread) * 0.5f;
                wr += mono * g.gr + vr * (1.0f - g.spread) * 0.5f;
                if (++g.age >= g.len) g.active = false;
            }
            L[i] += s.mix * (wl * norm - L[i]);
            R[i] += s.mix * (wr * norm - R[i]);
        }
    }

private:
    static constexpr int kGrains = 24, kOnsets = 16;
    struct Grain { bool active = false, reverse = false; long long start = 0; int len = 0, age = 0; float gl = 1, gr = 1, spread = 0; };

    void spawn (int len, const Settings& s)
    {
        Grain* slot = nullptr;
        for (auto& g : grains) if (! g.active) { slot = &g; break; }
        if (slot == nullptr) return;

        long long start;
        if (numOnsets > 0 && rng.chance (0.7f))
            start = onsets[(size_t) rng.range (numOnsets)];
        else
            start = total - (long long) (rng.uni() * sr * 1.2) - len;
        start = juce::jmin (start, total - (long long) len - 2);
        start = juce::jmax (start, total - (long long) size + 4);
        if (start < 0) return;

        slot->active = true;
        slot->start = start;
        slot->len = len;
        slot->age = 0;
        slot->reverse = rng.chance (s.reverse);
        const float pan = rng.bi() * s.spread;
        slot->gl = std::cos ((pan + 1.0f) * kPi * 0.25f) * 1.414f * s.spread;
        slot->gr = std::sin ((pan + 1.0f) * kPi * 0.25f) * 1.414f * s.spread;
        slot->spread = s.spread;
    }

    double sr = 44100.0;
    int size = 1;
    std::vector<float> hist[2];
    long long total = 0;
    double nextGrain = 0.0;
    Grain grains[kGrains];
    long long onsets[kOnsets] {};
    int numOnsets = 0, onsetW = 0, refractory = 0;
    EnvFollower fast, slow;
    Rng rng;
};

// ============================================================================ Micro-repeats
// Freezes the last slice of audio and loops it: 1/32, 1/64, 1/128 rolls, a pitch-rising burst,
// or a reversed repeat. Played from the pads, MIDI keys, or fired by the chaos engine.
class Stutter
{
public:
    enum Mode { Off = 0, Rep32, Rep64, Rep128, Riser, Reverse, Rep16 };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 4.0);
        hist[0].assign ((size_t) size, 0.0f);
        hist[1].assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (hist[0].begin(), hist[0].end(), 0.0f);
        std::fill (hist[1].begin(), hist[1].end(), 0.0f);
        w = 0; mode = Off; mix = 0.0f; autoLeft = 0; autoMode = Off;
    }

    // a one-off repeat fired by the chaos engine
    void fireAuto (Mode m, int durationSamples) { autoMode = m; autoLeft = durationSamples; }

    void process (float* L, float* R, int n, const Clock& clk, Mode padMode, float riseAmt)
    {
        const double spb = clk.samplesPerBeat();
        const float fadeStep = 1.0f / (float) (0.004 * sr);

        for (int i = 0; i < n; ++i)
        {
            hist[0][(size_t) w] = L[i];
            hist[1][(size_t) w] = R[i];

            Mode want = padMode;
            if (want == Off && autoLeft > 0) want = autoMode;
            if (autoLeft > 0) --autoLeft;

            if (want != Off && want != mode)
            {
                // capture: loop the most recent slice
                mode = want;
                loopLen = juce::jlimit (32.0, size * 0.5, beatsFor (mode) * spb);
                loopStart = (double) w - loopLen;
                pos = 0.0; held = 0.0;
            }
            else if (want == Off && mix <= 0.0f)
                mode = Off;

            mix = juce::jlimit (0.0f, 1.0f, mix + (want != Off ? fadeStep : -fadeStep));

            if (mode != Off && mix > 0.0f)
            {
                double cur = loopLen, rate = 1.0;
                if (mode == Riser)
                {
                    const double beatsHeld = held / spb;
                    cur = juce::jmax (loopLen / 8.0, loopLen * std::pow (2.0, -beatsHeld * 1.5));
                    rate = std::pow (2.0, juce::jmin (2.0, beatsHeld * 2.0 * riseAmt));
                }
                while (pos >= cur) pos -= cur;
                const double rp = mode == Reverse ? loopStart + cur - pos : loopStart + pos;
                const float fl = (float) juce::jmin (64.0, cur * 0.25);
                const float taper = juce::jmin (1.0f, juce::jmin ((float) pos / fl, (float) (cur - pos) / fl));
                const float sl = readLinear (hist[0].data(), size, rp) * taper;
                const float sr_ = readLinear (hist[1].data(), size, rp) * taper;
                L[i] += mix * (sl - L[i]);
                R[i] += mix * (sr_ - R[i]);
                pos += rate;
                held += 1.0;
            }
            if (++w >= size) w = 0;
        }
    }

    Mode current() const { return mix > 0.0f ? mode : Off; }

    static double beatsFor (Mode m)
    {
        switch (m)
        {
            case Rep32: return 0.125;  case Rep64: return 0.0625;  case Rep128: return 0.03125;
            case Riser: return 0.25;   case Reverse: return 0.25;  case Rep16: return 0.25;
            default: return 0.25;
        }
    }

private:
    double sr = 44100.0;
    int size = 1, w = 0;
    std::vector<float> hist[2];
    Mode mode = Off, autoMode = Off;
    int autoLeft = 0;
    double loopStart = 0, loopLen = 1, pos = 0, held = 0;
    float mix = 0.0f;
};
} // namespace dc6
