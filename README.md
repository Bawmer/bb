# Bub Leon Mix Rack

A modular channel-strip rack for **Windows x64 (VST3 + Standalone)** by BL Audio.
One plugin instance holds a chain of **8 slots**. You can load any of 12 built-in modules into each slot, reorder them by drag and drop, and bypass, solo, trim, blend and oversample each one. You can save single modules or the whole rack as presets.
All DSP, GUI artwork and module names are original.

![Bub Leon Mix Rack](docs/screenshot.png)

- C++20, JUCE 8.0.4, CMake. Internal processing is 64-bit double precision, 44.1–192 kHz
- Layouts: mono, stereo, stereo-in/mono-out, mono-in/stereo-out
- Real-time safe audio thread: no locks, allocations or I/O. Modules are swapped lock-free (built on the message thread, handed over through atomic pointers, freed later on the message thread)
- Full latency compensation. Each slot's dry path is delayed to match its module, and the total is reported to the host
- Tested target host: Cubase 15 on Windows 11

## Download

Every push builds the plugin on GitHub Actions (**Actions → Build Windows x64 VST3 → artifact `BubLeonMixRack-Windows-x64`**).
Copy `Bub Leon Mix Rack.vst3` to `C:\Program Files\Common Files\VST3\` and rescan in Cubase.

## Build locally

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```
Add `-DJUCE_DIR=C:\path\to\JUCE` to use a local JUCE checkout instead of fetching it.
Output: `build/BubLeonMixRack_artefacts/Release/VST3/Bub Leon Mix Rack.vst3`

## Using the rack

| Element | What it does |
|---|---|
| Module selector (top of each unit) | Load / swap / clear a module. Swapping resets the slot to the module's defaults |
| Grip (the ridges next to **S**) | Drag a unit onto another unit to move it to that chain position. Automation follows the module |
| Power LED | Module on / bypass, with a click-free 12 ms crossfade |
| **S** | Solo: hear this module only (all other modules are bypassed) |
| 1x / 2x / 4x | Per-module oversampling (Console Preamp, Passive Tube EQ, FET Compressor, Tape Saturator) |
| IN / MIX / OUT | Per-slot input trim, parallel dry/wet blend (latency aligned), output trim |
| ≡ menu / right-click | Save / load module preset (`.blmod`), copy / paste, reset, clear |
| Top bar | Rack presets (factory banks + user), save, A/B compare and copy, undo / redo, global input / output, global oversampling override, latency and CPU readout |
| Bottom bar | Input/output peak + RMS meters, LUFS short-term (3 s, BS.1770), gain reduction for every slot in chain order |

Knobs: drag to adjust, **Ctrl-drag** for fine control, **double-click** to reset, mouse wheel to step. The value shows while you drag and in the tooltip.
The window can be resized from 100 % to 200 % (drag the corner), and the size is saved with the project.

### Presets

- **Factory:** 6 banks × 5 presets: Vocal Lead, Vocal Background, Drum Bus, Bass, Mix Bus, Master Glue. They're gain-staged for material around −18 dBFS RMS
- **User racks:** `Documents\BL Audio\Mix Rack\Presets\*.blrack` (full rack state, XML)
- **Module presets:** `Documents\BL Audio\Mix Rack\Presets\Modules\*.blmod` (one module, values in real units, so they survive future range changes)

## Custom faceplate images

Every unit is drawn as vectors by default. To use your own artwork, put a PNG named after the module id in

```
Documents\BL Audio\Mix Rack\Faceplates\<module-id>.png
```

e.g. `vca.png`, `tubeeq.png`. Use **296 × 1096 px** (2× of the 148 × 548 unit) for sharp results at 200 %. Images are loaded once when the plugin opens and stretched to the unit. Controls are drawn on top at fixed positions: two columns × six rows of knobs starting 58 px from the top, the IN/MIX/OUT row at 422 px, and meters at 484 px (unit coordinates). Module ids: `preamp eq4 tubeeq fet opto vca deess gate tape imager transient limiter`.

## Parameter map (host automation)

