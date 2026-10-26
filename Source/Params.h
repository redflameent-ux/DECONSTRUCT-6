#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Every automatable control in DECONSTRUCT-6, in signal-flow order.
namespace pid
{
    // ---- front panel macros + global
    inline constexpr const char* chaos   = "chaos";    // ANARCHY: slicer zones, saturation, stutter odds, tape drift
    inline constexpr const char* grime   = "grime";    // GRIME / 12-BIT: one knob for clock rate, bit depth and hiss
    inline constexpr const char* chMod   = "chMod";    // CHAOS MOD: S&H routed to cutoff, drive and buffer length
    inline constexpr const char* stutRate= "stutRate"; // STUTTER RATE for chaos/auto repeats
    inline constexpr const char* latch   = "latch";    // pads latch instead of momentary
    inline constexpr const char* mix     = "mix";      // 0..1   global dry/wet
    inline constexpr const char* inGain  = "inGain";
    inline constexpr const char* outGain = "outGain";

    // ---- module power switches
    inline constexpr const char* slOn    = "slOn";
    inline constexpr const char* spOn    = "spOn";
    inline constexpr const char* aOn     = "aOn";
    inline constexpr const char* bOn     = "bOn";
    inline constexpr const char* cOn     = "cOn";
    inline constexpr const char* chOn    = "chOn";
    inline constexpr const char* mOn     = "mOn";

    // ---- Slicer (Direction 2)
    inline constexpr const char* slMix    = "slMix";
    inline constexpr const char* slSlices = "slSlices";  // 16 / 32
    inline constexpr const char* slLen    = "slLen";     // 1/2 bar, 1 bar, 2 bars
    inline constexpr const char* slDetect = "slDetect";  // transient snap
    inline constexpr const char* slDecay  = "slDecay";   // Memphis pitch-down tails
    inline constexpr const char* slSeed   = "slSeed";

    // ---- Band split (Direction 3)
    inline constexpr const char* xover    = "xover";
    inline constexpr const char* monoLock = "monoLock";
    inline constexpr const char* harm     = "harm";
    inline constexpr const char* lowLvl   = "lowLvl";
    inline constexpr const char* snap     = "snap";
    inline constexpr const char* fDrive   = "fDrive";
    inline constexpr const char* fFold    = "fFold";
    inline constexpr const char* ringAmt  = "ringAmt";
    inline constexpr const char* ringFreq = "ringFreq";
    inline constexpr const char* chorus   = "chorus";
    inline constexpr const char* smear    = "smear";
    inline constexpr const char* highLvl  = "highLvl";

    // ---- Engine A: Memphis tape & sub
    inline constexpr const char* flutter  = "flutter";
    inline constexpr const char* preamp   = "preamp";
    inline constexpr const char* bits     = "bits";
    inline constexpr const char* srate    = "srate";
    inline constexpr const char* hiss     = "hiss";
    inline constexpr const char* aMix     = "aMix";
    inline constexpr const char* stopTime = "stopTime";
    inline constexpr const char* stopCurve= "stopCurve";
    inline constexpr const char* subLvl   = "subLvl";
    inline constexpr const char* subTune  = "subTune";
    inline constexpr const char* subDecay = "subDecay";
    inline constexpr const char* subSens  = "subSens";
    inline constexpr const char* subDrop  = "subDrop";

    // ---- Engine B: industrial
    inline constexpr const char* bDrive   = "bDrive";
    inline constexpr const char* clipMode = "clipMode";
    inline constexpr const char* bFold    = "bFold";
    inline constexpr const char* bAsym    = "bAsym";
    inline constexpr const char* combFreq = "combFreq";
    inline constexpr const char* combFb   = "combFb";
    inline constexpr const char* combMix  = "combMix";
    inline constexpr const char* cutoff   = "cutoff";
    inline constexpr const char* reso     = "reso";
    inline constexpr const char* bMix     = "bMix";
    inline constexpr const char* gateOn   = "gateOn";
    inline constexpr const char* gateRate = "gateRate";
    inline constexpr const char* gatePat  = "gatePat";
    inline constexpr const char* gateThr  = "gateThr";
    inline constexpr const char* gateDepth= "gateDepth";
    inline constexpr const char* gateDuty = "gateDuty";
    inline constexpr const char* gateSC   = "gateSC";

