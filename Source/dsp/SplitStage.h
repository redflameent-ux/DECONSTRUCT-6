#pragma once
#include "Common.h"
#include <juce_dsp/juce_dsp.h>

namespace dc6
{
/*  Direction 3 - the "Filth & Sub" split.
    A Linkwitz-Riley crossover (the two bands add back together flat) sends everything below the
    crossover to the LOW path (mono lock + harmonic drive) and everything above to the HIGH path
    (transient snap -> tape/fold drive -> ring mod -> phase smear -> chorus width).
    The high band then carries on into the engines; the low band waits to be added back in.          */
class SplitStage
{
public:
    struct Settings
    {
        bool enabled = true, monoLock = true;
        float xover = 120.0f, harm = 0.0f, lowGain = 1.0f;
        float snap = 0.0f, drive = 0.0f, fold = 0.0f, ringAmt = 0.0f, ringFreq = 110.0f;
        float chorus = 0.0f, smear = 0.0f, highGain = 1.0f;
    };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        xo.prepare (spec);
        xo.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        chorusFx.prepare (spec);
        chorusFx.setCentreDelay (7.0f);
        chorusFx.setFeedback (0.0f);
        fast.set (0.0005f, 0.02f, sr);
        slow.set (0.025f, 0.2f, sr);
        for (auto& d : dc) d.prepare (sr);
        reset();
    }

    void reset()
    {
        xo.reset();
        chorusFx.reset();
        fast.reset(); slow.reset();
        for (auto& d : dc) d.reset();
        for (auto& a : ap) a = {};
        ringPhase = lfoPhase = 0.0f;
    }

    // L/R in: full signal. L/R out: high band. lowL/lowR out: low band.
    void process (float* L, float* R, float* lowL, float* lowR, int n, const Settings& s)
    {
        if (! s.enabled)
        {
            std::fill (lowL, lowL + n, 0.0f);
            std::fill (lowR, lowR + n, 0.0f);
            return;
        }

        xo.setCutoffFrequency (s.xover);
        const float harmGain = 1.0f + 3.0f * s.harm;
        const float driveGain = 1.0f + 10.0f * s.drive * s.drive;
        const float driveNorm = 1.0f / std::tanh (driveGain);
        const float foldGain = 1.0f + 4.0f * s.fold;
        const float ringInc = kTau * s.ringFreq / (float) sr;
        const float lfoInc = kTau * 0.21f / (float) sr;

        for (int i = 0; i < n; ++i)
        {
            float loL, hiL, loR, hiR;
            xo.processSample (0, L[i], loL, hiL);
            xo.processSample (1, R[i], loR, hiR);

            // ---- low band
            if (s.monoLock) { const float m = 0.5f * (loL + loR); loL = loR = m; }
            if (s.harm > 0.0f)
            {
                auto shape = [&] (float x, DcBlock& d)
                {
                    const float xd = x * harmGain;
                    const float y = xd + s.harm * (0.6f * xd * xd + 0.35f * xd * xd * xd);   // 2nd + 3rd harmonics
                    return d.process (std::tanh (y)) / harmGain * (1.0f + s.harm);
                };
                loL = shape (loL, dc[0]);
                loR = s.monoLock ? loL : shape (loR, dc[1]);
            }
            lowL[i] = loL * s.lowGain;
            lowR[i] = loR * s.lowGain;

            // ---- high band: transient snap
            const float mono = 0.5f * (hiL + hiR);
            const float f = fast.process (mono), sl = slow.process (mono);
            const float trans = juce::jlimit (0.0f, 1.0f, (f - sl) / (sl + 1.0e-4f));
            const float snapGain = std::pow (2.0f, 2.0f * s.snap * trans);
            hiL *= snapGain; hiR *= snapGain;

            // drive core: tape overdrive blended into a sine wavefolder
            if (s.drive > 0.0f || s.fold > 0.0f)
            {
                auto drv = [&] (float x)
                {
                    const float tape = std::tanh (x * driveGain) * driveNorm;
                    const float folded = std::sin (tape * foldGain * kPi * 0.5f);
                    return tape + s.fold * (folded - tape);
                };
                hiL = drv (hiL); hiR = drv (hiR);
            }

            // ring modulation
            if (s.ringAmt > 0.0f)
            {
                const float car = std::sin (ringPhase);
                ringPhase += ringInc; if (ringPhase > kTau) ringPhase -= kTau;
                hiL += s.ringAmt * (hiL * car - hiL);
                hiR += s.ringAmt * (hiR * car - hiR);
            }

            // phase smear: four moving allpasses per side, opposite directions
            if (s.smear > 0.0f)
            {
                const float lfo = std::sin (lfoPhase);
                lfoPhase += lfoInc; if (lfoPhase > kTau) lfoPhase -= kTau;
                const float aL = 0.55f + 0.35f * lfo, aR = 0.55f - 0.35f * lfo;
                float yl = hiL, yr = hiR;
                for (int k = 0; k < 4; ++k)
                {
                    yl = ap[k].processL (yl, aL);
                    yr = ap[k].processR (yr, aR);
                }
                hiL += s.smear * 0.7f * (yl - hiL) ;
                hiR += s.smear * 0.7f * (yr - hiR);
            }

            L[i] = hiL;
            R[i] = hiR;
        }

        // chorus + width on the high band only, so the sub stays centred
        if (s.chorus > 0.0f)
        {
            float* chans[2] = { L, R };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            chorusFx.setRate (0.35f + 0.6f * s.chorus);
            chorusFx.setDepth (0.15f + 0.35f * s.chorus);
            chorusFx.setMix (0.5f * s.chorus);
            chorusFx.process (juce::dsp::ProcessContextReplacing<float> (block));

            const float width = 1.0f + 0.8f * s.chorus;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * width;
                L[i] = m + sd; R[i] = m - sd;
            }
        }

        if (s.highGain != 1.0f)
            for (int i = 0; i < n; ++i) { L[i] *= s.highGain; R[i] *= s.highGain; }
    }

private:
    struct Allpass
    {
        float xl = 0, yl = 0, xr = 0, yr = 0;
        float processL (float x, float a) { const float y = a * x + xl - a * yl; xl = x; yl = y; return y; }
        float processR (float x, float a) { const float y = a * x + xr - a * yr; xr = x; yr = y; return y; }
    };

    double sr = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> xo;
    juce::dsp::Chorus<float> chorusFx;
    EnvFollower fast, slow;
    DcBlock dc[2];
    Allpass ap[4];
    float ringPhase = 0.0f, lfoPhase = 0.0f;
};
} // namespace dc6
