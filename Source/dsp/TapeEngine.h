#pragma once
#include "Common.h"
#include <vector>

namespace dc6
{
// ============================================================================ Engine A tone
// Preamp saturation -> 12-bit style crush -> sample-rate reduction -> cassette rolloff + hiss.
class TapeTone
{
public:
    struct Settings { float preamp = 0, bits = 16, srate = 44100, hiss = 0, mix = 1; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : lp) l.reset();
        holdPhase = 1.0f;
        heldL = heldR = 0.0f;
        pink = {};
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        const bool crushRate = s.srate < 43000.0f;
        const bool crushBits = s.bits < 15.9f;
        const float preGain = 1.0f + 6.0f * s.preamp;
        const float preNorm = 1.0f / std::tanh (preGain);
        const float levels = std::pow (2.0f, s.bits - 1.0f);
        const float holdInc = (float) (s.srate / sr);
        const float roll = crushRate ? juce::jmin (s.srate * 0.45f, 18000.0f) : 16000.0f;
        lp[0].setCutoff (roll, sr); lp[1].setCutoff (roll, sr);
        const float hissAmt = s.hiss * 0.02f;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (s.preamp > 0.0f) { l = std::tanh (l * preGain) * preNorm; r = std::tanh (r * preGain) * preNorm; }

            if (crushRate)
            {
                holdPhase += holdInc;
                if (holdPhase >= 1.0f) { holdPhase -= 1.0f; heldL = l; heldR = r; }
                l = heldL; r = heldR;
            }
            if (crushBits)
            {
                l = std::round (l * levels) / levels;
                r = std::round (r * levels) / levels;
            }
            if (crushRate || s.preamp > 0.2f) { l = lp[0].process (l); r = lp[1].process (r); }
            if (hissAmt > 0.0f) { l += hissAmt * pinkNoise(); r += hissAmt * pinkNoise(); }

            L[i] += s.mix * (l - L[i]);
            R[i] += s.mix * (r - R[i]);
        }
    }

private:
    float pinkNoise()
    {
        // Paul Kellet's economy pink filter
        const float w = rng.bi();
        pink.b0 = 0.99765f * pink.b0 + w * 0.0990460f;
        pink.b1 = 0.96300f * pink.b1 + w * 0.2965164f;
        pink.b2 = 0.57000f * pink.b2 + w * 1.0526913f;
        return (pink.b0 + pink.b1 + pink.b2 + w * 0.1848f) * 0.25f;
    }

    double sr = 44100.0;
    OnePoleLP lp[2];
    float holdPhase = 1.0f, heldL = 0, heldR = 0;
    struct { float b0 = 0, b1 = 0, b2 = 0; } pink;
    Rng rng;
};

// ============================================================================ Sub-harmonic resynth
// Listens for kicks in the low end and fires a tuned, mono sine underneath each one.
class SubResynth
{
public:
    struct Settings { bool on = false; float level = 0.5f, tune = 45.0f, decay = 0.4f, sens = 0.5f, drop = 0.3f; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detLp1.setCutoff (120.0f, sr); detLp2.setCutoff (120.0f, sr);   // listen strictly below 120 Hz
        fast.set (0.001f, 0.03f, sr);
        slow.set (0.04f, 0.25f, sr);
        reset();
    }
    void reset() { detLp1.reset(); detLp2.reset(); fast.reset(); slow.reset(); amp = 0; phase = 0; refractory = 0; glide = 0; }

    // detectL/R: the signal to listen to. outL/R: sub gets added here.
    void process (const float* detL, const float* detR, float* outL, float* outR, int n, const Settings& s)
    {
        if (! s.on) { amp = 0.0f; return; }
        const float decayCoef = onePoleCoef (s.decay * 0.35f, sr);
        const float glideCoef = onePoleCoef (0.035f, sr);
        const float thresh = 0.6f - 0.5f * s.sens;        // how far above the slow envelope a kick must jump
        const float floorLvl = dbToGain (-48.0f + 18.0f * (1.0f - s.sens));
        const int refractorySamples = (int) (0.07 * sr);

        for (int i = 0; i < n; ++i)
        {
            const float x = detLp2.process (detLp1.process (0.5f * (detL[i] + detR[i])));
            const float f = fast.process (x), sl = slow.process (x);
            if (refractory > 0) --refractory;
            else if (f > floorLvl && (f - sl) > thresh * (sl + 1.0e-3f) + floorLvl * 0.5f)
            {
                amp = 1.0f;
                glide = 1.0f;
                phase = 0.0f;
                refractory = refractorySamples;
            }
            if (amp < 1.0e-5f) continue;

            const float semis = 12.0f * s.drop * glide;
            const float freq = s.tune * std::pow (2.0f, semis / 12.0f);
            glide *= glideCoef;
            phase += kTau * freq / (float) sr;
            if (phase > kTau) phase -= kTau;
            const float y = std::sin (phase) * amp * s.level * 0.7f;
            amp *= decayCoef;
            outL[i] += y;
            outR[i] += y;
        }
    }

