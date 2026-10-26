#pragma once
#include "Common.h"
#include <atomic>
#include <vector>

namespace dc6
{
// The five editable lanes of the 16-step grid. Shared between the audio and GUI threads.
struct SlicerLanes
{
    enum Lane { Pitch = 0, Reverse, Pan, Gate, Start, NumLanes };
    static constexpr int kSteps = 16;
    std::atomic<float> v[NumLanes][kSteps];

    SlicerLanes() { reset(); }
    void reset()
    {
        for (int s = 0; s < kSteps; ++s)
        {
            v[Pitch][s] = 0.0f;  v[Reverse][s] = 0.0f;  v[Pan][s] = 0.0f;
            v[Gate][s]  = 1.0f;  v[Start][s]   = 0.0f;
        }
    }
    static float minOf (int lane) { return lane == Pitch ? -12.0f : (lane == Pan ? -1.0f : (lane == Gate ? 0.05f : 0.0f)); }
    static float maxOf (int lane) { return lane == Pitch ? 12.0f : 1.0f; }
    static float defOf (int lane) { return lane == Gate ? 1.0f : 0.0f; }
};

/*  Live loop slicer.
    It records the incoming audio one loop at a time (locked to the host tempo), finds the hits in it,
    cuts it into 16 or 32 slices and plays the previous loop back rearranged on the step grid.
    The Chaos amount moves it through three zones:
        0-33%   Memphis Pocket        tight, mostly in order, pitched-down tails
        34-66%  Squarepusher          fills on the turnaround, micro-rolls, pitch ramps, off-beat reverses
        67-100% Aphex Drill-Core      slices thrown everywhere, audio-rate retriggers, stretch, velocity spikes
*/
class Slicer
{
public:
    struct Settings
    {
        float mix = 1.0f, detect = 0.5f, decay = 0.3f, chaos = 0.0f;
        int slices = 16, lenIndex = 1, seed = 0;
    };

    void prepare (double sampleRate, SlicerLanes* lanes);
    void reset();
    void process (float* L, float* R, int n, const Clock& clk, const Settings& s);

    // read by the GUI
    std::atomic<int>  guiStep { -1 };
    std::atomic<int>  guiSteps { 16 };
    std::atomic<bool> guiHasLoop { false };
    std::atomic<int>  guiOnsets { 0 };

private:
    struct Plan
    {
        int slice = 0, retrig = 1;
        float ramp = 0.0f, vel = 1.0f, rate = 1.0f, panAdd = 0.0f, gateMul = 1.0f;
        bool flipRev = false;
    };

    void startNewLoop (long long index, const Clock& clk, const Settings& s);
    void analyseOnsets (float detect, int steps);
    void makePlan (long long index, const Settings& s, int steps);

    SlicerLanes* lanes = nullptr;
    double sr = 44100.0;
    int capacity = 0;
    std::vector<float> rec[2], play[2];
    int recLen = 0, playLen = 0, recWritten = 0;
    long long loopIndex = std::numeric_limits<long long>::min();
    bool havePlay = false;

    std::vector<int> onsets;
    int sliceStart[32] {};
    Plan plan[32];
    int planSteps = 16;

    // playback state
    int curStep = -1, curSub = -1;
    double readAcc = 0.0;
    float stepPitch = 0, stepPan = 0, stepGate = 1, stepStart = 0;
    bool stepRev = false;
    float wetGain = 0.0f;   // fades the slicer in once the first loop is captured
    Rng rng;
};
} // namespace dc6
