DeepTrance

A trance-focused modulation and combo-effects plug-in (VST3 and CLAP, plus a Windows
standalone) for Ableton Live, FL Studio and Serato Studio. DeepTrance turns static pads,
plucks, leads and vocals into moving, euphoric trance textures.

Status: v1.1.0 “Euphoria”. Projects and presets made with 1.0 load and sound bit-for-bit
the same (checked in CI against 30 saved 1.0.3 sessions).

1.1.0 “Euphoria” adds:

-   Pump: tempo-synced sidechain-style ducking (1/8 to 1 bar, depth, shape, Full / Lows /
    Highs), no routing needed.
-   Performance pads: Throw, Wash, Drop and Riser (Build-Up over 4, 8 or 16 bars). Hold to
    play, Shift-click to latch; automatable.
-   Gate: swing, ping-pong stereo, 8 new patterns, and a live step playhead in both views.
-   Output stage: Width, Bass Mono and a Safety Clip (on for new instances, off for 1.0
    projects).
-   Modulation: Gate, Pump and Step Random sources; six new destinations.
-   A/B compare and Randomize in the preset bar; 16 new presets (42 in all).
-   Cleaner sound changes: a new sound starts with empty echoes and reverb, and every
    module takes its new settings at once. Before, the old tail could replay through the
    new settings, and a Vowel or Swirl could glide audibly while the new sound faded in
    (found by switching between 1,000 random sounds).

1.0.3 smooths fast synced LFOs and Astral across loop wraps. When your DAW loops round,
they now glide on from exactly where they were, and a fast LFO (1/16, 1/32) glides over
about one of its own cycles, so it never sweeps faster than drawn.

1.0.2 removes clicks and static:

-   Loops and restarts: when your DAW loops round or you jump the playhead, synced sweeps
    (Swirl, Vowel, the LFOs and Astral) now glide smoothly to the new position instead of
    jumping.
-   Changing sounds: loading a preset, switching programs or recalling a project while
    audio plays now fades smoothly (about 50 ms) instead of clicking.
-   Abyss: turning Decay or Damping over a ringing tail is now smooth.
-   Punch: no longer fires a false hit when a sound fades back in.

1.0.1 improves stability when your DAW saves or loads a project while DeepTrance is open.
Some hosts do this in the background, and in rare cases it could crash the plug-in. Your
projects and presets are unchanged.

-   User Guide: docs/DeepTrance_User_Guide.md (also .txt and .pdf)
-   Download: the installer is attached to each GitHub Release, and to every successful
    Build run under Artifacts.

Features

  -----------------------------------------------------------------------------------------
  Area                                         What it does
  -------------------------------------------- --------------------------------------------
  Trance Gate                                  16-step tempo-synced volume + filter gate,
                                               16 patterns, drawable custom pattern, swing,
                                               ping-pong

  Vowel Filter                                 A-E-I-O-U formant morph with synced sweep

  Swirl                                        Dual-band phaser/flanger; lows stay solid

  Drift                                        Tape wow and flutter (up to 25 cents)

  Acid                                         Resonant 303-style filter + accent
                                               envelope + 3-band anti-aliased distortion,
                                               level-matched

  Punch                                        Transient shaper that ducks every wet tail
                                               after it

  Cloud                                        Live granular engine (up to 96 grains) with
                                               pitch, jitter, shimmer, Freeze

  Fractal Delay                                Ping-pong delay with pitch shift + filter in
                                               the feedback loop

  Abyss                                        8-line FDN reverb, decays to 60 s, octave
                                               shimmer, Freeze

  Build-Up                                     One knob: HPF sweep, noise riser, bigger
                                               reverb, faster swirl

  Pump                                         Synced duck on the whole output; Full, Lows
                                               (LR4 split) or Highs

  Pads                                         Throw, Wash, Drop, Riser: momentary
                                               performance moves

  Output                                       Width, Bass Mono (24 dB/oct on the side
                                               signal), Safety Clip

  Combos                                       Astral Projection, Acid Bath, blendable by
                                               amount

  Modulation                                   2 LFOs (8 shapes incl. drawn), envelope
                                               follower, gate, pump and step-random
                                               sources, 8-slot matrix, 35 destinations

  Interface                                    Easy view (Deep Space XY pad, 6 macros,
                                               Perform row, one-click switches) and Pro
                                               view

  Presets                                      42 factory presets (also host programs) +
                                               user presets, A/B compare, Randomize
  -----------------------------------------------------------------------------------------

Zero latency. Effects that are off cost no CPU once their tails end.

Signal flow

    Input > Gate > Vowel > Swirl > Drift > Acid > Build-Up > Punch
          > Cloud > Fractal Delay > Abyss > Astral sweep > Mix > Pump
          > Output gain > Width / Bass Mono / Drop / Safety Clip > Output
                (wet tails ducked by Punch)

Parameters are applied every 32 samples in this order:
user values > mod matrix > Easy macros > Build-Up (+ Riser) > Combo > Pads > modules.

Saved-state compatibility. Parameter IDs are permanent and only ever added. The 1.0 Gate
Pattern list is unchanged (DAW automation stores a choice as a 0..1 position); the 1.1
patterns are a second parameter, Gate Pattern+, shown as one menu. Every 1.1 parameter
defaults to “no change” except Safety Clip, which migrateState() turns off for 1.0.x
sessions and presets (states without stateVersion 2).

Building from source

Requirements: CMake 3.22+, a C++20 compiler (Visual Studio 2022 or newer on Windows). JUCE
8.0.15 is downloaded automatically.

    cmake -S . -B build -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
    build/DeepTranceRender_artefacts/Release/DeepTranceRender.exe   # renders all presets