Each slot exposes 18 fixed parameters, so automation stays valid when you change modules:
`S<n> P1…P12` (generic module controls), `S<n> Bypass`, `S<n> Solo`, `S<n> Mix`, `S<n> Input Trim`, `S<n> Output Trim`, `S<n> Oversampling`.
Global controls: `Input Gain`, `Output Gain`, `Oversampling Mode` (Per Slot / Off / 2x All / 4x All). That makes 147 parameters in total.
Names and value text follow the loaded module (e.g. `S2 VCA Threshold = -12.0 dB`). The host is told when names change.
P-numbers map to module controls as follows:

#### Console Preamp (`preamp`, Color, oversampling)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Drive | 0.0 dB … 24.0 dB | 0.0 dB |
| P2 | Tilt | -6.0 dB … 6.0 dB | 0.0 dB |
| P3 | Color | Clean / Iron / Tube | Iron |
| P4 | Low Cut | Off … 250 Hz | Off |
| P5 | Output | -18.0 dB … 6.0 dB | 0.0 dB |

#### 4-Band EQ (`eq4`, EQ)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | High Pass | Off … 500 Hz | Off |
| P2 | Low Pass | 3.00 kHz … Off | Off |
| P3 | Low Gain | -15.0 dB … 15.0 dB | 0.0 dB |
| P4 | Low Freq | 30 Hz … 450 Hz | 100 Hz |
| P5 | Low-Mid Gain | -15.0 dB … 15.0 dB | 0.0 dB |
| P6 | Low-Mid Freq | 200 Hz … 2.50 kHz | 600 Hz |
| P7 | Low-Mid Q | 0.40 … 4.00 | 0.90 |
| P8 | High-Mid Gain | -15.0 dB … 15.0 dB | 0.0 dB |
| P9 | High-Mid Freq | 600 Hz … 7.00 kHz | 2.50 kHz |
| P10 | High-Mid Q | 0.40 … 4.00 | 0.90 |
| P11 | High Gain | -15.0 dB … 15.0 dB | 0.0 dB |
| P12 | High Freq | 1.50 kHz … 16.0 kHz | 8.00 kHz |

#### Passive Tube EQ (`tubeeq`, EQ, oversampling)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Low Freq | 20 Hz / 30 Hz / 60 Hz / 100 Hz | 60 Hz |
| P2 | Low Boost | 0.0 … 10.0 | 0.0 |
| P3 | Low Atten | 0.0 … 10.0 | 0.0 |
| P4 | High Freq | 3 kHz / 4 kHz / 5 kHz / 8 kHz / 10 kHz / 12 kHz / 16 kHz | 8 kHz |
| P5 | High Boost | 0.0 … 10.0 | 0.0 |
| P6 | Bandwidth | 0.0 … 10.0 | 5.0 |
| P7 | Atten Freq | 5 kHz / 10 kHz / 20 kHz | 10 kHz |
| P8 | High Atten | 0.0 … 10.0 | 0.0 |
| P9 | Tube Drive | 0.0 … 10.0 | 2.0 |
| P10 | Output | -12.0 dB … 6.0 dB | 0.0 dB |

#### FET Compressor (`fet`, Dynamics, oversampling)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Input | 0.0 dB … 48.0 dB | 0.0 dB |
| P2 | Output | -24.0 dB … 24.0 dB | 0.0 dB |
| P3 | Attack | 20 us … 800 us | 200 us |
| P4 | Release | 50 ms … 1.10 s | 400 ms |
| P5 | Ratio | 4:1 / 8:1 / 12:1 / 20:1 / All | 4:1 |

#### Opto Compressor (`opto`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Peak Reduction | 0 … 100 | 0 |
| P2 | Gain | 0.0 dB … 40.0 dB | 0.0 dB |
| P3 | Mode | Compress / Limit | Compress |
| P4 | Emphasis | 0 % … 100 % | 0 % |

#### VCA Bus Comp (`vca`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Threshold | -40.0 dB … 0.0 dB | 0.0 dB |
| P2 | Ratio | 1.5:1 / 2:1 / 4:1 / 10:1 | 2:1 |
| P3 | Attack | 0.1 ms / 0.3 ms / 1 ms / 3 ms / 10 ms / 30 ms | 10 ms |
| P4 | Release | 0.1 s / 0.3 s / 0.6 s / 1.2 s / Auto | Auto |
| P5 | Makeup | 0.0 dB … 20.0 dB | 0.0 dB |
| P6 | SC High Pass | Off … 400 Hz | Off |

