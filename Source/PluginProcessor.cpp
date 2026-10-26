#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace dc6;

static constexpr int kChunk = 32;

DC6Processor::DC6Processor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "DC6", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            params[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
    for (auto& m : midiPadHeld) m = false;
    presets = std::make_unique<PresetManager> (*this);
}

DC6Processor::~DC6Processor() = default;

bool DC6Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono())
            return false;
    }
    return true;
}

void DC6Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxBlock = juce::jmax (samplesPerBlock, kChunk);

    slicer.prepare (sr, &lanes);
    split.prepare (sr, kChunk);
    tapeTone.prepare (sr);
    subSynth.prepare (sr);
    transport.prepare (sr);
    industrial.prepare (sr, kChunk);
    gate.prepare (sr);
    scrambler.prepare (sr);
    stutter.prepare (sr);
    master.prepare (sr, kChunk);
    chaosEngine.reset();

    work.setSize (2, maxBlock);
    low.setSize (2, maxBlock);
    ref.setSize (2, maxBlock);
    dry.setSize (2, maxBlock);

    latency = TapeTransport::latencySamples (sr);
    setLatencySamples (latency);
    for (auto& d : dryDelay) d.assign ((size_t) latency + 1, 0.0f);
    dryDelayW = 0;

    mixSm.reset (sr, 0.03);  mixSm.setCurrentAndTargetValue (p (pid::mix));
    inSm.reset (sr, 0.03);   inSm.setCurrentAndTargetValue (dbToGain (p (pid::inGain)));
    outSm.reset (sr, 0.03);  outSm.setCurrentAndTargetValue (dbToGain (p (pid::outGain)));
    internalPpq = 0.0;
}

static inline float safety (float x) noexcept
{
    // internal brickwall for the buffer / feedback stages: transparent below 0 dBFS, caps runaway peaks
    const float a = std::abs (x);
    if (a <= 1.0f) return x;
    return std::copysign (1.0f + 0.5f * std::tanh (2.0f * (a - 1.0f)), x);
}

