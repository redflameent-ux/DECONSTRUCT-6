#pragma once
#include "Common.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace dc6
{
// ============================================================================ Engine B tone
// Multi-stage clipping/folding -> screaming comb -> self-oscillating ladder low-pass.
class IndustrialTone
{
public:
    struct Settings
    {
        float drive = 0.3f, fold = 0, asym = 0.1f, combFreq = 180, combFb = 0.6f, combMix = 0;
        float cutoff = 20000, reso = 0.15f, mix = 1;
        int clipMode = 2;
    };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        ladder.prepare (spec);
        ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
        combSize = (int) (sr / 30.0) + 4;
        comb[0].assign ((size_t) combSize, 0.0f);
        comb[1].assign ((size_t) combSize, 0.0f);
        for (auto& d : dc) d.prepare (sr);
        dry.setSize (2, maxBlock);
        reset();
    }

    void reset()
    {
        ladder.reset();
        std::fill (comb[0].begin(), comb[0].end(), 0.0f);
        std::fill (comb[1].begin(), comb[1].end(), 0.0f);
        cw = 0; damp[0] = damp[1] = 0.0f;
        for (auto& d : dc) d.reset();
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        dry.copyFrom (0, 0, L, n);
        dry.copyFrom (1, 0, R, n);

        const float pre = 1.0f + 30.0f * s.drive * s.drive;
        const float comp = 1.0f / (1.0f + 1.6f * s.drive);
        const float bias = 0.35f * s.asym;
        const float foldGain = 1.0f + 5.0f * s.fold;
        const double combDelay = juce::jlimit (2.0, (double) combSize - 3.0, sr / s.combFreq);

        auto shape = [&] (float v, int c)
        {
            // stage 1: console overdrive, with asymmetric bias
            v = std::tanh (v * pre * 0.5f + bias) - std::tanh (bias);
            // stage 2: fold
            if (s.fold > 0.0f) v += s.fold * (std::sin (v * foldGain * kPi * 0.5f) - v);
            // stage 3: clip character
            switch (s.clipMode)
            {
                case 0:  v = juce::jlimit (-0.7f, 0.7f, v * 1.6f) / 0.7f; break;                  // hard diode
                case 1:  v = v > 0.0f ? std::tanh (v * 2.2f) : std::tanh (v * 0.7f) * 1.3f; break; // asymmetric
                case 2:  v = std::tanh (v * 1.4f); break;                                           // console
                default: v = std::sin (v * (1.5f + 3.0f * s.drive) * kPi * 0.5f); break;            // wavefold
            }
            return dc[c].process (v) * (0.6f + 0.4f * comp);
        };

        auto combStep = [&] (float v, int c)
        {
            const float fb = readLinear (comb[c].data(), combSize, (double) cw - combDelay);
            damp[c] += 0.6f * (fb - damp[c]);
            const float in = v + s.combFb * std::tanh (damp[c] * 1.2f);
            comb[c][(size_t) cw] = in;
            return v + s.combMix * (in * 0.7f - v);
        };

        const bool doShape = s.drive > 0.0f || s.fold > 0.0f || s.clipMode == 3;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (doShape) { l = shape (l, 0); r = shape (r, 1); }
            if (s.combMix > 0.0f)
            {
                l = combStep (l, 0); r = combStep (r, 1);
                if (++cw >= combSize) cw = 0;
            }
            L[i] = l; R[i] = r;
        }

        // self-oscillating ladder low-pass
        if (s.cutoff < 19500.0f || s.reso > 0.05f)
        {
            ladder.setCutoffFrequencyHz (juce::jlimit (30.0f, (float) (sr * 0.45), s.cutoff));
            ladder.setResonance (juce::jlimit (0.0f, 1.0f, s.reso));
            ladder.setDrive (1.0f + 1.5f * s.reso);
            float* chans[2] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            ladder.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        if (s.mix < 1.0f)
        {
            const float* dl = dry.getReadPointer (0);
            const float* dr = dry.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                L[i] = dl[i] + s.mix * (L[i] - dl[i]);
                R[i] = dr[i] + s.mix * (R[i] - dr[i]);
            }
        }
    }

private:
    double sr = 44100.0;
    juce::dsp::LadderFilter<float> ladder;
    std::vector<float> comb[2];
    int combSize = 1, cw = 0;
    float damp[2] {};
    DcBlock dc[2];
    juce::AudioBuffer<float> dry;
};

// ============================================================================ Pumping gate
// Tempo-locked chopper with a threshold, keyed by the signal itself or the sidechain input.
class PumpGate
{
public:
    struct Settings
    {
        bool on = false, useSidechain = false;
        int rate = 2, pattern = 0;
        float threshDb = -60, depth = 1, duty = 0.5f;
        int euSteps = 16, euPulses = 5, euRot = 0;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        key.set (0.0005f, 0.03f, sr);
        keySlow.set (0.03f, 0.3f, sr);
        attack = onePoleCoef (0.0012f, sr);
        release = onePoleCoef (0.006f, sr);
        g = 1.0f;
    }
    void reset() { key.reset(); keySlow.reset(); g = 1.0f; hold = 0; }

    static double divisionBeats (int rate)
    {
        switch (rate)
        {
            case 0: return 1.0;  case 1: return 0.5;  case 2: return 0.25;  case 3: return 0.125;
            case 4: return 1.0 / 3.0;  default: return 1.0 / 6.0;
        }
    }

    void process (float* L, float* R, const float* scL, const float* scR, int n, const Clock& clk, const Settings& s)
    {
        if (! s.on) { g = 1.0f; return; }
        const double div = divisionBeats (s.rate);
        const float thr = dbToGain (s.threshDb);
        const float closed = 1.0f - s.depth;
        holdLen = juce::jmax (64, (int) (s.duty * div * clk.samplesPerBeat()));

        for (int i = 0; i < n; ++i)
        {
            const double ppq = clk.ppqAt (i);
            const double pos = ppq / div;
            const long long idx = (long long) std::floor (pos);
            const double ph = pos - (double) idx;

            const float k = (s.useSidechain && scL != nullptr)
                              ? key.process (0.5f * (scL[i] + scR[i]))
                              : key.process (0.5f * (L[i] + R[i]));
            const float ks = keySlow.process (k);

            bool open;
            switch (s.pattern)
            {
                case 0:   // transient: each hit opens the gate for a slice of the division, then it slams shut
                    if (hold > 0) --hold;
                    if (k > thr && k > ks * 1.6f && hold < holdLen / 2) hold = holdLen;
                    open = hold > 0;
                    break;
                case 1:  open = ph < s.duty; break;
                case 2:  open = ph >= (1.0 - s.duty); break;
                default:
                {
                    const long long sixteenth = (long long) std::floor (ppq * 4.0);
                    const double ph16 = ppq * 4.0 - (double) sixteenth;
                    open = euclidHit ((int) (sixteenth % 64), s.euSteps, s.euPulses, s.euRot) && ph16 < s.duty * 2.0;
                }
            }

            if (k < thr) open = false;

            const float target = open ? 1.0f : closed;
            const float c = target > g ? attack : release;
            g = c * g + (1.0f - c) * target;
            L[i] *= g; R[i] *= g;
        }
    }

    float guiGain() const { return g; }

private:
    double sr = 44100.0;
    EnvFollower key, keySlow;
    int hold = 0, holdLen = 64;
    float attack = 0, release = 0, g = 1.0f;
};
} // namespace dc6
