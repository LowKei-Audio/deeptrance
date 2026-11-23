# DeepTrance User Guide

**Version 1.1.0 · VST3, CLAP and standalone for Windows · by LowKei**

DeepTrance turns static pads, plucks, leads and vocals into moving, euphoric trance textures. It combines rhythmic gating, swirling modulation, vowel filtering, tape drift, acid grit, granular clouds, fractal echoes and an infinite reverb in one plug-in, with performance macros you can ride live or automate.

**What's new**

- **1.1.0 "Euphoria":** **Pump** (sidechain-style ducking with no routing), four **performance pads** (Throw, Wash, Drop, Riser), gate **swing**, **ping-pong** and 8 new patterns, an **Output** stage (Width, Bass Mono, Safety Clip), three new modulation sources, **A/B compare**, **Randomize** and 16 new presets. Your 1.0 projects and presets sound exactly as before.
- **1.0.3:** fast synced LFOs and Astral glide smoothly across loop wraps.
- **1.0.2:** no more clicks or static when your DAW loops, jumps or restarts playback, when you change presets or load a project while music plays, when you turn Abyss Decay or Damping over a tail, or when Punch hears a sound fade back in.
- **1.0.1:** better stability when your DAW saves or loads a project while DeepTrance is open.

---

## 1. Installing

1. Run **DeepTrance-1.1.0-Windows-Setup.exe**. It replaces an earlier version in place.
2. Choose **VST3 and CLAP plug-ins and standalone app**, or **Plug-ins only**.
3. Open your DAW and rescan plug-ins:
   - **Ableton Live:** Settings > Plug-Ins > turn on *Use VST3 Plug-In System Folders* > *Rescan*.
   - **FL Studio:** Options > Manage plugins > *Find plugins*. Versions of FL Studio with CLAP support list both a VST3 and a CLAP entry; either works, so use one per project.
   - **Serato Studio:** Settings > Plugins > enable VST3 > *Rescan*.

The VST3 is installed to `C:\Program Files\Common Files\VST3\DeepTrance.vst3` and the CLAP to `C:\Program Files\Common Files\CLAP\DeepTrance.clap`, the standard folders every DAW scans. The standalone app appears in the Start menu as **DeepTrance**.

> Windows may show a SmartScreen notice the first time you run the installer. Choose *More info* > *Run anyway*.

---

## 2. Quick Start

1. Put DeepTrance on a pad, pluck or vocal track.
2. Press play. DeepTrance follows your project tempo automatically.
3. Drag the dot in the **Deep Space** pad: right adds swirl, up adds space.
4. Click **GATE** in the module strip for an instant trance rhythm.
5. Step through presets with the **<** and **>** arrows at the top, or click **RANDOM** for a new sound.
6. Hold **WASH** or **THROW** in the Perform row and let go: that is a breakdown moment in one click.

By default DeepTrance loads with a gentle phaser (Swirl), subtle tape width (Drift), a dotted-eighth ping-pong echo and a medium reverb, so it sounds musical from the first second.

---

## 3. The Two Views

Click **EASY** or **PRO** in the top-right corner. Switching never changes the sound, and your choice is saved with your project.

### Easy view

| Control | What it does |
|---|---|
| **Deep Space pad** | X = Swirl intensity, Y = Space (delay and reverb). Double-click to recentre. |
| **GATE** | How hard the Trance Gate chops (when the gate is on). |
| **SWIRL** | Phaser/flanger depth and mix. |
| **SPACE** | Delay and reverb amount, plus reverb size. |
| **DRIFT** | Tape wobble and width. |
| **BUILD-UP** | The tension knob for breakdowns (see section 5). |
| **MIX** | Overall dry/wet. |
| **COMBO + Amount** | One-click effect chains (see section 6). |
| **Gate Pattern** | Choose the rhythm without opening Pro. The 1.1 patterns are listed under *New in 1.1*. |
| **Perform row** | The pads **THROW**, **WASH**, **DROP** and **RISER** (section 6), the **Riser** length, **Pump Depth**, and a live view of the gate pattern that lights the step playing now. |
| **Module strip** | One click turns each module on or off, including **PUMP**. |
| **FREEZE CLOUD / FREEZE ABYSS** | Hold the current sound indefinitely. |

