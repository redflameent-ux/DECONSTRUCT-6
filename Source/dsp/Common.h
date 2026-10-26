#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <array>
#include <algorithm>

namespace dc6
{
constexpr float kPi  = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

// Musical time for one chunk of audio. ppq = position in quarter notes.
struct Clock
{
    double ppq0 = 0.0;          // ppq at the first sample of the chunk
    double ppqPerSample = 0.0;
    double bpm = 120.0;
    double beatsPerBar = 4.0;   // in quarter notes
    double sampleRate = 44100.0;

    double ppqAt (int n) const noexcept           { return ppq0 + ppqPerSample * n; }
    double samplesPerBeat() const noexcept         { return sampleRate * 60.0 / bpm; }
};

// small fast RNG (xorshift) that is safe to use on the audio thread
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t x) noexcept     { s = x ? x : 0x9e3779b9u; next(); next(); }
    uint32_t next() noexcept            { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept                { return (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi() noexcept                 { return uni() * 2.0f - 1.0f; }                     // -1..1
    bool chance (float p) noexcept      { return uni() < p; }
    int range (int n) noexcept          { return n <= 0 ? 0 : (int) (next() % (uint32_t) n); }
};

inline float dbToGain (float db) noexcept     { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept      { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp01 (float x) noexcept       { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float softClip (float x) noexcept      { return std::tanh (x); }

// one-pole coefficient for a time constant in seconds
inline float onePoleCoef (float seconds, double sr) noexcept
{
    return seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (seconds * sr));
}

// envelope follower with separate attack / release
struct EnvFollower
{
    float att = 0.0f, rel = 0.0f, y = 0.0f;
    void set (float attackSec, float releaseSec, double sr) noexcept
    { att = onePoleCoef (attackSec, sr); rel = onePoleCoef (releaseSec, sr); }
    float process (float x) noexcept
    {
        x = std::abs (x);
        const float c = x > y ? att : rel;
        y = c * y + (1.0f - c) * x;
        return y;
    }
    void reset() noexcept { y = 0.0f; }
};

// DC blocker
struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare (double sr) noexcept { r = (float) std::exp (-kTau * 8.0 / sr); }
    float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

// simple one-pole lowpass
struct OnePoleLP
{
    float a = 0.0f, y = 0.0f;
    void setCutoff (float hz, double sr) noexcept { a = (float) std::exp (-kTau * hz / sr); }
    float process (float x) noexcept { y = (1.0f - a) * x + a * y; return y; }
    void reset() noexcept { y = 0.0f; }
};

// linear interpolated read from a circular buffer
inline float readLinear (const float* buf, int size, double pos) noexcept
{
    while (pos < 0.0) pos += size;
    while (pos >= size) pos -= size;
    const int i0 = (int) pos;
    const int i1 = (i0 + 1) % size;
    const float f = (float) (pos - i0);
    return buf[i0] + f * (buf[i1] - buf[i0]);
}

// Euclidean rhythm: is step i a hit?
inline bool euclidHit (int i, int steps, int pulses, int rotate) noexcept
{
    if (steps <= 0 || pulses <= 0) return false;
    pulses = std::min (pulses, steps);
    const int k = ((i + rotate) % steps + steps) % steps;
    return ((k * pulses) % steps) < pulses;
}
} // namespace dc6