    float guiLastAmp() const { return amp; }

private:
    double sr = 44100.0;
    OnePoleLP detLp1, detLp2;
    EnvFollower fast, slow;
    float amp = 0, phase = 0, glide = 0;
    int refractory = 0;
};

// ============================================================================ Tape transport
// One delay line with two read heads. Handles wow & flutter, the tape stop wind-down and the
// half-speed drag. Runs on the full mix so the sub stops along with everything else.
// It always sits a fixed few milliseconds behind live (reported to the host as latency).
class TapeTransport
{
public:
    struct Settings { bool enabled = true; bool stop = false, drag = false; float flutter = 0, stopTime = 0.7f, stopCurve = 0.4f; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 10.0);
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        baseDelay = latencySamples (sr);
        reset();
    }

    static int latencySamples (double sampleRate) { return (int) std::round (sampleRate * 0.004); }

    void reset()
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0;
        mode = Live;
        head = (double) -baseDelay;
        oldHead = 0.0; xfade = 0.0f;
        rate = 1.0; stopT = 0.0;
        wowPhase = flutPhase = 0.0f; drift = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        const float depth = s.enabled ? s.flutter : 0.0f;
        const bool wantStop = s.enabled && s.stop;
        const bool wantDrag = s.enabled && s.drag && ! wantStop;
        const float xfInc = 1.0f / (float) (0.025 * sr);

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) w] = L[i];
            buf[1][(size_t) w] = R[i];

            // --- transport state machine
            if (wantStop && mode != Stopping) { mode = Stopping; stopT = 0.0; }
            else if (wantDrag && mode != Dragging && mode != Stopping) { mode = Dragging; }
            else if (! wantStop && ! wantDrag && mode != Live) { backToLive(); }

            if (mode == Stopping)
            {
                stopT += 1.0 / sr;
                const double t = juce::jmin (1.0, stopT / juce::jmax (0.02f, s.stopTime));
                rate = std::pow (1.0 - t, 1.0 + 3.0 * s.stopCurve);
            }
            else if (mode == Dragging)
                rate += (0.5 - rate) * 0.0015;
            else
                rate = 1.0;

            // --- wow & flutter (only moves the read head while live)
            wowPhase  += kTau * 0.55f / (float) sr;  if (wowPhase  > kTau) wowPhase  -= kTau;
            flutPhase += kTau * 6.3f  / (float) sr;  if (flutPhase > kTau) flutPhase -= kTau;
            drift += (rng.bi() - drift) * 0.0002f;
            const float mod = depth * (0.0025f * std::sin (wowPhase) + 0.00025f * std::sin (flutPhase) + 0.0008f * drift);
            const double modSamples = (double) mod * sr;    // max about 2.5 ms either side of base delay

            double readPos;
            if (mode == Live)
                readPos = (double) w - baseDelay - (double) juce::jmax (0.0, modSamples + 0.003 * sr * depth) ;
            else
            {
                head += rate;
                if (head >= size) head -= size;
                // a long drag can run out of tape: jump back to live
                double behind = (double) w - head;
                if (behind < 0.0) behind += size;
                if (behind > size - 2 * sr) backToLive();
                readPos = mode == Live ? (double) w - baseDelay : head;
            }

            float gain = 1.0f;
            if (mode == Stopping) gain = (float) std::sqrt (juce::jmax (0.0, rate));

            float l = readLinear (buf[0].data(), size, readPos) * gain;
            float r = readLinear (buf[1].data(), size, readPos) * gain;

            if (xfade > 0.0f)
            {
                oldHead += oldRate;
                if (oldHead >= size) oldHead -= size;
                const float ol = readLinear (buf[0].data(), size, oldHead) * oldGain;
                const float orr = readLinear (buf[1].data(), size, oldHead) * oldGain;
                l = l * (1.0f - xfade) + ol * xfade;
                r = r * (1.0f - xfade) + orr * xfade;
                xfade -= xfInc;
            }

            L[i] = l; R[i] = r;
            if (mode == Live) head = readPos < 0.0 ? readPos + size : readPos;   // park the head at live
            if (++w >= size) w = 0;
        }
    }

    bool isStopped() const { return mode == Stopping; }

private:
    enum Mode { Live, Stopping, Dragging };

    void backToLive()
    {
        // crossfade from where the tape head is now back to the live signal
        oldHead = head;
        oldRate = mode == Live ? 1.0 : rate;
        oldGain = mode == Stopping ? (float) std::sqrt (juce::jmax (0.0, rate)) : 1.0f;
        xfade = 1.0f;
        mode = Live;
        rate = 1.0;
    }

    double sr = 44100.0;
    int size = 1, w = 0, baseDelay = 0;
    std::vector<float> buf[2];
    Mode mode = Live;
    double head = 0.0, oldHead = 0.0, oldRate = 1.0, rate = 1.0, stopT = 0.0;
    float oldGain = 1.0f, xfade = 0.0f;
    float wowPhase = 0, flutPhase = 0, drift = 0;
    Rng rng;
};
} // namespace dc6
