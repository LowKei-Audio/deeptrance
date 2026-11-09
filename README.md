# DeepTrance

A trance-focused modulation and combo-effects plug-in (VST3 and CLAP, plus a Windows standalone) for Ableton Live, FL Studio and Serato Studio. DeepTrance turns static pads, plucks, leads and vocals into moving, euphoric trance textures.

**Status:** v1.0.1. All six build phases are complete and validated. Final sign-off needs a listening and loading pass in the three target DAWs.

**1.0.1** improves stability when your DAW saves or loads a project while DeepTrance is open. Some hosts do this in the background, and in rare cases it could crash the plug-in. Your projects and presets are unchanged.

- **User Guide:** [`docs/DeepTrance_User_Guide.md`](docs/DeepTrance_User_Guide.md) (also `.txt` and `.pdf`)
- **Download:** the installer is attached to each GitHub Release, and to every successful **Build** run under *Artifacts*.

## Features

| Area | What it does |
|---|---|
| **Trance Gate** | 16-step tempo-synced volume + filter gate, 8 patterns, drawable custom pattern |
| **Vowel Filter** | A-E-I-O-U formant morph with synced sweep |
| **Swirl** | Dual-band phaser/flanger; lows stay solid |
| **Drift** | Tape wow and flutter (up to 25 cents) |
| **Acid** | Resonant 303-style filter + accent envelope + 3-band anti-aliased distortion, level-matched |
| **Punch** | Transient shaper that ducks every wet tail after it |
| **Cloud** | Live granular engine (up to 96 grains) with pitch, jitter, shimmer, Freeze |
| **Fractal Delay** | Ping-pong delay with pitch shift + filter in the feedback loop |
| **Abyss** | 8-line FDN reverb, decays to 60 s, octave shimmer, Freeze |
| **Build-Up** | One knob: HPF sweep, noise riser, bigger reverb, faster swirl |
| **Combos** | Astral Projection, Acid Bath, blendable by amount |
| **Modulation** | 2 LFOs (8 shapes incl. drawn), envelope follower, 8-slot matrix, 29 destinations |
| **Interface** | Easy view (Deep Space XY pad + 6 macros + one-click switches) and Pro view |
| **Presets** | 26 factory presets (also host programs) + user presets |

Zero latency. Effects that are off cost no CPU once their tails end.

## Signal flow

```
Input > Gate > Vowel > Swirl > Drift > Acid > Build-Up > Punch
      > Cloud > Fractal Delay > Abyss > Astral sweep > Mix > Output
            (wet tails ducked by Punch)
```

Parameters are applied every 32 samples in this order:
`user values > mod matrix > Easy macros > Build-Up > Combo > modules`.

## Building from source

Requirements: CMake 3.22+, a C++20 compiler (Visual Studio 2022 or newer on Windows). JUCE 8.0.15 is downloaded automatically.

```
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
build/DeepTranceRender_artefacts/Release/DeepTranceRender.exe   # renders all presets
```

Options:

- `-DDT_JUCE_PATH=C:/path/to/JUCE` uses a local JUCE checkout.
- `-DDT_BUILD_PLUGIN=OFF` builds only the DSP library and its tests (no JUCE).
- `DeepTranceRender <folder>` also writes one WAV per factory preset.
- `DeepTranceTests --render <folder>` writes per-module demo WAVs.

The installer is built with Inno Setup 6: `iscc /DAppVersion=1.0.1 installer\DeepTrance.iss`.

## Project layout

```
Source/DSP/        Plain C++ DSP, no JUCE (one class per module + engine)
Source/Params/     Parameter IDs and layout (IDs are permanent once released)
Source/Presets/    Preset manager and the factory library
Source/UI/         Look-and-feel, controls, Easy view, Pro view
Source/            JUCE processor and editor
Tests/             DSP unit tests; offline preset renderer
installer/         Inno Setup script
docs/              User Guide and README in .md, .txt and .pdf
.github/workflows/ CI: tests, presets, pluginval, VST3 and CLAP validators, installer, releases
```

## Quality checks

Every push runs on Windows:

- **DSP unit tests:** bit-exact bypass; stability at extreme settings from 44.1 to 192 kHz; gate timing and block-size independence; formants; detune in cents; echo timing and ping-pong sides; pitch per repeat; tail spillover; reverb decay time; Freeze hold; shimmer stability; transient and ducking depth; grain pitch and loudness; acid drive loudness; Build-Up, combos, macros, LFO sync and mod matrix maths; CPU budget.
- **Preset renderer:** all 26 factory presets through the real processor (finite, sane loudness).
- **pluginval** at strictness level 10.
- **Steinberg VST3 validator** (47 tests).
- **clap-validator**, the official CLAP conformance suite.
- **Installer** build.

Pushing a tag such as `v1.0.1` publishes a GitHub Release with the installer attached.

## Measured performance

48 kHz, one core: defaults about 3%, everything on with 8 mod slots about 6%, worst case (96 grains + Build-Up) about 13%.

## Before selling

- **JUCE licence:** selling requires a JUCE licence that matches your revenue tier. Check current terms at juce.com.
- **Code signing:** the installer is unsigned, so Windows SmartScreen shows a warning. A code-signing certificate removes it.
- **EULA:** add your licence agreement (`LicenseFile=` in `installer/DeepTrance.iss`).

## Roadmap status

| Phase | Scope | Status |
|---|---|---|
| 1 | Scaffold, CI, Gate, Vowel, Swirl, Drift | Done |
| 2 | Fractal Delay, Abyss + Freeze, Punch | Done |
| 3 | Cloud granular engine | Done |
| 4 | Acid, Build-Up, combos, LFOs, mod matrix | Done |
| 5 | Easy/Pro UI, XY pad, presets | Done |
| 6 | Validation, installer, docs | Done (DAW listening pass pending) |
