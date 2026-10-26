#include "Slicer.h"

namespace dc6
{
static constexpr double kMaxLoopSeconds = 12.0;   // 2 bars at 40 BPM

void Slicer::prepare (double sampleRate, SlicerLanes* l)
{
    sr = sampleRate;
    lanes = l;
    capacity = (int) std::ceil (kMaxLoopSeconds * sr) + 8;
    for (int c = 0; c < 2; ++c)
    {
        rec[c].assign ((size_t) capacity, 0.0f);
        play[c].assign ((size_t) capacity, 0.0f);
    }
    onsets.reserve (512);
    reset();
}

void Slicer::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        std::fill (rec[c].begin(), rec[c].end(), 0.0f);
        std::fill (play[c].begin(), play[c].end(), 0.0f);
    }
    recLen = playLen = recWritten = 0;
    loopIndex = std::numeric_limits<long long>::min();
    havePlay = false;
    wetGain = 0.0f;
    curStep = curSub = -1;
    readAcc = 0.0;
    for (int i = 0; i < 32; ++i) { sliceStart[i] = 0; plan[i] = Plan(); plan[i].slice = i; }
    guiStep = -1;
    guiHasLoop = false;
}

static double loopBeatsFor (int lenIndex, double beatsPerBar)
{
    const double mult = lenIndex == 0 ? 0.5 : (lenIndex == 2 ? 2.0 : 1.0);
    return beatsPerBar * mult;
}

void Slicer::startNewLoop (long long index, const Clock& clk, const Settings& s)
{
    const bool contiguous = (index == loopIndex + 1);
    const bool complete = recLen > 0 && recWritten >= (int) (recLen * 0.9);

    if (contiguous && complete)
    {
        std::swap (rec[0], play[0]);
        std::swap (rec[1], play[1]);
        playLen = recLen;
        havePlay = true;
        analyseOnsets (s.detect, s.slices);
    }

    loopIndex = index;
    const double beats = loopBeatsFor (s.lenIndex, clk.beatsPerBar);
    recLen = juce::jlimit (64, capacity - 4, (int) std::llround (beats * clk.samplesPerBeat()));
    recWritten = 0;
    std::fill (rec[0].begin(), rec[0].begin() + recLen + 4, 0.0f);
    std::fill (rec[1].begin(), rec[1].begin() + recLen + 4, 0.0f);

    makePlan (index, s, s.slices);
    curStep = curSub = -1;
    guiHasLoop = havePlay;
}

void Slicer::analyseOnsets (float detect, int steps)
{
    onsets.clear();
    const int hop = 128;
    const int hops = playLen / hop;
    float hist[4] = { 1e-9f, 1e-9f, 1e-9f, 1e-9f };
    int lastOnset = -1000000;
    const int minGap = std::max (hop, playLen / 64);

    for (int h = 0; h < hops; ++h)
    {
        float e = 0.0f;
        const int base = h * hop;
        for (int i = 0; i < hop; ++i)
        {
            const float m = 0.5f * (play[0][(size_t) (base + i)] + play[1][(size_t) (base + i)]);
            e += m * m;
        }
        e /= (float) hop;
        const float avg = 0.25f * (hist[0] + hist[1] + hist[2] + hist[3]);
        const float rise = 10.0f * std::log10 ((e + 1e-10f) / (avg + 1e-10f));
        if (rise > 6.0f && e > 1.0e-5f && base - lastOnset >= minGap && onsets.size() < 500)
        {
            onsets.push_back (std::max (0, base - 32));
            lastOnset = base;
        }
        hist[h & 3] = e;
    }
    guiOnsets = (int) onsets.size();

    // nominal grid positions, pulled onto the nearest hit when one is close enough
    const double sliceLen = (double) playLen / steps;
    const double window = detect * sliceLen * 0.5;
    for (int k = 0; k < steps; ++k)
    {
        const double nominal = k * sliceLen;
        double best = nominal, bestDist = window + 1.0;
        for (int o : onsets)
        {
            const double d = std::abs (o - nominal);
            if (d < bestDist) { bestDist = d; best = o; }
        }
        sliceStart[k] = (int) (bestDist <= window ? best : nominal);
    }
}

void Slicer::makePlan (long long index, const Settings& s, int steps)
{
    planSteps = steps;
    rng.seed ((uint32_t) (s.seed * 7919 + (int) (index & 0x7fffffff) * 104729 + 1));
    const float c = s.chaos;
    static const int rollsLo[] = { 2, 3, 4, 6 };
    static const int rollsHi[] = { 2, 3, 4, 6, 8, 12, 16, 24, 32 };

    for (int st = 0; st < steps; ++st)
    {
        Plan p;
        p.slice = st;

        if (c <= 0.33f)
        {
            // Memphis Pocket: stay on the grid, the odd swap between neighbouring 8ths
            const float z = c / 0.33f;
            if ((st % 4) != 0 && rng.chance (0.12f * z))
                p.slice = (st + (rng.chance (0.5f) ? 2 : steps - 2)) % steps;
        }
        else if (c <= 0.66f)
        {
            // Squarepusher complexity: fills on the turnaround, micro-rolls, ramps, reverses
            const float z = (c - 0.33f) / 0.33f;
            if ((st % 4) != 0 && rng.chance (0.12f))
                p.slice = (st + (rng.chance (0.5f) ? 2 : steps - 2)) % steps;

            const bool turnaround = (index & 1) == 1 && st >= steps - steps / 4;
            if (turnaround && rng.chance (0.5f + 0.5f * z))
            {
                p.slice  = rng.range (steps);
                p.retrig = rollsLo[rng.range (4)];
                p.ramp   = rng.bi() * (4.0f + 8.0f * z);
            }
            else if (rng.chance (0.1f * z))
                p.retrig = rng.chance (0.5f) ? 2 : 4;

            if ((st & 1) && rng.chance (0.35f * z)) p.flipRev = true;
            if (rng.chance (0.15f * z)) p.ramp = rng.bi() * 5.0f;
        }
        else
        {
            // Aphex drill-core: full fragmentation
            const float z = juce::jmin (1.0f, (c - 0.66f) / 0.34f);
            if (rng.chance (0.35f + 0.65f * z)) p.slice = rng.range (steps);
            if (rng.chance (0.25f + 0.5f * z)) p.retrig = rollsHi[rng.range (3 + (int) (6.0f * z))];
            if (rng.chance (0.3f + 0.2f * z)) p.flipRev = true;
            if (rng.chance (0.25f * z)) p.rate = std::pow (2.0f, rng.bi());
            p.vel = rng.chance (0.3f) ? 1.2f + 0.6f * rng.uni() : 0.6f + 0.4f * rng.uni();
            if (rng.chance (0.3f)) p.ramp = rng.bi() * 12.0f;
            if (rng.chance (0.4f * z)) p.gateMul = 0.2f + 0.8f * rng.uni();
            if (rng.chance (0.4f * z)) p.panAdd = rng.bi();
        }
        plan[st] = p;
    }
}

