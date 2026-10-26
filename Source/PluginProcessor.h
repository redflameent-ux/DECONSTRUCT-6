#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <string_view>
#include <map>
#include "Params.h"
#include "dsp/Common.h"
#include "dsp/Slicer.h"
#include "dsp/SplitStage.h"
#include "dsp/TapeEngine.h"
#include "dsp/IndustrialEngine.h"
#include "dsp/GlitchEngine.h"
#include "dsp/ChaosEngine.h"
#include "dsp/Master.h"

class PresetManager;

class DC6Processor : public juce::AudioProcessor
{
public:
    DC6Processor();
    ~DC6Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DECONSTRUCT-6"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // lanes <-> XML (used by state + presets)
    juce::XmlElement* lanesToXml() const;
    void lanesFromXml (const juce::XmlElement* xml);

    juce::AudioProcessorValueTreeState apvts;
    dc6::SlicerLanes lanes;
    std::unique_ptr<PresetManager> presets;

    // ---- read by the GUI
    dc6::Slicer slicer;
    dc6::ChaosEngine chaosEngine;
    std::atomic<float> meterIn { 0 }, meterOut { 0 }, meterGr { 0 }, meterSub { 0 }, meterGate { 1 };
    std::atomic<int>   guiStutter { 0 };
    std::atomic<bool>  guiTapeStopped { false };
    std::atomic<bool>  midiPadHeld[8];

private:
    float p (const char* id) const { return params.find (std::string_view (id))->second->load (std::memory_order_relaxed); }
    bool  b (const char* id) const { return p (id) > 0.5f; }

    std::map<std::string, std::atomic<float>*, std::less<>> params;   // heterogeneous lookup: no allocation on the audio thread

    dc6::SplitStage split;
    dc6::TapeTone tapeTone;
    dc6::SubResynth subSynth;
    dc6::TapeTransport transport;
    dc6::IndustrialTone industrial;
    dc6::PumpGate gate;
    dc6::Scrambler scrambler;
    dc6::Stutter stutter;
    dc6::Master master;

    juce::AudioBuffer<float> work, low, ref, dry;
    std::vector<float> dryDelay[2];
    int dryDelayW = 0, latency = 0;
    double sr = 44100.0;
    double internalPpq = 0.0;
    bool midiPad[8] {};
    juce::SmoothedValue<float> mixSm, inSm, outSm;
    dc6::Rng rng;
};
