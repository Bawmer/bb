# Bub Leon Harmony Stack

VST3 plugin (Windows x64) that turns one lead vocal stem into stacked harmonies in real time:
up to **4 harmony voices**, each with an optional **double**, so you get as many as 8 layers.

## Auto Harmony: generate audio, drag it into your DAW
1. **Get the vocal in.** Drop a stem (WAV/AIFF/FLAC/MP3) onto the plugin or click the drop zone.
   Or press **CAPTURE** and play your track: the plugin records the vocal while the transport runs and stops when you hit stop.
2. **Pick a style** in the *Preset* menu and tweak it if you like. Every knob is used for the render.
3. Press **GENERATE**. **AUTO KEY** detects the song key from the vocal and sets Key/Scale for you.
4. **Drag the chips** onto tracks in Cubase:
   - *Harmony Stack*: all the voices together
   - *Lead + Stack*: your vocal plus the harmonies
   - *Voice 1–4*: each voice on its own (with **STEMS** on)
   - *ALL FILES*: drags everything at once
5. **ALL STYLES** renders a harmony stack for every style in one go (16 files), so you can audition them in the DAW.
6. **PREVIEW** plays the result through the plugin. **FOLDER** opens the render folder.

Every file is latency-free and starts at sample 0 of your stem. Drop it at the same position as the original vocal and it lines up.
Files are 24-bit WAV at the stem's sample rate, saved in `Documents\Bub Leon Harmony Stack\`.

## How it works
- **Pitch tracking:** YIN pitch detection follows the sung note, including vibrato and slides.
- **Formant-preserving shifting:** TD-PSOLA. Each harmony keeps the singer's own vowel tone, so a fifth up doesn't sound like a chipmunk.
- **Loudness matching:** every harmony is level-matched to the lead, whatever the interval.
- **Consonants:** breaths, "s" and "t" sounds are not pitch-shifted, so the stack doesn't spit. The **Breath** knob sets how much of them the harmonies keep.
- **Latency:** about 50 ms. The plugin reports it to the DAW, and Cubase compensates automatically.

## Modes
| Mode | What the voice Interval means |
|---|---|
| **Scale** | Scale steps in the selected Key/Scale (+2 = a third up). The harmony follows the singer's expression. |
| **Chromatic** | Fixed semitones (+7 = a fifth up, −12 = an octave down) |
| **MIDI** | Voice 1–4 take the held MIDI notes, lowest first. Play chords and the vocal sings them. |

## Controls
- **Voice 1–4:** on/off, Interval, Level, Pan, Detune (cents)
- **Double + Width:** adds a second take of every voice, panned around it with its own timing and detune
- **Humanize:** random pitch drift and timing offsets per voice, for a "real singers" feel
- **Formant:** shifts the tone of the harmonies darker (−) or brighter (+)
- **Breath:** how much of the consonants and breaths the harmonies keep
- **Glide:** how fast the harmonies move between notes
- **Vibrato / Vib Rate:** adds vibrato to the harmonies only
- **Gate:** the input level below which tracking stops (raise it if the recording has bleed)
- **Dry / Harmony / Output:** the mix. Turn **Dry to Off** to print a harmony-only stem.

## Style presets (17)
Init – Third Above · Pop Stack 3rd + 5th · R&B Silk · Gospel Choir · Neo-Soul 7ths · Kompa / Zouk Sweet ·
Afro House Chant · Trap Octave Stack · Drill Dark · EDM Anthem · Ballad Soft Below · Angelic Highs ·
Deep Choir Bed · Low Octave Thickener · Power Fifths · Wide Unison Doubler · MIDI Chord Choir (live MIDI only)

## Using it in Cubase 15
1. Copy `Bub Leon Harmony Stack.vst3` to `C:\Program Files\Common Files\VST3\` and rescan plugins.
2. Insert it on the vocal track. Set **Key/Scale** to the song's key and choose a preset.
3. **Printing harmony stems:** set Dry to Off, then use Render in Place on the vocal event.
4. **MIDI mode:** create a MIDI track and set its output to *Bub Leon Harmony Stack*. Record or draw the chords.
5. The lookahead latency is about 50 ms. While recording, bypass the plugin or use *Constrain Delay Compensation*.

Tip: a dry, tuned, single-take lead tracks best. Put reverb and delay **after** the plugin.

## Building
GitHub Actions (`.github/workflows/build-windows.yml`) builds the plugin on every push. It runs the engine tests and pluginval at strictness 10, then uploads the `.vst3` and the standalone `.exe` as an artifact.

To build locally (Visual Studio 2022 and CMake ≥ 3.22):
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
The output is `build/BubLeonHarmonyStack_artefacts/Release/VST3/Bub Leon Harmony Stack.vst3`.
