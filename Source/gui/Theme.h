#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace col
{
    const juce::Colour bg      { 0xff09090b };
    const juce::Colour panel   { 0xff131317 };
    const juce::Colour panelHi { 0xff1c1c22 };
    const juce::Colour border  { 0xff2a2a33 };
    const juce::Colour text    { 0xffe9e3d3 };   // bone
    const juce::Colour dim     { 0xff8a857a };
    const juce::Colour blood   { 0xffd2302c };

    // one accent per module, in signal-flow order
    const juce::Colour slicer  { 0xffe0a23a };   // amber
    const juce::Colour split   { 0xff3fb6a8 };   // teal
    const juce::Colour tape    { 0xff9b5de5 };   // Memphis purple
    const juce::Colour indus   { 0xffd94a2b };   // rust
    const juce::Colour glitch  { 0xff29d3f0 };   // cyan
    const juce::Colour chaos   { 0xffff3d8b };   // magenta
    const juce::Colour master  { 0xffe9e3d3 };   // bone
}

class DC6LookAndFeel : public juce::LookAndFeel_V4
{
public:
    DC6LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panelHi);
        setColour (juce::ComboBox::outlineColourId, col::border);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::dim);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::headerTextColourId, col::dim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::panelHi);
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::TextEditor::backgroundColourId, col::panelHi);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::border);
        setColour (juce::TextButton::buttonColourId, col::panelHi);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxBackgroundColourId, col::panelHi);
        setColour (juce::Slider::textBoxOutlineColourId, col::border);
        setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
        setColour (juce::TooltipWindow::textColourId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float rad = size * 0.5f;
        const float ang = start + pos * (end - start);

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, rad - 2.0f, rad - 2.0f, 0.0f, start, end, true);
        g.setColour (col::border);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc (bipolar knobs fill from the middle)
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? (start + end) * 0.5f : start;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad - 2.0f, rad - 2.0f, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (accent.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // body
        const float body = rad - 7.0f;
        juce::ColourGradient grad (col::panelHi.brighter (0.15f), c.x, c.y - body, col::bg, c.x, c.y + body, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
        g.setColour (col::border.brighter (0.2f));
        g.drawEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f, 1.0f);

        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-1.5f, -body + 3.0f, 3.0f, body * 0.55f, 1.5f);
        g.setColour (col::text);
        g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        auto r = b.getLocalBounds().toFloat();
        auto pill = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.85f) : col::panelHi);
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (over ? col::dim : col::border);
        g.drawRoundedRectangle (pill, 8.0f, 1.0f);
        const float kx = on ? pill.getRight() - 14.0f : pill.getX() + 2.0f;
        g.setColour (on ? col::bg : col::dim);
        g.fillEllipse (kx, pill.getY() + 2.0f, 12.0f, 12.0f);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? col::text : col::dim);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (b.getButtonText(), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
        }
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panelHi);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::dim : col::border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (12.5f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.5f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::border : (over ? col::panelHi.brighter (0.08f) : col::panelHi));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.5f, juce::Font::bold); }
};
