#pragma once
#include "Widgets.h"
#include "../dsp/Slicer.h"
#include "../dsp/ChaosEngine.h"
#include "../dsp/Common.h"

// ---------------------------------------------------------------- 16-step lane editor for the slicer
class StepGrid : public juce::Component
{
public:
    StepGrid (dc6::SlicerLanes& l, dc6::Slicer& s, std::atomic<float>* anarchyParam)
        : lanes (l), slicer (s), anarchy (anarchyParam)
    {
        randomBtn.setButtonText ("RANDOMIZE");
        clearBtn.setButtonText ("CLEAR");
        randomBtn.onClick = [this] { randomize(); };
        clearBtn.onClick = [this] { lanes.reset(); repaint(); };
        addAndMakeVisible (randomBtn);
        addAndMakeVisible (clearBtn);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        // header: zone readout
        const float a = anarchy->load();
        const char* zone = a <= 0.33f ? "MEMPHIS POCKET" : (a <= 0.66f ? "SQUAREPUSHER COMPLEXITY" : "APHEX DRILL-CORE");
        g.setColour (col::slicer);
        g.fillRect (r.getX() + 10.0f, r.getY() + 11.0f, 3.0f, 12.0f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText ("STEP GRID", (int) r.getX() + 18, (int) r.getY() + 6, 120, 22, juce::Justification::centredLeft);
        g.setColour (col::slicer);
        g.drawText (juce::String ("ZONE: ") + zone + "  (" + juce::String (juce::roundToInt (a * 100)) + "% ANARCHY)",
                    (int) r.getX() + 110, (int) r.getY() + 6, 360, 22, juce::Justification::centredLeft);
        if (! slicer.guiHasLoop.load())
        {
            g.setColour (col::dim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("listening... (plays the previous loop once one is captured)", grid.withHeight (14).translated (0, -16).toNearestInt(),
                        juce::Justification::centredRight);
        }

        static const char* names[] = { "PITCH", "REVERSE", "PAN", "GATE", "START" };
        const float cw = grid.getWidth() / 16.0f, rh = grid.getHeight() / 5.0f;
        const int steps = juce::jmax (1, slicer.guiSteps.load());
        const int st = slicer.guiStep.load();
        const int playCol = st < 0 ? -1 : (st * 16) / steps;

        for (int lane = 0; lane < 5; ++lane)
        {
            auto row = juce::Rectangle<float> (grid.getX(), grid.getY() + lane * rh, grid.getWidth(), rh);
            g.setColour (col::dim);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (names[lane], juce::Rectangle<float> (r.getX() + 8.0f, row.getY(), grid.getX() - r.getX() - 12.0f, rh), juce::Justification::centredRight);

            for (int s = 0; s < 16; ++s)
            {
                auto cell = juce::Rectangle<float> (grid.getX() + s * cw, row.getY(), cw, rh).reduced (1.5f);
                g.setColour (s == playCol ? col::panelHi.brighter (0.2f) : (s % 4 == 0 ? col::panelHi.brighter (0.05f) : col::panelHi));
                g.fillRoundedRectangle (cell, 2.0f);

                const float v = lanes.v[lane][s].load();
                const float lo = dc6::SlicerLanes::minOf (lane), hi = dc6::SlicerLanes::maxOf (lane);
                const float norm = (v - lo) / (hi - lo);
                const auto c = s == playCol ? col::text : col::slicer;
                if (lane == dc6::SlicerLanes::Reverse)
                {
                    if (v > 0.5f) { g.setColour (c); g.fillRoundedRectangle (cell.reduced (3.0f), 2.0f); }
                }
                else if (lane == dc6::SlicerLanes::Pitch || lane == dc6::SlicerLanes::Pan)
                {
                    const float mid = cell.getCentreY();
                    const float y = cell.getBottom() - norm * cell.getHeight();
                    g.setColour (c.withAlpha (0.85f));
                    g.fillRect (cell.getX() + 2.0f, juce::jmin (mid, y), cell.getWidth() - 4.0f, juce::jmax (1.5f, std::abs (y - mid)));
                    if (lane == dc6::SlicerLanes::Pitch && std::abs (v) > 0.01f)
                    {
                        g.setColour (col::text);
                        g.setFont (juce::FontOptions (9.0f));
                        g.drawText (juce::String (juce::roundToInt (v)), cell, juce::Justification::centredTop);
                    }
                }
                else
                {
                    const float hgt = norm * cell.getHeight();
                    g.setColour (c.withAlpha (0.8f));
                    g.fillRect (cell.getX() + 2.0f, cell.getBottom() - hgt, cell.getWidth() - 4.0f, hgt);
                }
            }
        }
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("drag to draw  -  double-click resets a cell", grid.withY (grid.getBottom() + 2).withHeight (14).toNearestInt(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (24);
        clearBtn.setBounds (top.removeFromRight (64).reduced (0, 1));
        top.removeFromRight (6);
        randomBtn.setBounds (top.removeFromRight (92).reduced (0, 1));
        r.removeFromTop (18);
        r.removeFromBottom (16);
        grid = r.withTrimmedLeft (64).toFloat();
    }

    void mouseDown (const juce::MouseEvent& e) override { lastRevStep = -1; edit (e, true); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e, false); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        int lane, step;
        if (hit (e.position, lane, step)) { lanes.v[lane][step] = dc6::SlicerLanes::defOf (lane); repaint(); }
    }

private:
    bool hit (juce::Point<float> p, int& lane, int& step) const
    {
        if (! grid.contains (p)) return false;
        step = juce::jlimit (0, 15, (int) ((p.x - grid.getX()) / (grid.getWidth() / 16.0f)));
        lane = juce::jlimit (0, 4, (int) ((p.y - grid.getY()) / (grid.getHeight() / 5.0f)));
        return true;
    }

    void edit (const juce::MouseEvent& e, bool first)
    {
        if (first)
        {
            int l, s;
            if (! hit (e.position, l, s)) { dragLane = -1; return; }
            dragLane = l;
        }
        if (dragLane < 0) return;
        const float rh = grid.getHeight() / 5.0f;
        const float x = juce::jlimit (grid.getX(), grid.getRight() - 1.0f, e.position.x);
        const int step = juce::jlimit (0, 15, (int) ((x - grid.getX()) / (grid.getWidth() / 16.0f)));
        const float top = grid.getY() + dragLane * rh;
        const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - top) / rh);
        const float lo = dc6::SlicerLanes::minOf (dragLane), hi = dc6::SlicerLanes::maxOf (dragLane);

        if (dragLane == dc6::SlicerLanes::Reverse)
        {
            if (step == lastRevStep) return;
            if (first) revPaint = lanes.v[dragLane][step].load() < 0.5f ? 1.0f : 0.0f;
            lanes.v[dragLane][step] = revPaint;
            lastRevStep = step;
        }
        else
        {
            float v = lo + norm * (hi - lo);
            if (dragLane == dc6::SlicerLanes::Pitch) v = std::round (v);     // semitones
            lanes.v[dragLane][step] = v;
        }
        repaint();
    }

    void randomize()
    {
        juce::Random rnd;
        for (int s = 0; s < 16; ++s)
        {
            lanes.v[dc6::SlicerLanes::Pitch][s] = rnd.nextFloat() < 0.25f ? (float) (rnd.nextInt (13) - 7) : 0.0f;
            lanes.v[dc6::SlicerLanes::Reverse][s] = rnd.nextFloat() < 0.2f ? 1.0f : 0.0f;
            lanes.v[dc6::SlicerLanes::Pan][s] = rnd.nextFloat() < 0.4f ? rnd.nextFloat() * 1.6f - 0.8f : 0.0f;
            lanes.v[dc6::SlicerLanes::Gate][s] = rnd.nextFloat() < 0.3f ? 0.25f + rnd.nextFloat() * 0.6f : 1.0f;
            lanes.v[dc6::SlicerLanes::Start][s] = rnd.nextFloat() < 0.2f ? rnd.nextFloat() * 0.6f : 0.0f;
        }
        repaint();
    }

    dc6::SlicerLanes& lanes;
    dc6::Slicer& slicer;
    std::atomic<float>* anarchy;
    juce::TextButton randomBtn, clearBtn;
    juce::Rectangle<float> grid;
    int dragLane = -1, lastRevStep = -1;
    float revPaint = 1.0f;
};

// ---------------------------------------------------------------- chaos engine scope: S&H trace + Euclid ring
class ChaosView : public juce::Component
{
public:
    ChaosView (dc6::ChaosEngine& e, APVTS& s) : engine (e), state (s) { history.fill (0.0f); }

