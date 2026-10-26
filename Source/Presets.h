#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Slicer.h"

class DC6Processor;

struct FactoryPreset
{
    juce::String bank, name;
    std::vector<std::pair<const char*, float>> values;       // real (not 0..1) values; anything left out = default
    std::function<void (dc6::SlicerLanes&)> lanes;            // optional step-grid pattern
};

class PresetManager
{
public:
    struct Entry { juce::String bank, name; int factoryIndex = -1; juce::File file; };

    explicit PresetManager (DC6Processor& p);

    void refresh();
    const juce::Array<Entry>& entries() const { return list; }
    void load (int index);
    void next() { if (list.size()) load ((current + 1) % list.size()); }
    void prev() { if (list.size()) load ((current - 1 + list.size()) % list.size()); }
    bool saveUser (const juce::String& name);

    int currentIndex() const { return current; }
    juce::String currentName() const { return name; }
    void setCurrentName (const juce::String& n);

    static juce::File userFolder();
    static const std::vector<FactoryPreset>& factory();

    std::function<void()> onChange;

private:
    void resetToDefaults();

    DC6Processor& proc;
    juce::Array<Entry> list;
    int current = 0;
    juce::String name { "Init" };
};