void Slicer::process (float* L, float* R, int n, const Clock& clk, const Settings& s)
{
    const double loopBeats = loopBeatsFor (s.lenIndex, clk.beatsPerBar);
    const float fadeInLen = 48.0f;
    const float wetStep = 1.0f / 2048.0f;

    for (int i = 0; i < n; ++i)
    {
        const double pos = clk.ppqAt (i) / loopBeats;
        const long long li = (long long) std::floor (pos);
        if (li != loopIndex)
            startNewLoop (li, clk, s);
        const double ph = pos - (double) li;

        const float inL = L[i], inR = R[i];

        // record this loop
        const int w = (int) (ph * recLen);
        if (w >= 0 && w < recLen)
        {
            rec[0][(size_t) w] = inL;
            rec[1][(size_t) w] = inR;
            ++recWritten;
        }

        wetGain = clamp01 (wetGain + (havePlay ? wetStep : -wetStep));
        if (! havePlay && wetGain <= 0.0f)
            continue;

        const int steps = planSteps;
        const double stepPos = ph * steps;
        const int st = juce::jlimit (0, steps - 1, (int) stepPos);
        const double stepLen = (double) recLen / steps;
        const double o = (stepPos - st) * stepLen;
        const Plan& p = plan[st];

        if (st != curStep)
        {
            curStep = st;
            curSub = -1;
            const int lane = st % SlicerLanes::kSteps;
            stepPitch = lanes->v[SlicerLanes::Pitch][lane].load (std::memory_order_relaxed);
            stepRev   = lanes->v[SlicerLanes::Reverse][lane].load (std::memory_order_relaxed) > 0.5f;
            stepPan   = lanes->v[SlicerLanes::Pan][lane].load (std::memory_order_relaxed);
            stepGate  = lanes->v[SlicerLanes::Gate][lane].load (std::memory_order_relaxed);
            stepStart = lanes->v[SlicerLanes::Start][lane].load (std::memory_order_relaxed);
            guiStep = st;
            guiSteps = steps;
        }

        const double subLen = stepLen / p.retrig;
        const int sub = (int) (o / subLen);
        const double local = o - sub * subLen;
        if (sub != curSub) { curSub = sub; readAcc = 0.0; }

        // pitch: lane pitch + chaos ramp + Memphis tail drop
        const double frac = o / stepLen;
        const double tail = local / stepLen;
        const double semis = stepPitch + p.ramp * frac - s.decay * 7.0 * tail * std::sqrt (tail);
        const double rate = p.rate * std::pow (2.0, semis / 12.0);

        // source position inside the captured loop
        const double sliceLen = (double) playLen / steps;
        const bool rev = stepRev != p.flipRev;
        const double startAt = sliceStart[p.slice % steps] + stepStart * sliceLen;
        const double srcLen = juce::jmin (subLen, sliceLen) * rate;
        double src = rev ? startAt + srcLen - readAcc : startAt + readAcc;
        if (playLen > 0)
        {
            src = std::fmod (src, (double) playLen);
            if (src < 0) src += playLen;
        }
        readAcc += rate;

        // envelope: short fade in, gate, short fade out
        const float gateLen = (float) juce::jmax (64.0, subLen * juce::jlimit (0.05f, 1.0f, stepGate * p.gateMul));
        const float fadeOutLen = (float) juce::jmin (96.0, subLen * 0.15);
        float env = juce::jmin (1.0f, (float) local / fadeInLen);
        env *= juce::jlimit (0.0f, 1.0f, (gateLen - (float) local) / fadeOutLen);
        env *= p.vel;

        float wl = 0.0f, wr = 0.0f;
        if (env > 0.0f && playLen > 0)
        {
            wl = readLinear (play[0].data(), playLen, src) * env;
            wr = readLinear (play[1].data(), playLen, src) * env;
        }

        const float pan = juce::jlimit (-1.0f, 1.0f, stepPan + p.panAdd);
        wl *= pan > 0.0f ? 1.0f - pan : 1.0f;
        wr *= pan < 0.0f ? 1.0f + pan : 1.0f;

        const float m = s.mix * wetGain;
        L[i] = inL * (1.0f - m) + wl * m;
        R[i] = inR * (1.0f - m) + wr * m;
    }
}
} // namespace dc6
