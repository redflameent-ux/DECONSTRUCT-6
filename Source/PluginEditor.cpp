#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

// ======================================================================== flow strip
juce::Rectangle<float> FlowStrip::blockRect (int i) const
{
    const float gap = 18.0f;
    const float w = (getWidth() - gap * (blocks.size() - 1)) / (float) blocks.size();
    return { i * (w + gap), 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> FlowStrip::ledRect (int i) const
{
    auto r = blockRect (i);
    return { r.getRight() - 24.0f, r.getY() + 8.0f, 16.0f, 16.0f };
}

void FlowStrip::paint (juce::Graphics& g)
{
    for (int i = 0; i < (int) blocks.size(); ++i)
    {
        const auto& b = blocks[(size_t) i];
        auto r = blockRect (i).reduced (0.5f);
        const bool on = state.getRawParameterValue (b.powerId)->load() > 0.5f;
        const bool sel = i == selected;

        g.setColour (sel ? col::panelHi.brighter (0.06f) : col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (sel ? b.colour : col::border);
        g.drawRoundedRectangle (r, 6.0f, sel ? 1.6f : 1.0f);
        g.setColour (on ? b.colour : col::border);
        g.fillRect (r.getX() + 1.0f, r.getBottom() - 4.0f, r.getWidth() - 2.0f, 3.0f);

        g.setColour (on ? col::text : col::dim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (b.name, r.reduced (10.0f, 0.0f).withTrimmedTop (8.0f).withHeight (18.0f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (b.sub, r.reduced (10.0f, 0.0f).withTrimmedTop (27.0f).withHeight (14.0f).toNearestInt(), juce::Justification::centredLeft);

        // power LED
        auto led = ledRect (i);
        g.setColour (on ? b.colour : col::panelHi);
        g.fillEllipse (led.reduced (3.0f));
        g.setColour (on ? b.colour.withAlpha (0.4f) : col::border);
        g.drawEllipse (led.reduced (1.0f), 1.2f);

        // arrow to the next block
        if (i < (int) blocks.size() - 1)
        {
            const float ax = r.getRight() + 4.0f, ay = r.getCentreY();
            juce::Path a;
            a.addTriangle (ax, ay - 5.0f, ax, ay + 5.0f, ax + 9.0f, ay);
            g.setColour (col::dim);
            g.fillPath (a);
        }
    }
}

void FlowStrip::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) blocks.size(); ++i)
    {
        if (ledRect (i).expanded (6.0f).contains (e.position))
        {
            auto* prm = state.getParameter (blocks[(size_t) i].powerId);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
            repaint();
            return;
        }
        if (blockRect (i).contains (e.position))
        {
            selected = i;
            if (onSelect) onSelect (i);
            repaint();
            return;
        }
    }
}

// ======================================================================== MIDI chart
void DC6Editor::MidiChart::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (col::glitch);
    g.fillRect (r.getX() + 10.0f, r.getY() + 11.0f, 3.0f, 12.0f);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText ("MIDI KEYS", (int) r.getX() + 18, (int) r.getY() + 6, 200, 22, juce::Justification::centredLeft);

    auto body = r.reduced (14.0f).withTrimmedTop (22.0f);
    const float rowH = body.getHeight() / 9.0f;
    for (int i = 0; i < 8; ++i)
    {
        auto row = body.removeFromTop (rowH);
        g.setColour (col::glitch);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (pads::notes[i], row.removeFromLeft (40.0f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (col::text);
        g.drawText (pads::labels[i], row.toNearestInt(), juce::Justification::centredLeft);
    }
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawFittedText ("notes 36-43  (FL Studio names these C3-G3)", body.toNearestInt(), juce::Justification::centredLeft, 2);
}

// ======================================================================== editor
DC6Editor::DC6Editor (DC6Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    addAndMakeVisible (root);
    root.setSize (baseW, baseH);
    root.painter = [this] (juce::Graphics& g)
    {
        g.fillAll (col::bg);

        // logo artwork, fitted to the left of the header
        if (logo.isValid())
        {
            auto r = logoArea.toFloat();
            const float scale = juce::jmin (r.getWidth() / (float) logo.getWidth(), r.getHeight() / (float) logo.getHeight());
            const auto dest = juce::Rectangle<float> (r.getX(), r.getCentreY() - logo.getHeight() * scale * 0.5f,
                                                      logo.getWidth() * scale, logo.getHeight() * scale);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (logo, dest, juce::RectanglePlacement::stretchToFit);
        }

        // meters
        auto m = meterArea.toFloat();
        auto drawMeter = [&] (juce::Rectangle<float> b, float level, const char* label)
        {
            g.setColour (col::dim);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (label, b.removeFromLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);
            g.setColour (col::panelHi);
            g.fillRoundedRectangle (b, 2.0f);
            const float db = juce::jlimit (-60.0f, 6.0f, dc6::gainToDb (level));
            const float w = b.getWidth() * (db + 60.0f) / 66.0f;
            g.setColour (db > -0.5f ? col::blood : col::split);
            g.fillRoundedRectangle (b.withWidth (w), 2.0f);
            g.setColour (col::border);
            const float zx = b.getX() + b.getWidth() * 60.0f / 66.0f;
            g.drawVerticalLine ((int) zx, b.getY(), b.getBottom());
        };
        drawMeter (m.removeFromTop (12.0f), proc.meterIn.load(), "IN");
        m.removeFromTop (5.0f);
        drawMeter (m.removeFromTop (12.0f), proc.meterOut.load(), "OUT");
        m.removeFromTop (4.0f);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("SMASH  -" + juce::String (proc.meterGr.load(), 1) + " dB", m.toNearestInt(), juce::Justification::centredLeft);

        // front panel group boxes
        for (auto& gr : groups)
        {
            auto b = gr.box.toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (col::border);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (gr.colour);
            g.fillRect (b.getX() + 10.0f, b.getY() + 11.0f, 3.0f, 12.0f);
            g.setColour (col::text);
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (gr.title, (int) b.getX() + 18, (int) b.getY() + 6, (int) b.getWidth() - 24, 22, juce::Justification::centredLeft);
        }

        // pad strip label
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("PERFORM", padArea.withWidth (60), juce::Justification::centredLeft);
    };

    // ---- header
    presetName.onClick = [this] { showPresetMenu(); };
    prevBtn.onClick = [this] { proc.presets->prev(); };
    nextBtn.onClick = [this] { proc.presets->next(); };
    saveBtn.onClick = [this] { savePreset(); };
    presetName.setTooltip ("Browse presets");
    for (auto* b : { &presetName, &prevBtn, &nextBtn, &saveBtn }) root.addAndMakeVisible (*b);
    inKnob = std::make_unique<Knob> (proc.apvts, pid::inGain, col::master, "Input");
    outKnob = std::make_unique<Knob> (proc.apvts, pid::outGain, col::master, "Output");
    root.addAndMakeVisible (*inKnob);
    root.addAndMakeVisible (*outKnob);

    // ---- flow strip
    flow = std::make_unique<FlowStrip> (proc.apvts, std::vector<FlowStrip::Block> {
        { "SLICER",           "loop chopper",     pid::slOn, col::slicer },
        { "SPLIT",            "filth & sub",      pid::spOn, col::split  },
        { "1  MEMPHIS",       "tape & sub",       pid::aOn,  col::tape   },
        { "2  INDUSTRIAL",    "decay",            pid::bOn,  col::indus  },
        { "3  MICRO-BUFFER",  "matrix",           pid::cOn,  col::glitch },
        { "CHAOS",            "s&h + euclid",     pid::chOn, col::chaos  },
        { "MASTER",           "output stage",     pid::mOn,  col::master } });
    flow->onSelect = [this] (int i) { showPage (i); };
    root.addAndMakeVisible (*flow);

    buildPages();
    buildFrontPanel();
    shownPreset = proc.presets->currentName();
    presetName.setButtonText (shownPreset);
    showPage (current);

    setResizable (true, true);
    setResizeLimits (baseW * 7 / 10, baseH * 7 / 10, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
    startTimerHz (30);
}

DC6Editor::~DC6Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DC6Editor::buildPages()
{
    auto& s = proc.apvts;
    using namespace pid;

    auto add = [&] (juce::Colour c) { pages.push_back (std::make_unique<ParamPage> (s, c)); return pages.back().get(); };

    auto* sl = add (col::slicer);
    sl->addSection ("Slicer", { slMix, slSlices, slLen, slDetect, slDecay, slSeed }, 3);
    stepGrid = std::make_unique<StepGrid> (proc.lanes, proc.slicer, s.getRawParameterValue (chaos));
    sl->setExtra (stepGrid.get(), 640);

    auto* sp = add (col::split);
    sp->addSection ("Crossover", { xover, monoLock }, 1);
    sp->addSection ("Low band  /  sub control", { harm, lowLvl }, 1);
    sp->addSection ("High band  /  filth", { snap, fDrive, fFold, ringAmt, ringFreq, chorus, smear, highLvl }, 4);

    auto* ta = add (col::tape);
    ta->addSection ("Grime  /  12-bit cassette", { preamp, bits, srate, hiss, flutter, aMix }, 3);
    ta->addSection ("Tape brake", { stopTime, stopCurve }, 1);
    ta->addSection ("808 sub resynth  (below 120 Hz)", { subLvl, subTune, subDecay, subSens, subDrop }, 3);

    auto* in = add (col::indus);
    in->addSection ("Dual-stage drive", { bDrive, clipMode, bFold, bAsym }, 2);
    in->addSection ("Screaming filters", { combFreq, combFb, combMix, cutoff, reso, bMix }, 3);
    in->addSection ("Reso gate", { gateOn, gateThr, gatePat, gateRate, gateDuty, gateDepth, gateSC }, 4);

    auto* gl = add (col::glitch);
    gl->addSection ("Micro-stutter", { stutRate, autoGlitch, riseAmt }, 2);
    gl->addSection ("Scatter  /  granular reverser", { granMix, granSize, granDens, granRev, granSpread }, 3);
    gl->setExtra (&midiChart, 210);

    auto* ch = add (col::chaos);
    ch->addSection ("Sample & hold", { chMod, chRate, chSmooth }, 1);
    ch->addSection ("Euclid", { euSteps, euPulses, euRot }, 1);
    ch->addSection ("Routing", { modDrive, modCut, modStut, modGran }, 2);
    chaosView = std::make_unique<ChaosView> (proc.chaosEngine, s);
    ch->setExtra (chaosView.get(), 440);

    auto* ma = add (col::master);
    ma->addSection ("Dynamics", { smash, mSat, subBoost }, 3);
    ma->addSection ("Brickwall", { limitOn, ceiling, autoLvl }, 3);

    for (auto& pg : pages) root.addChildComponent (*pg);
}

void DC6Editor::buildFrontPanel()
{
    auto& s = proc.apvts;
    using namespace pid;
    auto ctrl = [&] (const char* id, juce::Colour c, const juce::String& label) -> juce::Component*
    {
        frontOwned.push_back (makeControl (s, id, c, label));
        root.addAndMakeVisible (*frontOwned.back());
        return frontOwned.back().get();
    };

    tapeSlow = std::make_unique<PadButton> (s, padDrag, "TAPE SLOW", "F1  -1 oct", col::tape);
    root.addAndMakeVisible (*tapeSlow);

    groups.push_back ({ "MEMPHIS SUB & TAPE", col::tape,
                        { tapeSlow.get(), ctrl (subLvl, col::tape, "Sub Level"), ctrl (grime, col::tape, "Grime / 12-bit") } });
    groups.push_back ({ "INDUSTRIAL DECAY", col::indus,
                        { ctrl (bFold, col::indus, "Wavefold"), ctrl (bDrive, col::indus, "Drive"),
                          ctrl (gateThr, col::indus, "Reso Gate"), ctrl (gateOn, col::indus, "Gate On") } });
    groups.push_back ({ "MICRO-BUFFER", col::glitch,
                        { ctrl (stutRate, col::glitch, "Stutter Rate"), ctrl (granMix, col::glitch, "Scatter"),
                          ctrl (chMod, col::chaos, "Chaos Mod") } });
    groups.push_back ({ "GLOBAL MASTER", col::blood,
                        { ctrl (chaos, col::blood, "Anarchy"), ctrl (mix, col::master, "Dry / Wet") } });
    groups.back().big = 0;

    for (int i = 0; i < 8; ++i)
    {
        const auto c = i == 0 || i == 5 ? col::tape : (i == 3 ? col::glitch.interpolatedWith (col::chaos, 0.5f) : col::glitch);
        padBtns.push_back (std::make_unique<PadButton> (s, pads::ids[i], pads::labels[i], pads::notes[i], c));
        root.addAndMakeVisible (*padBtns.back());
    }
    latch = std::make_unique<Toggle> (s, pid::latch, col::text, "Latch");
    root.addAndMakeVisible (*latch);
}

void DC6Editor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    root.setTransform (juce::AffineTransform::scale (scale));

    auto r = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (14, 10);

    // header
    auto head = r.removeFromTop (70);
    logoArea = head.removeFromLeft (330).expanded (0, 4);
    auto right = head.removeFromRight (330);
    outKnob->setBounds (right.removeFromRight (66).expanded (0, 6));
    inKnob->setBounds (right.removeFromRight (66).expanded (0, 6));
    right.removeFromRight (12);
    meterArea = right.removeFromRight (180).withSizeKeepingCentre (180, 46);
    auto pb = head.withSizeKeepingCentre (380, 32);
    prevBtn.setBounds (pb.removeFromLeft (32));
    saveBtn.setBounds (pb.removeFromRight (58));
    pb.removeFromRight (6);
    nextBtn.setBounds (pb.removeFromRight (32));
    pb.reduce (6, 0);
    presetName.setBounds (pb);

    r.removeFromTop (4);
    flow->setBounds (r.removeFromTop (48));
    r.removeFromTop (4);

    // perform pads at the bottom
    padArea = r.removeFromBottom (62);
    {
        auto pa = padArea;
        pa.removeFromLeft (62);
        latch->setBounds (pa.removeFromRight (70));
        pa.removeFromRight (8);
        const int w = pa.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
            padBtns[(size_t) i]->setBounds (pa.getX() + i * w, pa.getY() + 4, w - 4, pa.getHeight() - 8);
    }
    r.removeFromBottom (4);

    // front panel
    auto front = r.removeFromBottom (168);
    const int slots[] = { 3, 4, 3, 3 };
    const int gap = 8;
    const float slotW = (front.getWidth() - gap * 3) / 13.0f;
    int x = front.getX();
    for (int gi = 0; gi < (int) groups.size(); ++gi)
    {
        auto& gr = groups[(size_t) gi];
        const int w = (int) std::round (slotW * slots[gi]);
        gr.box = { x, front.getY(), w, front.getHeight() };
        x += w + gap;
        auto inner = gr.box.reduced (6).withTrimmedTop (24);
        int units = 0;
        for (int i = 0; i < (int) gr.ctrls.size(); ++i) units += i == gr.big ? 2 : 1;
        const float unitW = (float) inner.getWidth() / (float) juce::jmax (1, units);
        float cx = (float) inner.getX();
        for (int i = 0; i < (int) gr.ctrls.size(); ++i)
        {
            const float sw = unitW * (i == gr.big ? 2.0f : 1.0f);
            auto cell = juce::Rectangle<float> (cx, (float) inner.getY(), sw, (float) inner.getHeight()).toNearestInt();
            cx += sw;
            if (gr.ctrls[(size_t) i] == tapeSlow.get())
                tapeSlow->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 86), 64));
            else
                gr.ctrls[(size_t) i]->setBounds (cell);
        }
    }
    r.removeFromBottom (8);

    for (auto& pg : pages) pg->setBounds (r);
}

void DC6Editor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void DC6Editor::showPage (int index)
{
    current = index;
    for (int i = 0; i < (int) pages.size(); ++i) pages[(size_t) i]->setVisible (i == index);
    flow->selected = index;
    flow->repaint();
}

void DC6Editor::timerCallback()
{
    for (int i = 0; i < 8; ++i) padBtns[(size_t) i]->setExternallyHeld (proc.midiPadHeld[i].load());
    tapeSlow->setExternallyHeld (proc.midiPadHeld[5].load());
    for (auto& pd : padBtns) pd->repaint();
    tapeSlow->repaint();
    flow->repaint();
    root.repaint (meterArea);
    if (current == 0) stepGrid->repaint();
    if (current == 5) chaosView->tick();

    const auto nm = proc.presets->currentName();
    if (nm != shownPreset) { shownPreset = nm; presetName.setButtonText (nm); }
}

void DC6Editor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& list = proc.presets->entries();
    std::map<juce::String, juce::PopupMenu> banks;
    for (int i = 0; i < list.size(); ++i)
        banks[list[i].bank].addItem (i + 1, list[i].name, true, i == proc.presets->currentIndex());
    for (auto& [bank, sub] : banks)
        menu.addSubMenu (bank, sub);
    menu.addSeparator();
    menu.addItem (10000, "Open user preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName),
        [this] (int result)
        {
            if (result == 10000)
            {
                auto f = PresetManager::userFolder();
                f.createDirectory();
                f.revealToUser();
            }
            else if (result > 0)
                proc.presets->load (result - 1);
        });
}

void DC6Editor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets->currentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r == 1) proc.presets->saveUser (w->getTextEditorContents ("name"));
    }), true);
}
