// Offline test: renders a synthetic drum + bass loop through every factory preset (and every pad),
// checks for silence, NaN/inf, overs and CPU cost, and writes a few WAVs to listen to.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <chrono>

struct FakeHead : juce::AudioPlayHead
{
    double bpm = 140.0, ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static juce::AudioBuffer<float> makeLoop (double sr, double bpm, int bars)
{
    const int spb = (int) (sr * 60.0 / bpm);
    const int len = spb * 4 * bars;
    juce::AudioBuffer<float> b (2, len);
    b.clear();
    juce::Random rnd (7);
    float bassPh = 0.0f, lp = 0.0f;
    for (int i = 0; i < len; ++i)
    {
        const int inBeat = i % spb;
        const int beat = (i / spb) % 4;
        const float t = (float) inBeat / (float) sr;
        float s = 0.0f;
        // kick on every beat (plus an off-beat ghost on beat 3)
        s += 0.9f * std::sin (juce::MathConstants<float>::twoPi * (50.0f + 120.0f * std::exp (-t * 30.0f)) * t) * std::exp (-t * 6.0f);
        // snare on 2 and 4
        if (beat == 1 || beat == 3) s += 0.5f * rnd.nextFloat() * 2.0f * std::exp (-t * 18.0f) - 0.25f * std::exp (-t * 18.0f);
        // hats on 8ths
        const int in8 = i % (spb / 2);
        s += 0.15f * (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) in8 / (float) sr * 60.0f);
        // saw bass
        bassPh += 41.2f / (float) sr; if (bassPh > 1.0f) bassPh -= 1.0f;
        lp += 0.05f * ((bassPh * 2.0f - 1.0f) - lp);
        s += 0.35f * lp;
        b.setSample (0, i, s * 0.7f);
        b.setSample (1, i, s * 0.7f + 0.05f * (rnd.nextFloat() - 0.5f));
    }
    return b;
}

struct Stats { float peak = 0, rms = 0; int bad = 0; double rtFactor = 0; };

static Stats render (DC6Processor& p, const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out,
                     int block, double sr, double bpm, std::vector<std::pair<int, int>> padHolds /* (noteIndex, startBar) */)
{
    FakeHead head; head.bpm = bpm;
    p.setPlayHead (&head);
    out.makeCopyOf (in);
    const int spb = (int) (sr * 60.0 / bpm);
    Stats st;
    double energy = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = juce::jmin (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        juce::MidiBuffer midi;
        for (auto [note, bar] : padHolds)
        {
            const int on = bar * 4 * spb, off = on + spb;   // hold for one beat
            if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, 36 + note, (juce::uint8) 100), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, 36 + note), off - pos);
        }
        head.ppq = (double) pos / spb;
        p.processBlock (view, midi);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    const double secs = std::chrono::duration<double> (t1 - t0).count();
    st.rtFactor = (out.getNumSamples() / sr) / secs;

    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const float v = out.getSample (c, i);
            if (! std::isfinite (v)) { ++st.bad; continue; }
            st.peak = juce::jmax (st.peak, std::abs (v));
            energy += v * v;
        }
    st.rms = (float) std::sqrt (energy / (2.0 * out.getNumSamples()));
    p.setPlayHead (nullptr);
    return st;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

#include "../Source/PluginEditor.h"