    void tick()
    {
        history[(size_t) head] = engine.guiValue.load();
        head = (head + 1) % (int) history.size();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (col::chaos);
        g.fillRect (r.getX() + 10.0f, r.getY() + 11.0f, 3.0f, 12.0f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText ("LIVE MODULATION", (int) r.getX() + 18, (int) r.getY() + 6, 200, 22, juce::Justification::centredLeft);

        auto body = r.reduced (12.0f).withTrimmedTop (24.0f);
        auto ring = body.removeFromRight (juce::jmin (body.getHeight(), body.getWidth() * 0.45f)).reduced (6.0f);
        auto scope = body.reduced (4.0f);

        // S&H trace
        g.setColour (col::panelHi);
        g.fillRoundedRectangle (scope, 4.0f);
        g.setColour (col::border);
        g.drawHorizontalLine ((int) scope.getCentreY(), scope.getX(), scope.getRight());
        juce::Path p;
        const int N = (int) history.size();
        for (int i = 0; i < N; ++i)
        {
            const float v = history[(size_t) ((head + i) % N)];
            const float x = scope.getX() + scope.getWidth() * i / (N - 1);
            const float y = scope.getCentreY() - v * scope.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::chaos);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("SAMPLE & HOLD", scope.reduced (6.0f).toNearestInt(), juce::Justification::topLeft);

        // Euclid ring
        const int steps = (int) state.getRawParameterValue ("euSteps")->load();
        const int pulses = (int) state.getRawParameterValue ("euPulses")->load();
        const int rot = (int) state.getRawParameterValue ("euRot")->load();
        const int cur = engine.guiStep.load();
        const auto c = ring.getCentre();
        const float rad = ring.getWidth() * 0.4f;
        for (int i = 0; i < steps; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * i / steps - juce::MathConstants<float>::halfPi;
            const float x = c.x + std::cos (a) * rad, y = c.y + std::sin (a) * rad;
            const bool hit = dc6::euclidHit (i, steps, pulses, rot);
            const float dot = i == cur ? 9.0f : 6.5f;
            g.setColour (hit ? (i == cur ? col::text : col::chaos) : (i == cur ? col::dim : col::border));
            g.fillEllipse (x - dot, y - dot, dot * 2, dot * 2);
        }
        g.setColour (col::dim);
        g.drawText (juce::String (pulses) + " / " + juce::String (steps), ring.withSizeKeepingCentre (60, 16).toNearestInt(), juce::Justification::centred);
    }

private:
    dc6::ChaosEngine& engine;
    APVTS& state;
    std::array<float, 120> history {};
    int head = 0;
};
