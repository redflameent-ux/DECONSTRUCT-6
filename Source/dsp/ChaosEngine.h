#pragma once
#include "Common.h"
#include <atomic>

namespace dc6
{
/*  Chaos engine: a non-linear sample-and-hold LFO (logistic map mixed with noise) and a Euclidean
    step sequencer, both locked to tempo. The processor reads their outputs once per small chunk and
    uses them to push drive, filter cutoff, scramble amount and to fire auto stutters.              */
class ChaosEngine
{
public:
    struct Settings { int rate = 2; float glide = 0.2f, depth = 0.25f; int steps = 16, pulses = 5, rotate = 0; };
    struct Tick { bool newStep = false; int step = 0; bool hit = false; int offset = 0; };

    void reset() { value = target = 0.0f; x = 0.37f; lastDiv = std::numeric_limits<long long>::min(); lastStep = lastDiv; }

    Tick advance (const Clock& clk, int n, const Settings& s)
    {
        const double div = s.rate == 0 ? 1.0 : (s.rate == 1 ? 0.5 : (s.rate == 2 ? 0.25 : 0.125));
        const long long d = (long long) std::floor (clk.ppq0 / div);
        if (d != lastDiv)
        {
            lastDiv = d;
            const float r = 3.57f + 0.42f * s.depth;
            x = r * x * (1.0f - x);
            if (x <= 0.0001f || x >= 0.9999f) x = 0.37f + 0.2f * rng.uni();
            target = 0.5f * (x * 2.0f - 1.0f) + 0.5f * rng.bi();
        }
        const double divSamples = div * clk.samplesPerBeat();
        const float k = 1.0f - (float) std::exp (-(double) n / (s.glide * s.glide * divSamples + 1.0));
        value += (target - value) * k;

        Tick t;
        const double endPpq = clk.ppqAt (n);
        const long long st = (long long) std::floor (endPpq * 4.0);
        if (st != lastStep)
        {
            const bool contiguous = st == lastStep + 1;
            lastStep = st;
            if (contiguous)
            {
                t.newStep = true;
                t.step = (int) (((st % 64) + 64) % 64);
                t.hit = euclidHit (t.step, s.steps, s.pulses, s.rotate);
                const double boundary = (double) st / 4.0;
                t.offset = juce::jlimit (0, n - 1, (int) ((boundary - clk.ppq0) / juce::jmax (1.0e-12, clk.ppqPerSample)));
            }
        }
        guiValue = value;
        if (t.newStep) { guiStep = t.step % juce::jmax (1, s.steps); guiHit = t.hit; }
        return t;
    }

    float output() const { return value; }       // -1..1
    float uni() { return rng.uni(); }
    int pick (int n) { return rng.range (n); }

    std::atomic<float> guiValue { 0.0f };
    std::atomic<int> guiStep { 0 };
    std::atomic<bool> guiHit { false };

private:
    float value = 0, target = 0, x = 0.37f;
    long long lastDiv = std::numeric_limits<long long>::min(), lastStep = std::numeric_limits<long long>::min();
    Rng rng;
};
} // namespace dc6
