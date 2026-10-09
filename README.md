# DeepTrance

A trance-focused modulation and combo-effects plugin (VST3 + Windows standalone) for Ableton Live, FL Studio and Serato Studio. DeepTrance turns static pads, plucks and vocals into moving, euphoric trance textures.

**Status:** v0.1 (Phase 1 of 6). The core modulation modules are complete and validated.

## What is in Phase 1

| Module | What it does | Default |
|---|---|---|
| **Trance Gate** | 16-step tempo-synced volume and filter gate. 8 factory patterns, or draw your own. | Off |
| **Vowel Filter** | Three formant filters morphing through A-E-I-O-U, with a tempo-synced sweep for "talking" leads. | Off |
| **Swirl** | Dual-band phaser and/or flanger. Only the highs swirl; the low end stays solid. | On, 35% mix |
| **Drift** | Tape-style wow and flutter for wide, slightly detuned 90s-trance width. | On, 35% mix |

The signal flows in a fixed order: Input, Gate, Vowel, Swirl, Drift, Mix, Output.

### Easy and Pro views

- **Easy** shows the one or two controls per module that matter most.
- **Pro** shows every parameter plus the drawable 16-step gate editor.
- Switching views never changes the sound. The choice is saved with your session.
- Drawing on the step grid while a factory pattern is playing copies that pattern into **Custom** first, so every factory pattern is an editable starting point.

### Behavior worth knowing

- **Tempo sync:** follows your DAW's tempo and bar position while playing. With the transport stopped, the gate and LFOs keep running at the last tempo, so you can audition without pressing play.
- **Zero latency:** DeepTrance reports no plugin delay.
- **Click-free:** every module fades in and out over 15 ms when switched, and all knobs are smoothed.
- **Low CPU:** about 1.7% of one core with every Phase 1 module on (48 kHz).
- **Double-click** any knob to return it to its default.

## Installing (Windows)

1. Download `DeepTrance-Windows-x64` from the latest successful **Build** run on GitHub Actions.
2. Copy `DeepTrance.vst3` to `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in your DAW:
   - **Ableton Live:** Preferences, Plug-Ins, enable "Use VST3 Plug-In System Folders", then Rescan.
   - **FL Studio:** Options, Manage plugins, Find plugins.
   - **Serato Studio:** Settings, Plugins, enable VST3, then Rescan.

`DeepTrance.exe` is the standalone version and needs no DAW.

## Building from source

Requirements: CMake 3.22+, a C++20 compiler (Visual Studio 2022 or newer on Windows). JUCE 8.0.15 is downloaded automatically.

```
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

To use a local JUCE checkout instead of downloading: `-DDT_JUCE_PATH=C:/path/to/JUCE`.
To build only the DSP and its tests (no JUCE): `-DDT_BUILD_PLUGIN=OFF`.

## Project layout

```
Source/DSP/        Plain C++ DSP, no JUCE dependency (one class per module)
Source/Params/     Parameter IDs and layout (IDs are permanent once released)
Source/            JUCE processor and editor
Tests/             DSP unit tests and demo renders
.github/workflows/ Windows build, unit tests and pluginval
```

## Quality checks

Every push runs on Windows:
- DSP unit tests: bit-exact bypass, stability at extreme settings across 44.1 to 192 kHz, gate timing against the host clock, detune measured in cents, click-free toggling and CPU budget.
- **pluginval** at strictness level 10.

Run `DeepTranceTests --render <folder>` to write demo WAVs of each module.

## Roadmap

| Phase | Scope |
|---|---|
| 1 | Scaffold, CI, Gate, Vowel, Swirl, Drift **(done)** |
| 2 | Ping-Pong Fractal Delay, Abyss reverb with Freeze, Punch transient shaper |
| 3 | Cloud granular engine with Freeze, pitch jitter and spray |
| 4 | Build-Up macro, Astral Projection and Acid Bath combos, LFOs, mod matrix |
| 5 | Final UI: Deep Space XY pad, macros, factory presets |
| 6 | Validation in all three DAWs, Steinberg VST3 validator, full documentation |

## License note

DeepTrance is built with JUCE. Selling it requires a JUCE license that matches your revenue tier. Check the current terms at juce.com before release.