void DC6Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int n = buffer.getNumSamples();
    const int nCh = mainBus.getNumChannels();
    if (n == 0 || nCh == 0) return;

    if (work.getNumSamples() < n)   // hosts are allowed to send bigger blocks than promised
    {
        work.setSize (2, n, false, false, true); low.setSize (2, n, false, false, true);
        ref.setSize (2, n, false, false, true);  dry.setSize (2, n, false, false, true);
    }

    // sidechain (optional)
    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getReadPointer (juce::jmin (1, sc.getNumChannels() - 1));
        }
    }

    // ---------------------------------------------------------------- musical time
    Clock base;
    base.sampleRate = sr;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) base.bpm = juce::jlimit (30.0, 300.0, *bpm);
            if (auto ts = pos->getTimeSignature()) base.beatsPerBar = juce::jmax (1.0, ts->numerator * 4.0 / juce::jmax (1, ts->denominator));
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) { base.ppq0 = *ppq; hostPlaying = true; }
        }
    base.ppqPerSample = base.bpm / 60.0 / sr;
    if (! hostPlaying) base.ppq0 = internalPpq;
    internalPpq = base.ppq0 + base.ppqPerSample * n;

    // ---------------------------------------------------------------- copy input in (mono -> both sides)
    for (int c = 0; c < 2; ++c)
        work.copyFrom (c, 0, mainBus, juce::jmin (c, nCh - 1), 0, n);

    inSm.setTargetValue (dbToGain (p (pid::inGain)));
    float peakIn = 0.0f;
    {
        float* L = work.getWritePointer (0); float* R = work.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float g = inSm.getNextValue();
            L[i] *= g; R[i] *= g;
            peakIn = juce::jmax (peakIn, std::abs (L[i]), std::abs (R[i]));
            // dry path, delayed to line up with the processed path
            for (int c = 0; c < 2; ++c)
            {
                auto& d = dryDelay[c];
                const float x = c == 0 ? L[i] : R[i];
                d[(size_t) dryDelayW] = x;
                const int r = (dryDelayW + 1) % (int) d.size();
                dry.setSample (c, i, d[(size_t) r]);
            }
            dryDelayW = (dryDelayW + 1) % (int) dryDelay[0].size();
        }
        ref.copyFrom (0, 0, work, 0, 0, n);
        ref.copyFrom (1, 0, work, 1, 0, n);
    }

    // ---------------------------------------------------------------- read controls
    const float anarchy = p (pid::chaos);
    const float grime = p (pid::grime);
    const bool latchOn = b (pid::latch);
    const bool chaosOn = b (pid::chOn);
    const float modDepth = chaosOn ? p (pid::chMod) : 0.0f;

    Slicer::Settings slS;
    slS.mix = p (pid::slMix); slS.detect = p (pid::slDetect); slS.decay = p (pid::slDecay);
    slS.chaos = anarchy; slS.slices = (int) p (pid::slSlices) == 1 ? 32 : 16;
    slS.lenIndex = (int) p (pid::slLen); slS.seed = (int) p (pid::slSeed);

    SplitStage::Settings spS;
    spS.enabled = b (pid::spOn); spS.monoLock = b (pid::monoLock);
    spS.xover = p (pid::xover); spS.harm = p (pid::harm); spS.lowGain = dbToGain (p (pid::lowLvl));
    spS.snap = p (pid::snap); spS.drive = p (pid::fDrive); spS.fold = p (pid::fFold);
    spS.ringAmt = p (pid::ringAmt); spS.ringFreq = p (pid::ringFreq);
    spS.chorus = p (pid::chorus); spS.smear = p (pid::smear); spS.highGain = dbToGain (p (pid::highLvl));

    TapeTone::Settings taS;
    taS.preamp = juce::jmax (p (pid::preamp), 0.5f * grime);
    taS.bits = juce::jmin (p (pid::bits), 16.0f - 10.0f * grime);
    taS.srate = juce::jmin (p (pid::srate), 44100.0f * std::pow (8000.0f / 44100.0f, grime));
    taS.hiss = juce::jmax (p (pid::hiss), 0.6f * grime);
    taS.mix = p (pid::aMix);

    SubResynth::Settings suS;
    suS.level = p (pid::subLvl); suS.on = b (pid::aOn) && suS.level > 0.001f;
    suS.tune = p (pid::subTune); suS.decay = p (pid::subDecay); suS.sens = p (pid::subSens); suS.drop = p (pid::subDrop);

    IndustrialTone::Settings inS;
    inS.drive = p (pid::bDrive); inS.fold = p (pid::bFold); inS.asym = p (pid::bAsym);
    inS.combFreq = p (pid::combFreq); inS.combFb = p (pid::combFb); inS.combMix = p (pid::combMix);
    inS.reso = p (pid::reso); inS.mix = p (pid::bMix); inS.clipMode = (int) p (pid::clipMode);
    const float baseDrive = juce::jmin (1.0f, inS.drive + 0.35f * anarchy * (1.0f - inS.drive));
    const float baseCut = p (pid::cutoff);

    PumpGate::Settings gS;
    gS.on = b (pid::bOn) && b (pid::gateOn); gS.useSidechain = b (pid::gateSC);
    gS.rate = (int) p (pid::gateRate); gS.pattern = (int) p (pid::gatePat);
    gS.threshDb = p (pid::gateThr); gS.depth = p (pid::gateDepth); gS.duty = p (pid::gateDuty);
    gS.euSteps = (int) p (pid::euSteps); gS.euPulses = (int) p (pid::euPulses); gS.euRot = (int) p (pid::euRot);

    Scrambler::Settings scS;
    scS.sizeMs = p (pid::granSize); scS.density = p (pid::granDens); scS.reverse = p (pid::granRev); scS.spread = p (pid::granSpread);
    const float baseScatter = p (pid::granMix);

    ChaosEngine::Settings chS;
    chS.rate = (int) p (pid::chRate); chS.glide = p (pid::chSmooth); chS.depth = juce::jmax (anarchy, modDepth);
    chS.steps = (int) p (pid::euSteps); chS.pulses = (int) p (pid::euPulses); chS.rotate = (int) p (pid::euRot);

    Master::Settings mS;
    mS.enabled = b (pid::mOn); mS.limit = b (pid::limitOn); mS.autoLevel = b (pid::autoLvl);
    mS.smash = p (pid::smash); mS.sat = juce::jmin (1.0f, p (pid::mSat) + 0.3f * anarchy);
    mS.subBoostDb = p (pid::subBoost); mS.ceilingDb = p (pid::ceiling);

    const bool slOn = b (pid::slOn), aOn = b (pid::aOn), bOn = b (pid::bOn), cOn = b (pid::cOn);
    const float flutter = juce::jmin (1.0f, p (pid::flutter) + 0.5f * anarchy);
    const float autoGlitch = p (pid::autoGlitch);
    const int stutRate = (int) p (pid::stutRate);
    const float riseAmt = p (pid::riseAmt);
    const float stopTime = p (pid::stopTime), stopCurve = p (pid::stopCurve);
    mixSm.setTargetValue (p (pid::mix));
    outSm.setTargetValue (dbToGain (p (pid::outGain)));

    // ---------------------------------------------------------------- chunked processing (MIDI lands on chunk edges)
    auto midiIt = midi.begin();
    float peakOut = 0.0f;
    int pos = 0;
    while (pos < n)
    {
        // apply MIDI events that fall at or before this chunk start
        while (midiIt != midi.end() && (*midiIt).samplePosition <= pos)
        {
            const auto msg = (*midiIt).getMessage();
            const int idx = msg.getNoteNumber() - pads::firstNote;
            if (idx >= 0 && idx < 8)
            {
                if (msg.isNoteOn()) midiPad[idx] = latchOn ? ! midiPad[idx] : true;
                else if (msg.isNoteOff() && ! latchOn) midiPad[idx] = false;
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                for (auto& m : midiPad) m = false;
            ++midiIt;
        }
        int len = juce::jmin (kChunk, n - pos);
        if (midiIt != midi.end())
            len = juce::jmin (len, juce::jmax (1, (*midiIt).samplePosition - pos));

        Clock clk = base;
        clk.ppq0 = base.ppqAt (pos);

        bool pad[8];
        for (int i = 0; i < 8; ++i) pad[i] = b (pads::ids[i]) || midiPad[i];

        float* L = work.getWritePointer (0) + pos;
        float* R = work.getWritePointer (1) + pos;
        float* lL = low.getWritePointer (0) + pos;
        float* lR = low.getWritePointer (1) + pos;
        const float* cL = scL != nullptr ? scL + pos : nullptr;
        const float* cR = scR != nullptr ? scR + pos : nullptr;

        // chaos engine
        const auto tick = chaosEngine.advance (clk, len, chS);
        const float v = chaosOn ? chaosEngine.output() : 0.0f;

        // 0. slicer
        if (slOn) slicer.process (L, R, len, clk, slS);
        else slicer.guiStep = -1;

        // 1. band split: L/R becomes the high band, low band goes to lL/lR
        split.process (L, R, lL, lR, len, spS);

        // MODULE 1: Memphis tape & sub (tone on the high band, sub into the low band)
        if (aOn) tapeTone.process (L, R, len, taS);
        subSynth.process (spS.enabled ? lL : L, spS.enabled ? lR : R, lL, lR, len, suS);

        // MODULE 2: industrial decay
        if (bOn)
        {
            inS.drive = juce::jlimit (0.0f, 1.0f, baseDrive + 0.5f * p (pid::modDrive) * v * modDepth);
            inS.cutoff = juce::jlimit (30.0f, 20000.0f, baseCut * std::pow (2.0f, 4.0f * p (pid::modCut) * v * modDepth));
            industrial.process (L, R, len, inS);
        }

        // MODULE 3a: granular scatter / reverse burst
        if (cOn)
        {
            scS.mix = juce::jlimit (0.0f, 1.0f, baseScatter + p (pid::modGran) * modDepth * (0.5f + 0.5f * v) * (baseScatter > 0.0f ? 1.0f : 0.0f) + 0.3f * anarchy * baseScatter);
            Scrambler::Settings use = scS;
            if (pad[3]) { use.mix = 1.0f; use.reverse = 1.0f; use.density = juce::jmax (use.density, 30.0f); use.spread = 1.0f; use.sizeMs = juce::jmin (use.sizeMs, 90.0f); }
            scrambler.process (L, R, len, use);
            for (int i = 0; i < len; ++i) { L[i] = safety (L[i]); R[i] = safety (R[i]); }
        }

        // recombine bands
        for (int i = 0; i < len; ++i) { L[i] += lL[i]; R[i] += lR[i]; }

        // reso gate (module 2, runs on the whole mix so the sub chops too)
        gate.process (L, R, cL, cR, len, clk, gS);

        // tape transport: wow/flutter, tape stop, tape slow (module 1, whole mix)
        TapeTransport::Settings trS;
        trS.enabled = aOn; trS.stop = pad[0]; trS.drag = pad[5];
        trS.flutter = aOn ? flutter : 0.0f; trS.stopTime = stopTime; trS.stopCurve = stopCurve;
        transport.process (L, R, len, trS);

        // MODULE 3b: micro-stutters
        if (cOn)
        {
            Stutter::Mode m = Stutter::Off;
            if      (pad[6]) m = Stutter::Riser;
            else if (pad[4]) m = Stutter::Rep128;
            else if (pad[2]) m = Stutter::Rep64;
            else if (pad[1]) m = Stutter::Rep32;
            else if (pad[7]) m = Stutter::Rep16;

            if (tick.newStep)
            {
                const float prob = autoGlitch + 0.35f * anarchy * anarchy + (tick.hit ? p (pid::modStut) * anarchy : 0.0f);
                if (prob > 0.0f && chaosEngine.uni() < prob)
                {
                    static const Stutter::Mode byRate[4] = { Stutter::Rep16, Stutter::Rep32, Stutter::Rep64, Stutter::Rep128 };
                    int r = stutRate;
                    if (r >= 4)   // random: the chaos S&H picks the buffer length when Chaos Mod is up
                        r = modDepth > 0.05f ? juce::jlimit (0, 3, (int) ((v * 0.5f + 0.5f) * 4.0f)) : chaosEngine.pick (4);
                    Stutter::Mode am = byRate[r];
                    const float roll = chaosEngine.uni();
                    if (anarchy > 0.5f && roll < 0.18f) am = Stutter::Riser;
                    else if (roll > 0.82f) am = Stutter::Reverse;
                    const int dur = (int) (clk.samplesPerBeat() * (anarchy > 0.75f && roll < 0.5f ? 0.5 : 0.25));
                    stutter.fireAuto (am, dur);
                }
            }
            stutter.process (L, R, len, clk, m, riseAmt);
            for (int i = 0; i < len; ++i) { L[i] = safety (L[i]); R[i] = safety (R[i]); }
        }

        // master
        master.process (L, R, ref.getReadPointer (0) + pos, ref.getReadPointer (1) + pos, len, mS);

        pos += len;
    }

    // ---------------------------------------------------------------- dry/wet, output gain, write back
    {
        float* L = work.getWritePointer (0); float* R = work.getWritePointer (1);
        const float* dL = dry.getReadPointer (0); const float* dR = dry.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue(), g = outSm.getNextValue();
            float l = (dL[i] + m * (L[i] - dL[i])) * g;
            float r = (dR[i] + m * (R[i] - dR[i])) * g;
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            L[i] = l; R[i] = r;
            peakOut = juce::jmax (peakOut, std::abs (l), std::abs (r));
        }
        if (nCh == 1)
            for (int i = 0; i < n; ++i) mainBus.setSample (0, i, 0.5f * (L[i] + R[i]));
        else
            for (int c = 0; c < nCh; ++c) mainBus.copyFrom (c, 0, work, juce::jmin (c, 1), 0, n);
    }

    // meters for the screen
    meterIn = juce::jmax (peakIn, meterIn.load() * 0.85f);
    meterOut = juce::jmax (peakOut, meterOut.load() * 0.85f);
    meterGr = master.guiGainReduction();
    meterSub = subSynth.guiLastAmp();
    meterGate = gate.guiGain();
    guiStutter = (int) stutter.current();
    guiTapeStopped = transport.isStopped();
    for (int i = 0; i < 8; ++i) midiPadHeld[i] = midiPad[i];
}

