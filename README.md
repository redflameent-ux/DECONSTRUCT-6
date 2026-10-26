# DECONSTRUCT-6 (DC-6)

A destruction multi-effect: Memphis tape & sub, industrial decay, and IDM micro-buffer glitching in one
insert, with a live loop slicer and a Filth & Sub band split in front of it.
VST3 + Standalone on Windows; AU + VST3 + Standalone on Mac.

## Signal flow

```
INPUT
  -> SLICER          records each loop, finds the hits, replays it re-cut on a 16-step grid (Anarchy picks the zone)
  -> SPLIT           Linkwitz-Riley crossover (80-250 Hz)
       LOW  ->  mono lock + harmonic drive  ------------------------------------------+
       HIGH ->  transient snap, tape/fold drive, ring mod, phase smear, chorus width   |
  -> 1 MEMPHIS TAPE & SUB   12-bit grime, cassette hiss; 808 sine resynth fired under kicks (adds to LOW)
  -> 2 INDUSTRIAL DECAY     diode/console/asymmetric/wavefold drive, screaming comb, self-oscillating ladder filter
  -> 3 MICRO-BUFFER         granular scatter + reverse burst
  -> bands recombined
  -> Reso Gate (transient / straight / off-beat / Euclid, optional sidechain key)
  -> Tape transport: wow & flutter, TAPE STOP, TAPE SLOW (-1 octave)
  -> Micro-stutters: 1/16, 1/32, 1/64, 1/128, riser, reverse  (+ internal safety limiter)
  -> MASTER: smash compressor, tape saturation, mono-focused sub boost, level match, brickwall limiter
  -> DRY / WET
```

The Chaos engine (sample & hold + Euclidean sequencer) modulates drive, cutoff, scatter and stutter length.
**Anarchy** scales the slicer zone, saturation, stutter odds and tape drift all at once.

## Front panel

| Module | Controls |
|---|---|
| Memphis Sub & Tape | TAPE SLOW pad, SUB LEVEL, GRIME / 12-BIT |
| Industrial Decay | WAVEFOLD, DRIVE, RESO GATE (+ on switch) |
| Micro-Buffer | STUTTER RATE, SCATTER, CHAOS MOD |
| Global Master | ANARCHY, DRY / WET |

## MIDI keys (also the PERFORM pads)

| Note | Pad |
|---|---|
| C1 (36) | Tape Stop |
| C#1 (37) | 1/32 stutter |
| D1 (38) | 1/64 stutter |
| D#1 (39) | Granular reverse burst |
| E1 (40) | 1/128 stutter |
| F1 (41) | Tape Slow (-1 octave) |
| F#1 (42) | Riser (pitch-ramp burst) |
| G1 (43) | 1/16 stutter |

FL Studio names these notes C3-G3. Turn on **LATCH** to make pads and keys toggle instead of hold.

In FL Studio, to play the pads from a MIDI track: in the plugin wrapper's settings (the cog), set an
input port number, then give the MIDI Out channel that plays the notes the same port.
In Ableton, set a MIDI track's "MIDI To" to the track holding DC-6, then pick "DECONSTRUCT-6".

## Factory presets

* **01 Memphis Sub Tapes** - 01_MEMPHIS_HEAVY_808, Drank Tape Crawl, Cassette Dub 1995, Half-Speed Basement, Cowbell Sub Pocket, Purple Wind-Down
* **02 Industrial Clipping** - 02_TRENT_CORROSION, Broken Console, Diode Furnace, Scrap Metal Gate, Self-Oscillate Scream, Reese Grinder
* **03 Drill IDM Matrix** - 03_APHEX_DRILL_CORE, Squarepusher Turnaround, Micro-Roll Machine, Granular Snowstorm, Audio-Rate Buzz, Offset Breaks
* **04 Hybrid Anarchy** - 04_TOTAL_DECONSTRUCTION, Memphis Machine Funeral, Bone Saw Sermon, Skeleton Key, Cathedral of Static

Your own presets save to `Documents/DECONSTRUCT-6/Presets`.

## Notes

* The slicer plays back the *previous* loop (½, 1 or 2 bars), so it needs one loop to "listen" before it starts.
* DC-6 reports 4 ms of latency to your DAW (the tape transport), so it stays in time.

## Building

Pushing to `main` builds everything on GitHub: the Windows installer, the Mac .pkg and zips land on the
Releases page as "latest". To build locally: `cmake -S . -B build` then `cmake --build build --config Release`.