    // ---- Engine C: glitch
    inline constexpr const char* granMix  = "granMix";
    inline constexpr const char* granSize = "granSize";
    inline constexpr const char* granDens = "granDens";
    inline constexpr const char* granRev  = "granRev";
    inline constexpr const char* granSpread="granSpread";
    inline constexpr const char* autoGlitch="autoGlitch";
    inline constexpr const char* riseAmt  = "riseAmt";

    // ---- performance pads (also playable from MIDI notes 36-43)
    inline constexpr const char* padStop  = "padStop";
    inline constexpr const char* padDrag  = "padDrag";
    inline constexpr const char* pad32    = "pad32";
    inline constexpr const char* pad64    = "pad64";
    inline constexpr const char* pad128   = "pad128";
    inline constexpr const char* padRise  = "padRise";
    inline constexpr const char* padRev   = "padRev";
    inline constexpr const char* pad16    = "pad16";

    // ---- Chaos engine
    inline constexpr const char* chRate   = "chRate";
    inline constexpr const char* chSmooth = "chSmooth";
    inline constexpr const char* euSteps  = "euSteps";
    inline constexpr const char* euPulses = "euPulses";
    inline constexpr const char* euRot    = "euRot";
    inline constexpr const char* modDrive = "modDrive";
    inline constexpr const char* modCut   = "modCut";
    inline constexpr const char* modStut  = "modStut";
    inline constexpr const char* modGran  = "modGran";

    // ---- Master
    inline constexpr const char* smash    = "smash";
    inline constexpr const char* mSat     = "mSat";
    inline constexpr const char* subBoost = "subBoost";
    inline constexpr const char* limitOn  = "limitOn";
    inline constexpr const char* ceiling  = "ceiling";
    inline constexpr const char* autoLvl  = "autoLvl";
}

namespace pads
{
    // order matters: MIDI note 36 + index triggers the pad  (C1, C#1, D1, D#1, E1, F1, F#1, G1)
    inline const char* ids[8]    = { pid::padStop, pid::pad32, pid::pad64, pid::padRev,
                                     pid::pad128, pid::padDrag, pid::padRise, pid::pad16 };
    inline const char* labels[8] = { "TAPE STOP", "1/32", "1/64", "REV BURST",
                                     "1/128", "TAPE SLOW", "RISER", "1/16" };
    inline const char* notes[8]  = { "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1" };
    inline constexpr int firstNote = 36;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    int v = 1;

    auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
    auto bip = [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x * 100.0f)) + "%"; };
    auto hz = [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 2) + " kHz" : String (roundToInt (x)) + " Hz"; };
    auto db = [] (float x, int) { return String (x, 1) + " dB"; };
    auto sec = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
    auto ms = [] (float x, int) { return String (roundToInt (x)) + " ms"; };

    auto F = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                  std::function<String (float, int)> fmt)
    {
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, v }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt))));
    };
    auto B = [&] (const char* id, const juce::String& name, bool def)
    { L.add (std::make_unique<AudioParameterBool> (ParameterID { id, v }, name, def)); };
    auto C = [&] (const char* id, const char* name, StringArray items, int def)
    { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, v }, name, items, def)); };
    auto I = [&] (const char* id, const char* name, int lo, int hi, int def)
    { L.add (std::make_unique<AudioParameterInt> (ParameterID { id, v }, name, lo, hi, def)); };
    auto skew = [] (float lo, float hi, float centre)
    { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    // macros
    F (pid::chaos,   "Anarchy",   { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::grime,   "Grime",     { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::chMod,   "Chaos Mod", { 0.0f, 1.0f }, 0.3f, pct);
    C (pid::stutRate,"Stutter Rate", { "1/16", "1/32", "1/64", "1/128", "Random" }, 4);
    B (pid::latch,   "Pad Latch", false);
    F (pid::mix,     "Dry/Wet",   { 0.0f, 1.0f }, 1.0f,  pct);
    F (pid::inGain,  "Input",     { -24.0f, 24.0f }, 0.0f, db);
    F (pid::outGain, "Output",    { -24.0f, 24.0f }, 0.0f, db);

    B (pid::slOn, "Slicer On", false);
    B (pid::spOn, "Band Split On", true);
    B (pid::aOn,  "Tape On", true);
    B (pid::bOn,  "Industrial On", true);
    B (pid::cOn,  "Glitch On", true);
    B (pid::chOn, "Chaos On", true);
    B (pid::mOn,  "Master On", true);

    // slicer
    F (pid::slMix,    "Slicer Mix",      { 0.0f, 1.0f }, 1.0f, pct);
    C (pid::slSlices, "Slices",          { "16", "32" }, 0);
    C (pid::slLen,    "Loop Length",     { "1/2 bar", "1 bar", "2 bars" }, 1);
    F (pid::slDetect, "Transient Snap",  { 0.0f, 1.0f }, 0.5f, pct);
    F (pid::slDecay,  "Tail Drop",       { 0.0f, 1.0f }, 0.3f, pct);
    I (pid::slSeed,   "Pattern Seed",    0, 99, 6);

    // split
    F (pid::xover,    "Crossover",       skew (80.0f, 250.0f, 140.0f), 120.0f, hz);
    B (pid::monoLock, "Mono Lock", true);
    F (pid::harm,     "Harmonic Drive",  { 0.0f, 1.0f }, 0.25f, pct);
    F (pid::lowLvl,   "Low Level",       { -24.0f, 12.0f }, 0.0f, db);
    F (pid::snap,     "Transient Snap",  { -1.0f, 1.0f }, 0.2f, bip);
    F (pid::fDrive,   "Filth Drive",     { 0.0f, 1.0f }, 0.2f, pct);
    F (pid::fFold,    "Filth Fold",      { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::ringAmt,  "Ring Mod",        { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::ringFreq, "Ring Freq",       skew (20.0f, 2000.0f, 220.0f), 110.0f, hz);
    F (pid::chorus,   "Chorus Width",    { 0.0f, 1.0f }, 0.15f, pct);
    F (pid::smear,    "Phase Smear",     { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::highLvl,  "High Level",      { -24.0f, 12.0f }, 0.0f, db);

    // engine A
    F (pid::flutter,  "Flutter",         { 0.0f, 1.0f }, 0.15f, pct);
    F (pid::preamp,   "Preamp",          { 0.0f, 1.0f }, 0.25f, pct);
    F (pid::bits,     "Bit Depth",       { 4.0f, 16.0f, 0.1f }, 16.0f, [] (float x, int) { return String (x, 1) + " bit"; });
    F (pid::srate,    "Sample Rate",     skew (2000.0f, 44100.0f, 16000.0f), 44100.0f, hz);
    F (pid::hiss,     "Cassette Hiss",   { 0.0f, 1.0f }, 0.05f, pct);
    F (pid::aMix,     "Tape Mix",        { 0.0f, 1.0f }, 1.0f, pct);
    F (pid::stopTime, "Stop Time",       skew (0.05f, 3.0f, 0.6f), 0.7f, sec);
    F (pid::stopCurve,"Stop Curve",      { 0.0f, 1.0f }, 0.4f, pct);
    F (pid::subLvl,   "Sub Level",       { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::subTune,  "Sub Tune",        skew (25.0f, 90.0f, 45.0f), 45.0f, hz);
    F (pid::subDecay, "Sub Decay",       skew (0.05f, 2.0f, 0.4f), 0.45f, sec);
    F (pid::subSens,  "Kick Sensitivity",{ 0.0f, 1.0f }, 0.5f, pct);
    F (pid::subDrop,  "Sub Pitch Drop",  { 0.0f, 1.0f }, 0.3f, pct);

    // engine B
    F (pid::bDrive,   "Drive",{ 0.0f, 1.0f }, 0.3f, pct);
    C (pid::clipMode, "Clip Type",       { "Diode", "Asymmetric", "Console", "Wavefold" }, 2);
    F (pid::bFold,    "Wavefold",            { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::bAsym,    "Asymmetry",       { 0.0f, 1.0f }, 0.1f, pct);
    F (pid::combFreq, "Comb Pitch",      skew (40.0f, 4000.0f, 300.0f), 180.0f, hz);
    F (pid::combFb,   "Comb Feedback",   { -0.97f, 0.97f }, 0.6f, bip);
    F (pid::combMix,  "Comb Mix",        { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::cutoff,   "Filter Cutoff",   skew (40.0f, 20000.0f, 1500.0f), 20000.0f, hz);
    F (pid::reso,     "Resonance",       { 0.0f, 1.0f }, 0.15f, pct);
    F (pid::bMix,     "Industrial Mix",  { 0.0f, 1.0f }, 1.0f, pct);
    B (pid::gateOn,   "Reso Gate On", false);
    C (pid::gateRate, "Gate Rate",       { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2);
    C (pid::gatePat,  "Gate Pattern",    { "Transient", "Straight", "Off-beat", "Chaos Euclid" }, 0);
    F (pid::gateThr,  "Reso Gate",       { -70.0f, 0.0f }, -40.0f, db);
    F (pid::gateDepth,"Gate Depth",      { 0.0f, 1.0f }, 1.0f, pct);
    F (pid::gateDuty, "Gate Length",     { 0.1f, 0.95f }, 0.5f, pct);
    B (pid::gateSC,   "Gate Sidechain", false);

    // engine C
    F (pid::granMix,  "Scatter",    { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::granSize, "Grain Size",      skew (10.0f, 400.0f, 80.0f), 70.0f, ms);
    F (pid::granDens, "Grain Density",   skew (1.0f, 60.0f, 12.0f), 14.0f, [] (float x, int) { return String (x, 1) + " /s"; });
    F (pid::granRev,  "Grain Reverse",   { 0.0f, 1.0f }, 0.4f, pct);
    F (pid::granSpread,"Stereo Scatter", { 0.0f, 1.0f }, 0.7f, pct);
    F (pid::autoGlitch,"Auto Glitch",    { 0.0f, 1.0f }, 0.0f, pct);
    F (pid::riseAmt,  "Riser Pitch",     { 0.0f, 1.0f }, 0.6f, pct);

    for (int i = 0; i < 8; ++i)
        B (pads::ids[i], String ("Pad ") + pads::labels[i], false);

    // chaos engine
    C (pid::chRate,   "Chaos Rate",      { "1/4", "1/8", "1/16", "1/32" }, 2);
    F (pid::chSmooth, "Chaos Glide",     { 0.0f, 1.0f }, 0.2f, pct);
    I (pid::euSteps,  "Euclid Steps",    2, 16, 16);
    I (pid::euPulses, "Euclid Hits",     1, 16, 5);
    I (pid::euRot,    "Euclid Rotate",   0, 15, 0);
    F (pid::modDrive, "Chaos > Drive",   { -1.0f, 1.0f }, 0.4f, bip);
    F (pid::modCut,   "Chaos > Cutoff",  { -1.0f, 1.0f }, 0.5f, bip);
    F (pid::modStut,  "Chaos > Stutter", { 0.0f, 1.0f }, 0.3f, pct);
    F (pid::modGran,  "Chaos > Scramble",{ 0.0f, 1.0f }, 0.2f, pct);

    // master
    F (pid::smash,    "Smash",           { 0.0f, 1.0f }, 0.2f, pct);
    F (pid::mSat,     "Tape Saturation", { 0.0f, 1.0f }, 0.15f, pct);
    F (pid::subBoost, "Sub Boost",       { 0.0f, 12.0f }, 1.5f, db);
    B (pid::limitOn,  "Limiter", true);
    F (pid::ceiling,  "Ceiling",         { -12.0f, 0.0f }, -0.3f, db);
    B (pid::autoLvl,  "Level Match", false);

    return L;
}
