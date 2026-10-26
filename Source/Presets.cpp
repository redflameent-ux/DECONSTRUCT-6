#include "Presets.h"
#include "PluginProcessor.h"

using L = dc6::SlicerLanes;

// ------------------------------------------------------------------------ step-grid patterns
static void lanePitchDrops (L& l)    { for (int s : { 2, 6, 10, 14 }) l.v[L::Pitch][s] = -3.0f; l.v[L::Pitch][15] = -7.0f; }
static void lanePocketGates (L& l)   { for (int s = 1; s < 16; s += 2) l.v[L::Gate][s] = 0.55f; l.v[L::Pitch][12] = -5.0f; }
static void laneTurnaround (L& l)    { l.v[L::Reverse][7] = 1; l.v[L::Reverse][15] = 1; for (int s = 0; s < 16; ++s) l.v[L::Pan][s] = (s % 2 ? 0.45f : -0.45f); }
static void laneScatter (L& l)
{
    const float pans[16] = { 0, -0.8f, 0.7f, -0.3f, 0.9f, -0.9f, 0.2f, 0.6f, -0.6f, 0.4f, -0.2f, 0.8f, -0.7f, 0.1f, 0.5f, -0.5f };
    for (int s = 0; s < 16; ++s) l.v[L::Pan][s] = pans[s];
    for (int s : { 3, 7, 11, 14 }) l.v[L::Reverse][s] = 1;
    for (int s : { 5, 13 }) l.v[L::Pitch][s] = 7.0f;
    for (int s : { 6, 15 }) l.v[L::Gate][s] = 0.3f;
}
static void laneFuneral (L& l)       { for (int s = 0; s < 16; ++s) { l.v[L::Pitch][s] = -2.0f - (s % 4 == 3 ? 5.0f : 0.0f); l.v[L::Gate][s] = s % 4 == 0 ? 1.0f : 0.7f; } }
static void laneStarts (L& l)        { for (int s = 0; s < 16; ++s) l.v[L::Start][s] = (s % 3) * 0.25f; l.v[L::Reverse][10] = 1; }