static int snapshots (const juce::File& dir)
{
    dir.createDirectory();
    DC6Processor p;
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    p.presets->load (13);   // 03_APHEX_DRILL_CORE: lots lit up
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (DC6Editor::baseW, DC6Editor::baseH);
    FlowStrip* flow = nullptr;
    for (auto* c : ed->getChildComponent (0)->getChildren())
        if (auto* f = dynamic_cast<FlowStrip*> (c)) flow = f;
    for (int page = 0; page < 7; ++page)
    {
        if (flow != nullptr && flow->onSelect) { flow->selected = page; flow->onSelect (page); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream os (dir.getChildFile ("page" + juce::String (page) + ".png"));
        os.setPosition (0); os.truncate();
        png.writeImageToStream (img, os);
    }
    printf ("snapshots written to %s\n", dir.getFullPathName().toRawUTF8());
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snap") return snapshots (juce::File (juce::String (argv[2])));
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName() + "/renders");
    outDir.createDirectory();

    int failures = 0;
    auto check = [&] (const juce::String& name, const Stats& s, bool limiterOn)
    {
        const float rmsDb = dc6::gainToDb (s.rms), peakDb = dc6::gainToDb (s.peak);
        juce::String verdict = "ok";
        if (s.bad > 0) verdict = "NaN/inf!";
        else if (rmsDb < -50.0f) verdict = "SILENT?";
        else if (limiterOn && peakDb > 0.05f) verdict = "OVER 0 dB";
        if (verdict != "ok") ++failures;
        printf ("  %-30s peak %6.1f dB  rms %6.1f dB  %5.0fx realtime  %s\n", name.toRawUTF8(), peakDb, rmsDb, s.rtFactor, verdict.toRawUTF8());
    };

    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        for (int block : { 512, 37 })
        {
            if (sr != 48000.0 && block == 37) continue;
            printf ("\n=== %.0f Hz, block %d ===\n", sr, block);
            DC6Processor p;
            p.setRateAndBufferSizeDetails (sr, block);
            p.prepareToPlay (sr, block);
            const double bpm = 140.0;
            auto loop = makeLoop (sr, bpm, 8);
            juce::AudioBuffer<float> out;

            const int nPresets = p.presets->entries().size();
            for (int i = 0; i < nPresets; ++i)
            {
                p.presets->load (i);
                p.reset();
                const bool lim = p.apvts.getRawParameterValue ("limitOn")->load() > 0.5f;
                auto s = render (p, loop, out, block, sr, bpm, { { 0, 4 }, { 1, 5 }, { 3, 6 }, { 6, 7 } });
                const auto& e = p.presets->entries()[i];
                check (e.name, s, lim);
                if (sr == 48000.0 && block == 512)
                    writeWav (outDir.getChildFile (juce::File::createLegalFileName (e.name) + ".wav"), out, sr);
            }

            // every pad alone on Init
            p.presets->load (0);
            for (int pad = 0; pad < 8; ++pad)
            {
                auto s = render (p, loop, out, block, sr, bpm, { { pad, 2 }, { pad, 3 }, { pad, 5 } });
                check (juce::String ("Init + pad ") + pads::labels[pad], s, true);
            }

            // extreme: everything maxed
            p.presets->load (0);
            for (auto* prm : p.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    const auto id = r->getParameterID();
                    if (id.startsWith ("pad") || id == "outGain" || id == "inGain" || id == "ceiling" || id == "slSeed" || id == "gateThr") continue;
                    r->setValueNotifyingHost (1.0f);
                }
            auto s = render (p, loop, out, block, sr, bpm, { { 0, 4 }, { 4, 5 }, { 3, 6 } });
            check ("ALL KNOBS MAXED", s, true);
        }
    }

    // ---- behaviour checks with a 440 Hz sine
    {
        printf ("\n=== behaviour checks ===\n");
        const double sr = 48000.0, bpm = 120.0;
        DC6Processor p;
        p.setRateAndBufferSizeDetails (sr, 256);
        p.prepareToPlay (sr, 256);
        p.presets->load (0);
        for (auto* id : { "smash", "mSat", "subBoost", "chorus", "snap", "fDrive", "bDrive", "preamp", "flutter", "hiss", "bAsym" })
            if (auto* r = p.apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (0.0f));
        if (auto* r = p.apvts.getParameter ("spOn")) r->setValueNotifyingHost (0.0f);
        juce::AudioBuffer<float> sine (2, (int) sr * 4), out;
        for (int i = 0; i < sine.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c) sine.setSample (c, i, 0.3f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * i / (float) sr));

        auto freqIn = [&] (int from, int len)
        {
            int zc = 0;
            for (int i = from + 1; i < from + len; ++i)
                if (out.getSample (0, i - 1) < 0.0f && out.getSample (0, i) >= 0.0f) ++zc;
            return zc * sr / len;
        };
        const int spb = (int) (sr * 60.0 / bpm);

        // TAPE SLOW held for beats 4-7 (bar 1): pitch should settle near 220 Hz
        render (p, sine, out, 256, sr, bpm, {});
        const double before = freqIn (spb * 2, spb);
        {
            FakeHead head; head.bpm = bpm; p.setPlayHead (&head);
            out.makeCopyOf (sine);
            for (int pos = 0; pos < out.getNumSamples(); pos += 256)
            {
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, 256);
                juce::MidiBuffer midi;
                if (pos == spb * 4 / 256 * 256) midi.addEvent (juce::MidiMessage::noteOn (1, 41, (juce::uint8) 100), 0);
                if (pos == spb * 7 / 256 * 256) midi.addEvent (juce::MidiMessage::noteOff (1, 41), 0);
                head.ppq = (double) pos / spb;
                p.processBlock (view, midi);
            }
            p.setPlayHead (nullptr);
        }
        const double slow = freqIn (spb * 5, spb);
        const double after = freqIn (spb * 7 + spb / 2, spb / 2);
        printf ("  tape slow: before %.0f Hz, held %.0f Hz, released %.0f Hz  %s\n", before, slow, after,
                (std::abs (slow - 220.0) < 25.0 && std::abs (after - 440.0) < 25.0) ? "ok" : "WRONG");
        if (! (std::abs (slow - 220.0) < 25.0 && std::abs (after - 440.0) < 25.0)) ++failures;

        // TAPE STOP held 2 beats: level should fall towards silence
        render (p, sine, out, 256, sr, bpm, { { 0, 1 } });
        float endLevel = 0.0f;
        for (int i = spb * 4 + spb - 2000; i < spb * 5; ++i) endLevel = juce::jmax (endLevel, std::abs (out.getSample (0, i)));
        printf ("  tape stop: level at end of hold %.3f (start 0.3)  %s\n", endLevel, endLevel < 0.2f ? "ok" : "WRONG");
        if (endLevel >= 0.2f) ++failures;
    }

    printf ("\n%s (%d problems)\n", failures == 0 ? "ALL PASSED" : "PROBLEMS FOUND", failures);
    return failures == 0 ? 0 : 1;
}
