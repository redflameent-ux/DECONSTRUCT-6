#pragma once
#include "PluginProcessor.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"
#include "gui/Views.h"

// signal-flow strip that doubles as the page tabs
class FlowStrip : public juce::Component
{
public:
    struct Block { juce::String name, sub, powerId; juce::Colour colour; };
    FlowStrip (APVTS& s, std::vector<Block> b) : state (s), blocks (std::move (b)) {}

    std::function<void (int)> onSelect;
    int selected = 2;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> blockRect (int i) const;
    juce::Rectangle<float> ledRect (int i) const;
    APVTS& state;
    std::vector<Block> blocks;
};

class DC6Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DC6Editor (DC6Processor&);
    ~DC6Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1120, baseH = 712;

private:
    void timerCallback() override;
    void showPage (int index);
    void showPresetMenu();
    void savePreset();
    void buildPages();
    void buildFrontPanel();

    DC6Processor& proc;
    DC6LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    } root;

    // header
    juce::TextButton presetName, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" };
    std::unique_ptr<Knob> inKnob, outKnob;
    juce::Rectangle<int> logoArea, meterArea;

    // flow + pages
    std::unique_ptr<FlowStrip> flow;
    std::vector<std::unique_ptr<ParamPage>> pages;
    std::unique_ptr<StepGrid> stepGrid;
    std::unique_ptr<ChaosView> chaosView;
    struct MidiChart : juce::Component { void paint (juce::Graphics&) override; } midiChart;
    int current = 2;

    // front panel
    struct Group { juce::String title; juce::Colour colour; std::vector<juce::Component*> ctrls; juce::Rectangle<int> box; int big = -1; };
    std::vector<Group> groups;
    std::vector<std::unique_ptr<juce::Component>> frontOwned;
    std::vector<std::unique_ptr<PadButton>> padBtns;
    std::unique_ptr<PadButton> tapeSlow;
    std::unique_ptr<Toggle> latch;
    juce::Rectangle<int> padArea;

    juce::String shownPreset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DC6Editor)
};
