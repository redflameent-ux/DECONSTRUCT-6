#pragma once
#include "Common.h"
#include <juce_dsp/juce_dsp.h>

namespace dc6
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cs = std::cos (w), sn = std::sin (w), al = sn / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * cs + sq;
        set ((A * ((A + 1) - (A - 1) * cs + sq)) / a0, (2 * A * ((A - 1) - (A + 1) * cs)) / a0,
             (A * ((A + 1) - (A - 1) * cs - sq)) / a0, (-2 * ((A - 1) + (A + 1) * cs)) / a0,
             ((A + 1) + (A - 1) * cs - sq) / a0);
    }
    void highPass (double sr, double f)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * 0.7071);
        const double a0 = 1 + al;
        set ((1 + cs) / 2 / a0, -(1 + cs) / a0, (1 + cs) / 2 / a0, -2 * cs / a0, (1 - al) / a0);
    }
    void set (double nb0, double nb1, double nb2, double na1, double na2)
    { b0 = (float) nb0; b1 = (float) nb1; b2 = (float) nb2; a1 = (float) na1; a2 = (float) na2; }
};

/*  Master: smashed vintage-style compressor -> tape saturation -> mono-focused sub boost
    -> level match -> brickwall limiter.                                                       */
class Master
{
public:
    struct Settings { bool enabled = true, limit = true, autoLevel = false; float smash = 0.2f, sat = 0.15f, subBoostDb = 1.5f, ceilingDb = -0.3f; };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        limiter.prepare (spec);
        limiter.setRelease (80.0f);
        sideHp.highPass (sr, 110.0);
        lastBoost = -100.0f;
        reset();
    }

    void reset()
    {
        limiter.reset();
        env = 0; grDb = 0; inE = outE = 1.0e-4f; levelGain = 1.0f;
        shelf.reset(); sideHp.reset();
    }

    // ref: the untouched input (for level matching)
    void process (float* L, float* R, const float* refL, const float* refR, int n, const Settings& s)
    {
        if (! s.enabled) { grDb = 0.0f; return; }

        // --- compressor
        if (s.smash > 0.0f)
        {
            const float thrDb = -4.0f - 26.0f * s.smash;
            const float ratio = 2.0f + 18.0f * s.smash;
            const float att = onePoleCoef (0.006f - 0.0055f * s.smash, sr);
            const float rel = onePoleCoef (0.12f - 0.06f * s.smash, sr);
            const float makeup = dbToGain (-thrDb * (1.0f - 1.0f / ratio) * 0.55f);
            for (int i = 0; i < n; ++i)
            {
                const float peak = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                const float c = peak > env ? att : rel;
                env = c * env + (1.0f - c) * peak;
                const float over = gainToDb (env) - thrDb;
                const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                grDb = gr;
                const float g = dbToGain (-gr) * makeup;
                L[i] *= g; R[i] *= g;
            }
        }
        else grDb = 0.0f;

        // --- tape saturation
        if (s.sat > 0.0f)
        {
            const float d = 1.0f + 4.0f * s.sat, nrm = 1.0f / std::tanh (d);
            for (int i = 0; i < n; ++i)
            {
                L[i] += s.sat * (std::tanh (L[i] * d) * nrm - L[i]);
                R[i] += s.sat * (std::tanh (R[i] * d) * nrm - R[i]);
            }
        }

        // --- mono-focused sub boost: shelf on the middle, low-cut on the sides
        if (std::abs (s.subBoostDb - lastBoost) > 0.01f) { shelf.lowShelf (sr, 70.0, s.subBoostDb); lastBoost = s.subBoostDb; }
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]);
            const float mo = s.subBoostDb > 0.01f ? shelf.process (m) : m;
            const float so = sideHp.process (sd);
            L[i] = mo + so; R[i] = mo - so;
        }

        // --- level match (slow, so it never pumps)
        if (s.autoLevel && refL != nullptr)
        {
            const float k = 1.0f - onePoleCoef (0.8f, sr / (double) juce::jmax (1, n));
            float ei = 0, eo = 0;
            for (int i = 0; i < n; ++i)
            {
                ei += refL[i] * refL[i] + refR[i] * refR[i];
                eo += L[i] * L[i] + R[i] * R[i];
            }
            inE += k * (ei / n - inE);
            outE += k * (eo / n - outE);
            const float target = juce::jlimit (dbToGain (-18.0f), dbToGain (6.0f), std::sqrt ((inE + 1e-9f) / (outE + 1e-9f)));
            const float start = levelGain;
            levelGain += 0.05f * (target - levelGain);
            for (int i = 0; i < n; ++i)
            {
                const float g = start + (levelGain - start) * (float) i / (float) n;
                L[i] *= g; R[i] *= g;
            }
        }
        else levelGain = 1.0f;

        // --- brickwall
        if (s.limit)
        {
            limiter.setThreshold (s.ceilingDb);
            float* chans[2] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
            const float ceil = dbToGain (s.ceilingDb);     // true brickwall after the limiter's release stage
            for (int i = 0; i < n; ++i) { L[i] = juce::jlimit (-ceil, ceil, L[i]); R[i] = juce::jlimit (-ceil, ceil, R[i]); }
        }
    }

    float guiGainReduction() const { return grDb; }

private:
    double sr = 44100.0;
    juce::dsp::Limiter<float> limiter;
    Biquad shelf, sideHp;
    float lastBoost = -100.0f, env = 0, grDb = 0, inE = 1e-4f, outE = 1e-4f, levelGain = 1.0f;
};
} // namespace dc6
