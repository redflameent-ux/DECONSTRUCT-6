#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

// ---------------------------------------------------------------- rotary knob with name + value
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& id, juce::Colour accent, const juce::String& label = {})
    {
        auto* prm = state.getParameter (id);
        name = label.isNotEmpty() ? label : prm->getName (32);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setTooltip (prm->getName (64));
        addAndMakeVisible (slider);
        att = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        slider.onValueChange = [this] { repaint(); };
        param = prm;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        auto txt = r.removeFromBottom (30);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawFittedText (name.toUpperCase(), txt.removeFromTop (15), juce::Justification::centred, 1, 0.8f);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (param->getCurrentValueAsText(), txt, juce::Justification::centredTop, 1, 0.8f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (30);
        slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())));
    }

    juce::Slider slider;

private:
    juce::String name;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// ---------------------------------------------------------------- on/off switch with caption
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& id, juce::Colour accent, const juce::String& label = {})
    {
        auto* prm = state.getParameter (id);
        name = label.isNotEmpty() ? label : prm->getName (32);
        button.setColour (juce::ToggleButton::tickColourId, accent);
        button.setTooltip (prm->getName (64));
        addAndMakeVisible (button);
        att = std::make_unique<APVTS::ButtonAttachment> (state, id, button);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawFittedText (name.toUpperCase(), getLocalBounds().removeFromBottom (30).removeFromTop (15), juce::Justification::centred, 1, 0.8f);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (30);
        button.setBounds (r.withSizeKeepingCentre (34, 22));
    }
    juce::ToggleButton button;

private:
    juce::String name;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// ---------------------------------------------------------------- drop-down with caption
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& id, const juce::String& label = {})
    {
        auto* prm = state.getParameter (id);
        name = label.isNotEmpty() ? label : prm->getName (32);
        box.addItemList (prm->getAllValueStrings(), 1);
        box.setTooltip (prm->getName (64));
        addAndMakeVisible (box);
        att = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawFittedText (name.toUpperCase(), getLocalBounds().removeFromBottom (30).removeFromTop (15), juce::Justification::centred, 1, 0.8f);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (30);
        box.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 92), 24));
    }
    juce::ComboBox box;

private:
    juce::String name;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// pick the right widget for a parameter
inline std::unique_ptr<juce::Component> makeControl (APVTS& state, const juce::String& id, juce::Colour accent, const juce::String& label = {})
{
    auto* prm = state.getParameter (id);
    if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)   return std::make_unique<Toggle> (state, id, accent, label);
    if (dynamic_cast<juce::AudioParameterChoice*> (prm) != nullptr) return std::make_unique<Choice> (state, id, label);
    return std::make_unique<Knob> (state, id, accent, label);
}

// ---------------------------------------------------------------- a page made of titled boxes of controls
class ParamPage : public juce::Component
{
public:
    ParamPage (APVTS& s, juce::Colour a) : state (s), accent (a) {}

    void addSection (const juce::String& title, std::initializer_list<const char*> ids, int columns)
    {
        Section sec;
        sec.title = title;
        sec.cols = columns;
        for (auto* id : ids)
        {
            auto c = makeControl (state, id, accent);
            addAndMakeVisible (*c);
            sec.ctrls.push_back (c.get());
            owned.push_back (std::move (c));
        }
        sections.push_back (std::move (sec));
    }

    // a custom component that takes the right-hand side of the page
    void setExtra (juce::Component* c, int width) { extra = c; extraWidth = width; if (c) addAndMakeVisible (c); }

    void paint (juce::Graphics& g) override
    {
        for (auto& s : sections)
        {
            auto r = s.box.toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::border);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (accent);
            g.fillRect (r.getX() + 10.0f, r.getY() + 11.0f, 3.0f, 12.0f);
            g.setColour (col::text);
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (s.title.toUpperCase(), (int) r.getX() + 18, (int) r.getY() + 6, (int) r.getWidth() - 24, 22, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        if (extra != nullptr)
        {
            extra->setBounds (r.removeFromRight (extraWidth));
            r.removeFromRight (8);
        }
        int totalCols = 0;
        for (auto& s : sections) totalCols += s.cols;
        const int gaps = 8 * juce::jmax (0, (int) sections.size() - 1);
        const float colW = (float) (r.getWidth() - gaps) / juce::jmax (1, totalCols);

        int x = r.getX();
        for (auto& s : sections)
        {
            const int w = (int) std::round (colW * s.cols);
            s.box = { x, r.getY(), w, r.getHeight() };
            x += w + 8;

            auto inner = s.box.reduced (6).withTrimmedTop (24);
            const int rows = juce::jmax (1, ((int) s.ctrls.size() + s.cols - 1) / s.cols);
            const int cw = inner.getWidth() / s.cols, ch = juce::jmin (inner.getHeight() / rows, 120);
            const int yOff = (inner.getHeight() - ch * rows) / 2;
            for (int i = 0; i < (int) s.ctrls.size(); ++i)
                s.ctrls[(size_t) i]->setBounds (inner.getX() + (i % s.cols) * cw, inner.getY() + yOff + (i / s.cols) * ch, cw, ch);
        }
    }

private:
    struct Section { juce::String title; int cols = 2; std::vector<juce::Component*> ctrls; juce::Rectangle<int> box; };
    APVTS& state;
    juce::Colour accent;
    std::vector<Section> sections;
    std::vector<std::unique_ptr<juce::Component>> owned;
    juce::Component* extra = nullptr;
    int extraWidth = 0;
};

// ---------------------------------------------------------------- performance pad (momentary or latched)
class PadButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    PadButton (APVTS& s, const juce::String& padId, const juce::String& lbl, const juce::String& note, juce::Colour a)
        : state (s), id (padId), label (lbl), noteName (note), accent (a)
    {
        param = state.getParameter (id);
        latchParam = state.getRawParameterValue ("latch");
        setTooltip (lbl + "  (MIDI " + note + ")");
    }

    void setExternallyHeld (bool h) { if (h != midiHeld) { midiHeld = h; repaint(); } }
    bool isOn() const { return param->getValue() > 0.5f || midiHeld; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool on = isOn();
        g.setColour (on ? accent : col::panelHi);
        g.fillRoundedRectangle (r, 6.0f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (1.5f), 7.0f, 2.0f);
        }
        g.setColour (on ? col::bg : col::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (on ? col::bg : col::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawFittedText (label, r.toNearestInt().withTrimmedBottom (12), juce::Justification::centred, 1);
        g.setColour (on ? col::bg.withAlpha (0.7f) : col::dim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (noteName, r.toNearestInt().removeFromBottom (16), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        const bool latch = latchParam->load() > 0.5f;
        set (latch ? param->getValue() < 0.5f : true);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (latchParam->load() <= 0.5f) set (false);
    }

private:
    void set (bool on)
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (on ? 1.0f : 0.0f);
        param->endChangeGesture();
        repaint();
    }

    APVTS& state;
    juce::String id, label, noteName;
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    std::atomic<float>* latchParam = nullptr;
    bool midiHeld = false;
};