Options:

-   -DDT_JUCE_PATH=C:/path/to/JUCE uses a local JUCE checkout.
-   -DDT_BUILD_PLUGIN=OFF builds only the DSP library and its tests (no JUCE).
-   DeepTranceRender <folder> also writes one WAV per factory preset.
-   DeepTranceTests --render <folder> writes per-module demo WAVs.
-   DeepTranceRender --compat checks the 1.0.3 sessions in Tests/compat (bit-exact on GCC
    x86-64).
-   DeepTranceRender --static-random N [firstSeed] switches between N random sounds and
    reports any click.
-   DeepTranceRender --snapshot <folder> draws the Easy and Pro views to PNG files,
    without a host.

The renderer builds on Linux too (install the JUCE dependencies: ALSA, X11, FreeType,
fontconfig).

The installer is built with Inno Setup 6:
iscc /DAppVersion=1.1.0 installer\DeepTrance.iss.

Project layout

    Source/DSP/        Plain C++ DSP, no JUCE (one class per module + engine)
    Source/Params/     Parameter IDs and layout (IDs are permanent once released)
    Source/Presets/    Preset manager and the factory library
    Source/UI/         Look-and-feel, controls, Easy view, Pro view
    Source/            JUCE processor and editor
    Tests/             DSP unit tests; offline preset renderer
    installer/         Inno Setup script
    docs/              User Guide and README in .md, .txt and .pdf
    .github/workflows/ CI: tests, presets, pluginval, VST3 and CLAP validators, installer, releases

Quality checks

Every push runs on Linux, then Windows:

-   1.0.3 compatibility: 12 engine configurations and 30 saved 1.0.3 sessions (every
    factory preset plus hand-edited states) must render bit-for-bit the same, and load
    with every 1.1 feature neutral.
-   1.1 features: pump depth, timing, band split and block-size independence; gate swing
    and ping-pong; width, bass mono and safety clip; Throw tails, Drop, Riser timing,
    Wash; the new modulation sources; loop wraps and switching every 1.1 feature while
    playing; A/B round trips; Randomize never touching levels; CPU (1.1 may cost at most
    20% over the 1.0 worst case, measured in the same run).
-   DSP unit tests: bit-exact bypass; stability at extreme settings from 44.1 to 192 kHz;
    gate timing and block-size independence; formants; detune in cents; echo timing and
    ping-pong sides; pitch per repeat; tail spillover; reverb decay time; Freeze hold;
    shimmer stability; transient and ducking depth; grain pitch and loudness; acid drive
    loudness; Build-Up, combos, macros, LFO sync and mod matrix maths; CPU budget.
-   Static checks: loop wraps, wraps with a stopped block, restarts and sound changes
    across every factory preset, host program and saved state, measured for clicks against
    the sound’s own sharpest moment. Unit tests reproduce the 1.0.1 Swirl loop-wrap and
    Abyss Decay clicks, and the 1.0.2 fast-LFO loop-wrap step, and fail on the old code.
-   Preset renderer: all 42 factory presets through the real processor (finite, sane
    loudness). The static check also covers Randomize (12 seeds) and A/B switching.
-   pluginval at strictness level 10.
-   Steinberg VST3 validator (47 tests).
-   clap-validator, the official CLAP conformance suite.
-   Installer build.

Pushing a tag such as v1.1.0 publishes a GitHub Release with the installer attached. A
release can also be made from the Build workflow’s Run workflow button by entering a tag
(for example v1.1.0); the Windows job waits for the Linux tests and the Linux render
checks, so nothing is published unless every check passes.

Known edge (1.1): over 1,000 random sound changes, one measured slightly over the click
limit (1.30 vs 1.25). It is the new sound’s own gate opening (its 2 ms attack starting
from silence) landing at a loud point of the test tone, not the change itself. The gate’s
attack shape is kept as in 1.0 so 1.0 sounds stay bit-exact.

Documentation: this README also ships as README.txt and README.pdf. Regenerate both with
python3 tools/build_docs.py (needs pandoc, wkhtmltopdf and beautifulsoup4).

Measured performance

48 kHz, one core: defaults about 3%, everything on with 8 mod slots about 7%, worst case
(96 grains + Build-Up) about 13%, and about the same with Pump, every pad and the Output
stage on top.

Before selling

-   JUCE licence: selling requires a JUCE licence that matches your revenue tier. Check
    current terms at juce.com.
-   Code signing: the installer is unsigned, so Windows SmartScreen shows a warning. A
    code-signing certificate removes it.
-   EULA: add your licence agreement (LicenseFile= in installer/DeepTrance.iss).

Roadmap status

  -----------------------------------------------------------------------------------------
  Phase                         Scope                         Status
  ----------------------------- ----------------------------- -----------------------------
  1                             Scaffold, CI, Gate, Vowel,    Done
                                Swirl, Drift                  

  2                             Fractal Delay, Abyss +        Done
                                Freeze, Punch                 

  3                             Cloud granular engine         Done

  4                             Acid, Build-Up, combos, LFOs, Done
                                mod matrix                    

  5                             Easy/Pro UI, XY pad, presets  Done

  6                             Validation, installer, docs   Done (DAW listening pass
                                                              pending)

  1.1                           Euphoria: Pump, pads, gate    Done
                                swing/ping-pong/patterns,     
                                Output stage, mod sources,    
                                A/B, Randomize, 16 presets    
  -----------------------------------------------------------------------------------------