const std::vector<FactoryPreset>& PresetManager::factory()
{
    using namespace pid;
    static const std::vector<FactoryPreset> list =
    {
        { "00 Init", "Init", {}, nullptr },

        // ================================================================ 01 Memphis Sub Tapes
        { "01 Memphis Sub Tapes", "01_MEMPHIS_HEAVY_808",
          { { subLvl, 1.0f }, { subTune, 45 }, { subDecay, 0.9f }, { subDrop, 0.4f }, { subSens, 0.6f },
            { bits, 12 }, { srate, 26040 }, { preamp, 0.45f }, { hiss, 0.12f }, { flutter, 0.12f },
            { harm, 0.35f }, { subBoost, 4 }, { smash, 0.35f }, { mSat, 0.25f }, { bDrive, 0.15f },
            { autoGlitch, 0 }, { chaos, 0 } }, nullptr },
        { "01 Memphis Sub Tapes", "Drank Tape Crawl",
          { { flutter, 0.45f }, { preamp, 0.4f }, { bits, 12 }, { srate, 22050 }, { hiss, 0.25f },
            { subLvl, 0.5f }, { subTune, 41 }, { subDecay, 0.6f }, { chaos, 0.1f }, { slOn, 1 }, { slDecay, 0.5f },
            { bDrive, 0.15f }, { smash, 0.35f }, { mSat, 0.3f }, { subBoost, 3 }, { chorus, 0.1f } }, lanePitchDrops },
        { "01 Memphis Sub Tapes", "Cassette Dub 1995",
          { { bits, 10 }, { srate, 16000 }, { hiss, 0.4f }, { flutter, 0.6f }, { preamp, 0.5f },
            { cutoff, 7000 }, { reso, 0.1f }, { smash, 0.3f }, { grime, 0.2f } }, nullptr },
        { "01 Memphis Sub Tapes", "Half-Speed Basement",
          { { flutter, 0.3f }, { stopTime, 1.2f }, { bits, 12 }, { srate, 26000 }, { subLvl, 0.6f }, { subTune, 38 },
            { harm, 0.4f }, { subBoost, 4 }, { chaos, 0.15f }, { slOn, 1 }, { slDecay, 0.7f }, { latch, 1 } }, lanePitchDrops },
        { "01 Memphis Sub Tapes", "Cowbell Sub Pocket",
          { { slOn, 1 }, { chaos, 0.2f }, { slDecay, 0.35f }, { subLvl, 0.7f }, { subTune, 49 }, { subDecay, 0.9f },
            { subDrop, 0.5f }, { harm, 0.35f }, { bits, 12 }, { srate, 22050 }, { smash, 0.45f } }, lanePocketGates },
        { "01 Memphis Sub Tapes", "Purple Wind-Down",
          { { stopTime, 2.0f }, { stopCurve, 0.7f }, { flutter, 0.5f }, { preamp, 0.3f }, { hiss, 0.15f },
            { srate, 18000 }, { bits, 11 }, { mSat, 0.3f } }, nullptr },

        // ================================================================ 02 Industrial Clipping
        { "02 Industrial Clipping", "02_TRENT_CORROSION",
          { { combMix, 0.6f }, { combFb, 0.95f }, { combFreq, 150 }, { bFold, 0.7f }, { clipMode, 3 }, { bDrive, 0.6f },
            { cutoff, 6000 }, { reso, 0.5f }, { gateOn, 1 }, { gatePat, 0 }, { gateThr, -40 }, { gateDuty, 0.6f },
            { gateRate, 1 }, { gateDepth, 0.92f }, { gateSC, 1 }, { smash, 0.45f }, { autoLvl, 1 } }, nullptr },
        { "02 Industrial Clipping", "Broken Console",
          { { bDrive, 0.7f }, { clipMode, 2 }, { bAsym, 0.4f }, { fDrive, 0.4f }, { snap, 0.5f }, { smash, 0.5f },
            { mSat, 0.3f }, { cutoff, 9000 }, { reso, 0.3f } }, nullptr },
        { "02 Industrial Clipping", "Diode Furnace",
          { { bDrive, 0.85f }, { clipMode, 0 }, { bFold, 0.2f }, { combMix, 0.25f }, { combFreq, 220 }, { combFb, 0.75f },
            { cutoff, 5000 }, { reso, 0.55f }, { smash, 0.4f } }, nullptr },
        { "02 Industrial Clipping", "Scrap Metal Gate",
          { { gateOn, 1 }, { gatePat, 1 }, { gateRate, 2 }, { gateDuty, 0.35f }, { gateThr, -60 }, { bDrive, 0.55f }, { clipMode, 1 },
            { combMix, 0.45f }, { combFreq, 640 }, { combFb, 0.85f }, { ringAmt, 0.3f }, { ringFreq, 330 } }, nullptr },
        { "02 Industrial Clipping", "Self-Oscillate Scream",
          { { cutoff, 900 }, { reso, 0.95f }, { bDrive, 0.5f }, { clipMode, 3 }, { bFold, 0.4f }, { chaos, 0.4f },
            { chMod, 0.8f }, { modCut, 0.8f }, { chRate, 2 }, { smash, 0.3f } }, nullptr },
        { "02 Industrial Clipping", "Reese Grinder",
          { { xover, 110 }, { harm, 0.5f }, { fDrive, 0.7f }, { fFold, 0.5f }, { chorus, 0.7f }, { smear, 0.5f },
            { ringAmt, 0.1f }, { bDrive, 0.3f }, { clipMode, 2 }, { snap, 0.4f }, { subBoost, 3 } }, nullptr },

        // ================================================================ 03 Drill IDM Matrix
        { "03 Drill IDM Matrix", "03_APHEX_DRILL_CORE",
          { { stutRate, 2 }, { autoGlitch, 0.45f }, { modStut, 0.6f }, { riseAmt, 0.8f }, { granMix, 0.35f },
            { granRev, 0.8f }, { granSpread, 1.0f }, { granSize, 35 }, { chaos, 0.75f }, { chMod, 0.5f },
            { slOn, 1 }, { euPulses, 7 } }, laneScatter },
        { "03 Drill IDM Matrix", "Squarepusher Turnaround",
          { { slOn, 1 }, { chaos, 0.55f }, { slSeed, 21 }, { slDecay, 0.1f }, { autoGlitch, 0.1f }, { stutRate, 1 } }, laneTurnaround },
        { "03 Drill IDM Matrix", "Micro-Roll Machine",
          { { chaos, 0.45f }, { autoGlitch, 0.35f }, { modStut, 0.7f }, { euPulses, 7 }, { granMix, 0.1f }, { stutRate, 4 }, { chMod, 0.7f } }, nullptr },
        { "03 Drill IDM Matrix", "Granular Snowstorm",
          { { granMix, 0.7f }, { granSize, 40 }, { granDens, 35 }, { granRev, 0.6f }, { granSpread, 1.0f },
            { chaos, 0.5f }, { modGran, 0.4f }, { chMod, 0.5f } }, nullptr },
        { "03 Drill IDM Matrix", "Audio-Rate Buzz",
          { { slOn, 1 }, { chaos, 1.0f }, { slSlices, 1 }, { euPulses, 9 }, { autoGlitch, 0.45f }, { stutRate, 3 },
            { bDrive, 0.4f }, { clipMode, 0 } }, laneScatter },
        { "03 Drill IDM Matrix", "Offset Breaks",
          { { slOn, 1 }, { chaos, 0.38f }, { slSeed, 44 }, { slDetect, 0.8f }, { autoGlitch, 0.05f }, { snap, 0.6f } }, laneStarts },

        // ================================================================ 04 Hybrid Anarchy
        { "04 Hybrid Anarchy", "04_TOTAL_DECONSTRUCTION",
          { { chaos, 0.92f }, { chMod, 0.9f }, { slOn, 1 }, { subLvl, 0.5f }, { grime, 0.35f }, { bDrive, 0.55f },
            { bFold, 0.3f }, { combMix, 0.25f }, { gateOn, 1 }, { gatePat, 3 }, { granMix, 0.3f }, { stutRate, 4 },
            { modCut, 0.8f }, { modDrive, 0.6f }, { modGran, 0.5f }, { smash, 0.45f } }, laneScatter },
        { "04 Hybrid Anarchy", "Memphis Machine Funeral",
          { { chaos, 0.35f }, { slOn, 1 }, { slDecay, 0.6f }, { bits, 12 }, { srate, 22050 }, { flutter, 0.35f },
            { bDrive, 0.5f }, { clipMode, 1 }, { gateOn, 1 }, { gatePat, 1 }, { gateRate, 1 }, { gateDuty, 0.6f },
            { subLvl, 0.6f }, { subTune, 43 } }, laneFuneral },
        { "04 Hybrid Anarchy", "Bone Saw Sermon",
          { { fDrive, 0.6f }, { fFold, 0.6f }, { ringAmt, 0.4f }, { ringFreq, 90 }, { combMix, 0.35f }, { combFreq, 110 },
            { combFb, -0.8f }, { reso, 0.6f }, { cutoff, 2500 }, { chaos, 0.6f }, { chMod, 0.7f }, { modCut, 0.7f }, { granMix, 0.15f } }, nullptr },
        { "04 Hybrid Anarchy", "Skeleton Key",
          { { gateOn, 1 }, { gatePat, 3 }, { euPulses, 6 }, { euRot, 2 }, { chaos, 0.6f }, { autoGlitch, 0.15f },
            { modStut, 0.5f }, { flutter, 0.3f }, { bits, 10 }, { srate, 14000 }, { granMix, 0.1f } }, nullptr },
        { "04 Hybrid Anarchy", "Cathedral of Static",
          { { harm, 0.4f }, { chorus, 0.5f }, { smear, 0.6f }, { flutter, 0.25f }, { granMix, 0.35f }, { granSize, 120 },
            { granDens, 10 }, { chaos, 0.4f }, { bDrive, 0.25f }, { smash, 0.3f }, { mSat, 0.25f }, { hiss, 0.2f } }, nullptr },
    };
    return list;
}