#### De-Esser (`deess`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Frequency | 2.00 kHz … 12.0 kHz | 6.00 kHz |
| P2 | Threshold | -60.0 dB … 0.0 dB | -30.0 dB |
| P3 | Range | 0.0 dB … 24.0 dB | 8.0 dB |
| P4 | Mode | Split / Wide | Split |
| P5 | Listen | Off / On | Off |

#### Gate / Expander (`gate`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Threshold | -80.0 dB … 0.0 dB | -60.0 dB |
| P2 | Range | 0 dB … 80 dB | 40 dB |
| P3 | Mode | Gate / Exp 1:2 / Exp 1:4 | Gate |
| P4 | Attack | 50 us … 50.00 ms | 500 us |
| P5 | Hold | 0 us … 500 ms | 30 ms |
| P6 | Release | 5 ms … 2.00 s | 120 ms |
| P7 | Hysteresis | 0.0 dB … 12.0 dB | 3.0 dB |
| P8 | SC High Pass | Off … 2.00 kHz | Off |
| P9 | SC Low Pass | 1.00 kHz … Off | Off |

#### Tape Saturator (`tape`, Color, oversampling)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Drive | -12.0 dB … 18.0 dB | 0.0 dB |
| P2 | Bias | -100 % … 100 % | 0 % |
| P3 | Head Bump | 0 % … 100 % | 50 % |
| P4 | Speed | 7.5 ips / 15 ips / 30 ips | 15 ips |
| P5 | Wow/Flutter | 0 % … 100 % | 0 % |
| P6 | Hiss | Off / On | Off |
| P7 | Output | -18.0 dB … 12.0 dB | 0.0 dB |

#### Stereo Imager (`imager`, Utility)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Width | 0 % … 200 % | 100 % |
| P2 | Mono Bass | Off … 300 Hz | Off |
| P3 | M/S Balance | -100 % … 100 % | 0 % |
| P4 | Output | -12.0 dB … 12.0 dB | 0.0 dB |

#### Transient Shaper (`transient`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Attack | -100 % … 100 % | 0 % |
| P2 | Sustain | -100 % … 100 % | 0 % |
| P3 | Speed | Fast / Medium / Slow | Medium |
| P4 | Output | -12.0 dB … 12.0 dB | 0.0 dB |

#### Output Limiter (`limiter`, Dynamics)

| # | Control | Range | Default |
|---|---|---|---|
| P1 | Input | 0.0 dB … 24.0 dB | 0.0 dB |
| P2 | Ceiling | -12.0 dB … 0.0 dB | -0.3 dB |
| P3 | Release | 1 ms … 1.00 s | 80 ms |
| P4 | True Peak | Off / On | On |

## Architecture

```
Source/
  DSP/        Common.h (biquads, smoothing, soft clip, delay line), Meters.h (peak/RMS/LUFS-S)
  Engine/     ModuleBase.h (module + ParamSpec interface), ModuleRegistry.h, RackEngine.h (slots, swaps, OS, latency)
  Modules/    Color, EQ, Dynamics, Utility, Limiter modules
  GUI/        RackLookAndFeel.h, Widgets.h (LED, meters), ModuleUnit (one rack unit)
  Presets/    FactoryPresets (30 rack presets)
  PluginProcessor / PluginEditor
tests/rack_test.cpp   null test, latency alignment, limiter ceiling, state recall,
                      all modules × 3 sample rates × 3 OS factors, factory presets, layouts, double path, undo
```

- **Slot swap:** `RackSlot::build()` (message thread) creates and prepares the module at the right rate → `install()` puts it in `pending` → the audio thread exchanges it at the next block start and moves the old one to `trash` → the processor timer deletes `trash`.
- **Chain order** is a packed `std::atomic<uint32_t>` (8 × 4 bits), so reordering never blocks audio.
- **Bypass null test:** with every module bypassed, the output equals the input delayed by the reported latency, bit-exact (checked in CI).