The four intensity macros sit at **50%** by default, which means "exactly as set in Pro". Turn one down to 0% to remove that effect, or up to 100% to double it.

### Pro view

Every module has its own card with an **ON** switch in its header. Scroll to reach the Modulation section at the bottom. Drag a knob to change it; double-click to return it to its default. The mouse wheel scrolls the page and never changes a knob.

---

## 4. The Modules

DeepTrance processes in this order:

**Gate > Vowel > Swirl > Drift > Acid > Build-Up > Punch > Cloud > Delay > Abyss > Mix > Pump > Output**

### Trance Gate (Rhythm Enforcer)
A tempo-synced, 16-step volume and filter gate.

- **Rate:** 1/8, 1/16, 1/32 or triplets.
- **Pattern:** sixteen factory rhythms or **Custom**. The originals: Classic 16ths, Offbeat Pump, Driving, Gallop, Stutter, Accent Wave, Half Time, Build Roll. **New in 1.1:** Dotted Eighths, Tresillo, Psy Roll, Uplift Chop, Syncopated, Eighths, Echo Steps, Swell.
- **Swing (1.1):** delays every second step, from straight (0%) to a full shuffle (100%).
- **Ping-Pong (1.1):** steps alternate between the left and right speakers. At 100%, odd steps play only on the left and even steps only on the right.
- **Draw your own:** in Pro, draw on the 16-step grid. Drawing over a factory pattern copies it into Custom first, so every pattern is an editable starting point.
- **Depth, Length, Attack, Release:** how deep, how long and how smooth each step is.
- **Filter:** closed steps also sweep a low-pass filter down, for a softer "breathing" gate.

### Vowel Filter (The Voice)
Morphs through A-E-I-O-U formants for talking leads. **LFO Depth** and **LFO Rate** sweep the vowel in time with your track. **Resonance** sharpens the formants.

### Swirl (Dual-Band Phase and Flange)
Phaser, flanger or both, synced to tempo. Only the highs swirl; the bass below the **Crossover** stays solid. Raise **Low Band** to let some of the lows move too.

### Drift (Wow and Flutter)
Slow, tape-like pitch wobble for the wide, slightly detuned sound of classic 90s anthems. **Amount** reaches about 25 cents at maximum.

### Acid (Aggressive Saturation)
A resonant 303-style filter with an accent envelope (the "squelch"), followed by three-band distortion. Drive is level-matched, so turning it up changes the character, not the volume.

### Punch (Psy-Transient Shaper)
Sharpens the attack of plucks and kicks so they cut through a wet mix, and briefly ducks the delay, reverb and cloud after each hit (**Duck** and **Duck Release**).

### Cloud (Granular Engine)
Smears the live input into a cloud of tiny grains.

- **Size, Density:** short, sparse grains stutter; long, dense grains become a pad.
- **Position, Spray:** how far back grains reach, and how scattered they are.
- **Pitch, Jitter, Shimmer:** transpose, detune each grain, or throw grains up an octave.
- **Freeze:** stops listening and loops the current moment, perfect before a drop.

### Fractal Delay (Ping-Pong)
A stereo echo whose feedback runs through its own pitch shifter and filter. Set **Pitch** to +12 for crystalline, climbing echoes, or below zero for a deep, murky wash. **Tone** darkens each repeat. **Sync** off lets you set the time in milliseconds.

### Abyss Reverb (Infinite Space)
A lush, modulated reverb with decays up to 60 seconds. **Shimmer** adds octave-up sparkle. **Freeze** holds the tail indefinitely, turning a short pluck into an endless drone.

> Delay, Cloud and Abyss tails ring out naturally when you switch them off.

### Pump (1.1)
The classic trance "breathing": the level dips on every beat and swells back, like a kick sidechain, with nothing to route. It works on everything DeepTrance outputs, tails included.

- **Rate:** 1/8, 1/4 (one dip per beat), 1/2 or 1 bar.
- **Depth:** how far it dips. 100% reaches silence at the bottom of each dip.
- **Shape:** low values swell back at once; high values hold the dip and swell late.
- **Band:** **Full**, **Lows** (only the bass and low pads duck, the top stays steady) or **Highs**.