// ------------------------------------------------------------------------ manager
PresetManager::PresetManager (DC6Processor& p) : proc (p) { refresh(); }

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("DECONSTRUCT-6").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clear();
    const auto& f = factory();
    for (int i = 0; i < (int) f.size(); ++i)
        list.add ({ f[(size_t) i].bank, f[(size_t) i].name, i, {} });

    auto folder = userFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.dc6preset");
        files.sort();
        for (auto& file : files)
            list.add ({ "05 User", file.getFileNameWithoutExtension(), -1, file });
    }
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == name) { current = i; break; }
}

void PresetManager::resetToDefaults()
{
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());
    proc.lanes.reset();
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size())) return;
    const auto& e = list.getReference (index);

    if (e.factoryIndex >= 0)
    {
        const auto& fp = factory()[(size_t) e.factoryIndex];
        resetToDefaults();
        for (auto& [id, value] : fp.values)
            if (auto* r = proc.apvts.getParameter (id))
                r->setValueNotifyingHost (r->convertTo0to1 (value));
        if (fp.lanes) fp.lanes (proc.lanes);
    }
    else
    {
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (e.file));
        if (xml == nullptr) return;
        resetToDefaults();
        for (auto* prm : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (xml->hasAttribute (r->getParameterID()))
                    r->setValueNotifyingHost (r->convertTo0to1 ((float) xml->getDoubleAttribute (r->getParameterID())));
        proc.lanesFromXml (xml->getChildByName ("LANES"));
    }
    // pads always come back released
    for (auto* id : pads::ids)
        if (auto* r = proc.apvts.getParameter (id)) r->setValueNotifyingHost (0.0f);

    current = index;
    name = e.name;
    if (onChange) onChange();
}

bool PresetManager::saveUser (const juce::String& newName)
{
    auto clean = juce::File::createLegalFileName (newName.trim());
    if (clean.isEmpty()) return false;
    auto folder = userFolder();
    folder.createDirectory();

    juce::XmlElement xml ("DC6PRESET");
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            xml.setAttribute (r->getParameterID(), r->convertFrom0to1 (r->getValue()));
    xml.addChildElement (proc.lanesToXml());
    if (! xml.writeTo (folder.getChildFile (clean + ".dc6preset"))) return false;

    name = clean;
    refresh();
    if (onChange) onChange();
    return true;
}

void PresetManager::setCurrentName (const juce::String& n)
{
    name = n;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == n) { current = i; break; }
    if (onChange) onChange();
}