// ======================================================================== state
juce::XmlElement* DC6Processor::lanesToXml() const
{
    auto* x = new juce::XmlElement ("LANES");
    for (int l = 0; l < SlicerLanes::NumLanes; ++l)
    {
        juce::StringArray vals;
        for (int s = 0; s < SlicerLanes::kSteps; ++s) vals.add (juce::String (lanes.v[l][s].load(), 3));
        x->setAttribute ("l" + juce::String (l), vals.joinIntoString (","));
    }
    return x;
}

void DC6Processor::lanesFromXml (const juce::XmlElement* x)
{
    lanes.reset();
    if (x == nullptr) return;
    for (int l = 0; l < SlicerLanes::NumLanes; ++l)
    {
        juce::StringArray vals;
        vals.addTokens (x->getStringAttribute ("l" + juce::String (l)), ",", "");
        for (int s = 0; s < juce::jmin (vals.size(), (int) SlicerLanes::kSteps); ++s)
            lanes.v[l][s] = juce::jlimit (SlicerLanes::minOf (l), SlicerLanes::maxOf (l), vals[s].getFloatValue());
    }
}

void DC6Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->addChildElement (lanesToXml());
    xml->setAttribute ("preset", presets->currentName());
    copyXmlToBinary (*xml, destData);
}

void DC6Processor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    lanesFromXml (xml->getChildByName ("LANES"));
    presets->setCurrentName (xml->getStringAttribute ("preset", "Init"));
    auto copy = std::make_unique<juce::XmlElement> (*xml);
    if (auto* l = copy->getChildByName ("LANES")) copy->removeChildElement (l, true);
    apvts.replaceState (juce::ValueTree::fromXml (*copy));
}

juce::AudioProcessorEditor* DC6Processor::createEditor() { return new DC6Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DC6Processor(); }