### Output (1.1)
- **Width:** 0% is mono, 100% is unchanged, 200% doubles the stereo spread.
- **Bass Mono:** keeps everything below the frequency you set in mono, for a solid, club-safe low end. *Off* by default.
- **Safety Clip:** a soft clipper that keeps the output under 0 dBFS. It does not touch anything below -3 dBFS, so it only acts on peaks (for example a runaway feedback or shimmer). It is on for new instances; projects and presets saved with 1.0 load with it off, so they sound exactly as before.

---

## 5. Build-Up (Tension Generator)

Automate **BUILD-UP** from 0% to 100% over 8 or 16 bars before a drop. As it rises, DeepTrance:

- sweeps a steep high-pass filter upward, removing the bass,
- adds a white-noise riser that follows the sweep (**Build-Up Noise** sets its level),
- increases the reverb amount, size and decay,
- speeds up the Swirl.

Snap it back to 0% on the drop.

---

## 6. Performance Pads (1.1)

Hold a pad to play it; let go to stop. **Shift-click** latches a pad on until you click it again. The pads are automatable, so you can record them into your DAW.

| Pad | What happens |
|---|---|
| **THROW** | Sends the sound hard into the echo and reverb. When you let go, the input stops but the tail keeps ringing, then eases back over 2 bars. Hold it over the last word of a vocal phrase. |
| **WASH** | Washes everything into a huge, endless reverb. Eases back over 1 bar when you let go. |
| **DROP** | Cuts the lows: the kick and bass vanish, the top stays. Hold it for the beat before a drop. |
| **RISER** | Runs Build-Up from 0% to 100% over the **Riser** length (4, 8 or 16 bars). The pad fills as it rises. Let go on the drop and it snaps back at once. |

---

## 7. Combos

| Combo | What happens |
|---|---|
| **Astral Projection** | Removes the dry signal, routes a granular smear into a huge shimmer reverb, and sweeps a slow 8-bar low-pass over it. Turns a fast arpeggio into an instant ambient breakdown. |
| **Acid Bath** | Engages the resonant 303 filter and distortion with a fast 1/16 ping-pong delay. Turns a clean pluck into a gritty acid line. |

**Amount** blends from your own settings (0%) to the full combo (100%). Automate it to morph into a combo over a few bars.

---

## 8. Modulation (Pro view)

### LFO 1 and LFO 2

- **Shapes:** Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold, Smooth Random and **Custom**.
- **Custom:** draw your own 16-point shape in the editor under each LFO. Drawing switches the LFO to Custom automatically. Your drawing is saved with the project and with user presets.
- **Sync:** on = locked to the bar (1/32 to 16 bars); off = free rate in Hz.

### Envelope Follower
Follows the input level. **Sensitivity** lifts quiet sources so they still drive the modulation.

### Mod Matrix
Eight slots. Each slot connects a **source** to a **destination**, with a positive or negative **amount**.

**Sources:** LFO 1, LFO 2, Envelope, and new in 1.1: **Gate** (follows the Trance Gate as it opens and closes), **Pump** (follows the pump's dip, even with Pump switched off) and **Step Random** (a new random value on every gate step).

**Destinations** include gate depth, length and filter, vowel and vowel resonance, swirl depth/feedback/mix/speed, drift, acid cutoff/resonance/drive, cloud position/size/density/pitch/jitter/mix, delay feedback/pitch/tone/mix, abyss size/decay/damping/shimmer/mix, punch attack, Build-Up, Pump Depth, Output Width and the global mix.

> Try: LFO 1, Saw Up, 8 Bars > Cloud Position at 40% for a pad that slowly scans through a vocal. Or Step Random > Acid Cutoff at 45% for a filter that jumps on every step.

---

## 9. Presets

Use **<** and **>** to step through presets, or click the preset name for the full menu.

**A / B:** compare two sounds. Edit freely, click **B**, and B starts as a copy of A; each slot keeps its own edits. **COPY** copies the sound you hear into the other slot. The two slots last for the session.

**RANDOM:** one click for a new sound from safe, musical settings: modules, rhythms, echoes and spaces. It never changes your levels, the global Mix, Build-Up or Safety Clip.

**Factory presets (42):**

- **Pads:** Euphoric Wash, Gated Supersaw, Talking Pad, Frozen Horizon
- **Plucks:** Classic 3/16 Echo, Crystal Ascend, Murky Descent, Psy Punch
- **Leads:** Acid Squelch, Vowel Lead, 90s Anthem, Flange Rider
- **Vocals:** Breathe Cloud, Angel Shimmer, Stutter Chop, Throw-Ready Vocal *(1.1)*, Wide Pump Vox *(1.1)*
- **Build-Ups:** 8-Bar Riser, Tension Gate, Infinite Riser, 16-Bar Swell *(1.1)*, Gate Pump Riser *(1.1)*
- **FX:** Astral Projection, Acid Bath, Frozen Abyss, Glitch Tops, Wobble Space, Breathing Swirl, Pumping Space
- **Uplifting (1.1):** Sidechain Supersaw, Euphoria Pump Pad, Ping-Pong Gate, Breakdown Wash
- **Psy (1.1):** Rolling Bass Gate, Acid Triplets, Step Random Filter
- **Progressive (1.1):** Swung Pluck Gate, Deep Pump Chords, Dotted Echo Gate
- **Tech (1.1):** Hard Eighths, Stereo Stab Gate

**Your presets:** click **SAVE** and give it a name. User presets are stored in `Documents\DeepTrance Presets` as `.dtpreset` files, so you can back them up or share them. The menu also has **Open Presets Folder**.

The factory presets also appear in your DAW's own preset list.

---

## 10. Tips for Each DAW

- **Ableton Live:** use *Configure* on the device title bar to expose macros for Push or clip automation.
- **FL Studio:** right-click any knob > *Create automation clip*. Build-Up is perfect for this.
- **Serato Studio:** load DeepTrance as a channel FX. The Easy view keeps everything one click away during a live set, and the pads are made for it.

---

## 11. Performance

DeepTrance adds **no latency**, so it stays in time in every DAW. Effects that are switched off use no CPU once their tails finish.

Measured CPU use (48 kHz, one core):

| Setup | CPU |
|---|---|
| Default sound | about 3% |
| Every module on, 8 mod slots active | about 7% |
| Worst case (96 dense grains + Build-Up) | about 13% |
| Worst case with Pump, every pad and the Output stage on top | about 13 to 14% |

---

## 12. Troubleshooting

| Problem | Fix |
|---|---|
| DeepTrance does not appear in the DAW | Rescan plug-ins (section 1). Make sure your DAW scans `C:\Program Files\Common Files\VST3` (or `C:\Program Files\Common Files\CLAP` for the CLAP). |
| The gate is out of time | DeepTrance follows the DAW tempo. With the transport stopped it keeps running at the last tempo; press play to lock it to the grid. |
| A frozen sound will not stop | Click **FREEZE ABYSS** or **FREEZE CLOUD** again, or switch that module off. |
| Too much low end in the reverb | Raise **Abyss Damping**, or use **Punch** to duck the tail. |
| A click when a loop wraps or a preset changes | Update to 1.1.0: loop wraps, restarts, preset changes, A/B and Randomize are click-free, and a new sound starts with fresh echoes and reverb. |
| Peaks sound softened | **Safety Clip** (Output card) rounds off peaks above -3 dBFS. Turn it off if you prefer, and lower Output Gain instead. |
| A pad stays on | It is latched (Shift-click). Click it once to release it. |
| The standalone app makes no sound | Click *Options* > *Audio/MIDI Settings* and choose your audio device and input. |

---

## 13. Specifications

- **Format:** VST3 and CLAP (64-bit) and standalone, Windows 10/11
- **Supported hosts:** Ableton Live, FL Studio, Serato Studio
- **Channels:** stereo or mono
- **Sample rates:** 44.1 to 192 kHz
- **Latency:** 0 samples
- **Parameters:** 149, all automatable
- **Window:** 1040 x 780 by default, resizable from 980 x 720
- **Validation:** Steinberg VST3 validator (47/47) and pluginval at strictness level 10
