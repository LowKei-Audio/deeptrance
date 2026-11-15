#include "DeepTranceEngine.h"
#include <cstring>

namespace dt
{
const std::array<const char*, (size_t) ModDest::Count>& modDestinationNames() noexcept
{
    static const std::array<const char*, (size_t) ModDest::Count> names {
        "None",
        "Gate Depth", "Gate Filter",
        "Vowel", "Vowel Mix",
        "Swirl Depth", "Swirl Feedback", "Swirl Mix",
        "Drift Amount", "Drift Mix",
        "Acid Cutoff", "Acid Resonance", "Acid Drive",
        "Cloud Position", "Cloud Size", "Cloud Density", "Cloud Pitch", "Cloud Jitter", "Cloud Mix",
        "Delay Feedback", "Delay Pitch", "Delay Tone", "Delay Mix",
        "Abyss Size", "Abyss Decay", "Abyss Shimmer", "Abyss Mix",
        "Punch Attack", "Build-Up", "Global Mix" };
    return names;
}

namespace
{
    /** Adds a modulation offset (`m` = amount x source, -1..1) to one destination.
        Spans are chosen so 100% amount sweeps the musically useful range;
        frequency/time destinations move in octaves. Modules clamp the results. */
    void applyMod (EngineParams& p, ModDest d, float m) noexcept
    {
        switch (d)
        {
            case ModDest::GateDepth:     p.gate.depth        += m; break;
            case ModDest::GateFilter:    p.gate.filterAmount += m; break;
            case ModDest::VowelMorph:    p.vowel.morph       += 4.0f * m; break;
            case ModDest::VowelMix:      p.vowel.mix         += m; break;
            case ModDest::SwirlDepth:    p.swirl.depth       += m; break;
            case ModDest::SwirlFeedback: p.swirl.feedback    += 0.95f * m; break;
            case ModDest::SwirlMix:      p.swirl.mix         += m; break;
            case ModDest::DriftAmount:   p.drift.amount      += m; break;
            case ModDest::DriftMix:      p.drift.mix         += m; break;
            case ModDest::AcidCutoff:    p.acid.cutoffHz     *= std::exp2 (5.0f * m); break;
            case ModDest::AcidResonance: p.acid.resonance    += m; break;
            case ModDest::AcidDrive:     p.acid.drive        += m; break;
            case ModDest::CloudPosition: p.cloud.positionMs  += 1000.0f * m; break;
            case ModDest::CloudSize:     p.cloud.sizeMs      *= std::exp2 (3.0f * m); break;
            case ModDest::CloudDensity:  p.cloud.density     *= std::exp2 (3.0f * m); break;
            case ModDest::CloudPitch:    p.cloud.pitch       += 24.0f * m; break;
            case ModDest::CloudJitter:   p.cloud.jitter      += m; break;
            case ModDest::CloudMix:      p.cloud.mix         += m; break;
            case ModDest::DelayFeedback: p.delay.feedback    += 0.95f * m; break;
            case ModDest::DelayPitch:    p.delay.pitch       += 12.0f * m; break;
            case ModDest::DelayTone:     p.delay.toneHz      *= std::exp2 (4.0f * m); break;
            case ModDest::DelayMix:      p.delay.mix         += m; break;
            case ModDest::AbyssSize:     p.abyss.size        += m; break;
            case ModDest::AbyssDecay:    p.abyss.decaySec    *= std::exp2 (3.0f * m); break;
            case ModDest::AbyssShimmer:  p.abyss.shimmer     += m; break;
            case ModDest::AbyssMix:      p.abyss.mix         += m; break;
            case ModDest::PunchAttack:   p.punch.attack      += 2.0f * m; break;
            case ModDest::BuildUp:       p.buildUp           += m; break;
            case ModDest::GlobalMix:     p.mix               += m; break;
            case ModDest::None: case ModDest::Count: break;
        }
    }

    float sat01 (float v) noexcept { return clampf (v, 0.0f, 1.0f); }
}

EngineParams DeepTranceEngine::derive (const EngineParams& user, float lfo1, float lfo2, float env) noexcept
{
    EngineParams p = user;

    // 1. Modulation matrix.
    for (const auto& slot : user.mod)
    {
        if (slot.source == 0 || slot.dest <= 0 || slot.dest >= (int) ModDest::Count || slot.amount == 0.0f) continue;
        const float src = slot.source == 1 ? lfo1 : slot.source == 2 ? lfo2 : env;
        applyMod (p, (ModDest) slot.dest, slot.amount * src);
    }
    p.buildUp = sat01 (p.buildUp);
    p.mix     = sat01 (p.mix);

    // 2. Easy-view macros: 50% = unchanged, 0% = none, 100% = double.
    const float fg = 2.0f * sat01 (user.macros.gate),  fs = 2.0f * sat01 (user.macros.swirl);
    const float fp = 2.0f * sat01 (user.macros.space), fd = 2.0f * sat01 (user.macros.drift);
    p.gate.depth        *= fg;  p.gate.filterAmount *= fg;
    p.swirl.depth       *= fs;  p.swirl.mix         *= fs;
    p.delay.mix         *= fp;  p.delay.feedback    *= 0.75f + 0.25f * fp;
    p.abyss.mix         *= fp;  p.abyss.size         = sat01 (p.abyss.size * (0.6f + 0.4f * fp));
    p.drift.amount      *= fd;  p.drift.mix         *= fd;

    // 3. Build-Up: more (and bigger, longer) reverb, faster swirl. HPF + riser run in BuildUp.
    const float b = p.buildUp;
    if (b > 0.0005f)
    {
        p.abyssOn = true;
        p.abyss.mix      = lerpf (p.abyss.mix, std::max (p.abyss.mix, 0.6f), b);
        p.abyss.size     = lerpf (p.abyss.size, 1.0f, 0.7f * b);
        p.abyss.decaySec *= 1.0f + 1.5f * b;
        p.swirl.speedMul = std::exp2 (3.0f * b);
    }

    // 4. Combos: blend from the user's settings toward the recipe by Combo Amount.
    const float a = sat01 (user.comboAmount);
    if (user.combo == Combo::AstralProjection && a > 0.0f)
    {
        // Granular smear straight into a huge shimmer reverb, dry signal removed.
        p.cloudOn = p.abyssOn = true;
        auto& c = p.cloud;
        c.mix = lerpf (c.mix, 1.0f, a);          c.sizeMs = lerpf (c.sizeMs, 180.0f, a);
        c.density = lerpf (c.density, 40.0f, a); c.positionMs = lerpf (c.positionMs, 400.0f, a);
        c.spray = lerpf (c.spray, 0.6f, a);      c.jitter = lerpf (c.jitter, 0.25f, a);
        c.shimmer = lerpf (c.shimmer, 0.35f, a); c.stereo = lerpf (c.stereo, 1.0f, a);
        auto& r = p.abyss;
        r.mix = lerpf (r.mix, 1.0f, a);           r.decaySec = lerpf (r.decaySec, 18.0f, a);
        r.size = lerpf (r.size, 1.0f, a);         r.shimmer = lerpf (r.shimmer, 0.55f, a);
        r.damping = lerpf (r.damping, 0.35f, a);  r.mod = lerpf (r.mod, 0.7f, a);
    }
    else if (user.combo == Combo::AcidBath && a > 0.0f)
    {
        // Resonant 303 filter + 3-band distortion into a fast synced ping-pong.
        p.acidOn = p.delayOn = true;
        auto& f = p.acid;
        f.cutoffHz = lerpf (f.cutoffHz, 500.0f, a); f.resonance = lerpf (f.resonance, 0.82f, a);
        f.envMod = lerpf (f.envMod, 0.75f, a);      f.decayMs = lerpf (f.decayMs, 180.0f, a);
        f.drive = lerpf (f.drive, 0.65f, a);        f.mix = lerpf (f.mix, 1.0f, a);
        auto& d = p.delay;
        if (a > 0.5f) { d.sync = true; d.syncIndex = 2; } // 1/16
        d.feedback = lerpf (d.feedback, 0.5f, a);   d.mix = lerpf (d.mix, 0.3f, a);
        d.pingPong = lerpf (d.pingPong, 1.0f, a);   d.toneHz = lerpf (d.toneHz, 4500.0f, a);
        d.pitch    = lerpf (d.pitch, 0.0f, a);
        p.abyss.mix *= 1.0f - 0.7f * a;              // keep it dry and aggressive
    }
    return p;
}

void DeepTranceEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    clock.prepare (sampleRate);
    params = incoming;   // whatever was loaded before this point is simply current now
    astralGlide.prepare (sampleRate);
    transitionOut = (float) (1.0 / (0.010 * sampleRate));   // fade to dry in 10 ms
    transitionIn  = (float) (1.0 / (0.030 * sampleRate));   // back in over 30 ms
    swapHoldSamples = (int) std::lround (0.010 * sampleRate);

    // Hand every module its real settings first: each module's prepare() snaps its
    // smoothers to its stored params, so nothing glides in from defaults on load.
    applyToModules (derive (params, 0.0f, 0.0f, 0.0f), true);

    gate.prepare (sampleRate);  vowel.prepare (sampleRate); swirl.prepare (sampleRate);
    drift.prepare (sampleRate); acid.prepare (sampleRate);  buildUp.prepare (sampleRate);
    punch.prepare (sampleRate); cloud.prepare (sampleRate); delay.prepare (sampleRate);
    abyss.prepare (sampleRate);
    for (auto& l : lfo) l.prepare (sampleRate);
    envFollower.prepare (sampleRate);

    for (auto* f : { &gateFader, &vowelFader, &swirlFader, &driftFader, &acidFader, &buildFader }) f->prepare (sampleRate);
    for (auto* s : { &inGainSm, &outGainSm, &mixSm }) s->prepare (sampleRate, 0.02);
    astralSm.prepare (sampleRate, 0.05);

    for (auto* v : { &dryL, &dryR, &modL, &modR, &duck }) v->assign (kChunk, 0.0f);

    reset();
}

void DeepTranceEngine::reset() noexcept
{
    gate.reset(); vowel.reset(); swirl.reset(); drift.reset(); acid.reset(); buildUp.reset();
    for (auto& l : lfo) l.reset();
    envFollower.reset();
    for (auto& s : astralLp) s.reset();
    lfoOut[0] = lfoOut[1] = envOut = 0.0f;
    astralGlide.reset();
    seenSoundChange = soundGate.current();
    holdingSound = false;
    transition = transitionTarget = 0.0f;
    swapHold = 0;
    lastBlockPeak = 0.0f;

    // Start in the requested state with no fades or ramps pending.
    applyToModules (derive (params, 0.0f, 0.0f, 0.0f), true);
    punch.reset(); cloud.reset(); delay.reset(); abyss.reset();
    inGainSm.setImmediate (dbToGain (params.inputGainDb));
    outGainSm.setImmediate (dbToGain (params.outputGainDb));
    mixSm.setImmediate (params.mix);
    astralSm.setImmediate (params.combo == Combo::AstralProjection ? sat01 (params.comboAmount) : 0.0f);
}

void DeepTranceEngine::applyToModules (const EngineParams& e, bool snapEnables) noexcept
{
    gateFader.setEnabled (e.gateOn);
    vowelFader.setEnabled (e.vowelOn);
    swirlFader.setEnabled (e.swirlOn);
    driftFader.setEnabled (e.driftOn);
    acidFader.setEnabled (e.acidOn);
    buildFader.setEnabled (e.buildUp > 0.0005f);
    punch.setEnabled (e.punchOn);
    cloud.setEnabled (e.cloudOn);
    delay.setEnabled (e.delayOn);
    abyss.setEnabled (e.abyssOn);

    if (snapEnables)
    {
        for (auto* f : { &gateFader, &vowelFader, &swirlFader, &driftFader, &acidFader, &buildFader }) f->snap();
        punch.snapEnabled(); cloud.snapEnabled(); delay.snapEnabled(); abyss.snapEnabled();
    }

    gate.setParams (e.gate);
    vowel.setParams (e.vowel);
    swirl.setParams (e.swirl);
    drift.setParams (e.drift);
    acid.setParams (e.acid);
    buildUp.setParams (e.buildUp, e.buildUpNoise);
    punch.setParams (e.punch);
    cloud.setParams (e.cloud);
    delay.setParams (e.delay, clock.getBpm());
    abyss.setParams (e.abyss);

    inGainSm.setTarget (dbToGain (clampf (e.inputGainDb, -24.0f, 24.0f)));
    outGainSm.setTarget (dbToGain (clampf (e.outputGainDb, -24.0f, 24.0f)));
    mixSm.setTarget (sat01 (e.mix));
    astralSm.setTarget (e.combo == Combo::AstralProjection ? sat01 (e.comboAmount) : 0.0f);
}

template <typename Fn>
void DeepTranceEngine::runModule (ModuleFader& fader, float* l, float* r, int n, Fn&& process) noexcept
{
    if (! fader.isActive())
        return; // fully off: skip, zero CPU

    if (fader.isWakingUp())
        process.reset(); // clear stale state before fading back in

    if (fader.isFullyOn())
    {
        process (l, r, n);
        return;
    }

    // Fading: run on a copy, then crossfade with the untouched input.
    std::memcpy (modL.data(), l, sizeof (float) * (size_t) n);
    std::memcpy (modR.data(), r, sizeof (float) * (size_t) n);
    process (modL.data(), modR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        const float g = fader.next();
        l[i] += (modL[(size_t) i] - l[i]) * g;
        r[i] += (modR[(size_t) i] - r[i]) * g;
    }
}

namespace
{
    // Adapts a module to the small call shape runModule expects.
    template <typename Module, typename Call>
    struct Runner
    {
        Module& m; Call call;
        void reset() noexcept { m.reset(); }
        void operator() (float* l, float* r, int n) noexcept { call (m, l, r, n); }
    };
    template <typename Module, typename Call>
    Runner<Module, Call> runner (Module& m, Call c) { return { m, c }; }
}

// Start of every block: has a sound change been made since the last swap? Over silence
// it is taken at once; otherwise the output fades to dry and the swap waits for it.
void DeepTranceEngine::beginBlockSoundChange() noexcept
{
    if (holdingSound) return;
    if (soundGate.current() == seenSoundChange && ! soundGate.busy()) return;
    if (lastBlockPeak <= 3.0e-5f && ! soundGate.busy())
    {
        seenSoundChange = soundGate.current();   // nothing sounding: nothing to click
        return;
    }
    holdingSound = true;
    transitionTarget = 1.0f;
}

// The output is fully dry, every module has been fed silence for 10 ms and no change is
// still being written: take the whole new sound at once, with no fades or glides left.
void DeepTranceEngine::swapSound() noexcept
{
    seenSoundChange = soundGate.current();
    holdingSound = false;
    transitionTarget = 0.0f;
    params = incoming;
    applyToModules (derive (params, lfoOut[0], lfoOut[1], envOut), true);
    outGainSm.setImmediate (dbToGain (clampf (params.outputGainDb, -24.0f, 24.0f)));
    mixSm.setImmediate (sat01 (params.mix));
    astralSm.setImmediate (params.combo == Combo::AstralProjection ? sat01 (params.comboAmount) : 0.0f);
}

void DeepTranceEngine::processChunk (float* left, float* right, int n, const TransportInfo& t) noexcept
{
    if (holdingSound && ! soundGate.busy() && transition >= 1.0f && swapHold >= swapHoldSamples)
        swapSound();
    clock.beginBlock (t, lastBlockPeak > 3.0e-5f);

    // The sound-change fade (smootherstep: a linear fade's corners tick).
    float fade[kChunk], feed[kChunk];
    const bool fading = transition > 0.0f || transitionTarget > 0.0f;
    if (fading)
        for (int i = 0; i < n; ++i)
        {
            transition = transitionTarget > transition ? std::min (transitionTarget, transition + transitionOut)
                                                       : std::max (transitionTarget, transition - transitionIn);
            swapHold = transition >= 1.0f ? std::min (swapHold + 1, swapHoldSamples) : 0;
            const float x = transition;
            fade[i] = x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
            feed[i] = 1.0f - fade[i];
        }
    // What each module hears fades with the output, so the swap happens on silence: no
    // delay line, grain buffer or reverb records the instant a setting jumps.
    const auto gateFeed = [&]
    {
        if (fading)
            for (int i = 0; i < n; ++i) { left[i] *= feed[i]; right[i] *= feed[i]; }
    };

    // Input gain, then keep a dry copy for the global mix.
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.next();
        left[i]  *= g;
        right[i] *= g;
    }
    std::memcpy (dryL.data(), left,  sizeof (float) * (size_t) n);
    std::memcpy (dryR.data(), right, sizeof (float) * (size_t) n);

    // Modulation sources, then the effective parameters for this chunk.
    for (int i = 0; i < 2; ++i) { lfo[i].setParams (params.lfo[(size_t) i]); lfoOut[i] = lfo[i].tick (clock, n); }
    envFollower.setParams (params.env);
    envOut = envFollower.process (left, right, n);
    applyToModules (derive (params, lfoOut[0], lfoOut[1], envOut), false);

    // Insert chain.
    const auto& clk = clock;
    gateFeed();
    runModule (gateFader,  left, right, n, runner (gate,    [&clk] (TranceGate& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    gateFeed();
    runModule (vowelFader, left, right, n, runner (vowel,   [&clk] (VowelFilter& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    gateFeed();
    runModule (swirlFader, left, right, n, runner (swirl,   [&clk] (Swirl& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    gateFeed();
    runModule (driftFader, left, right, n, runner (drift,   [] (Drift& m, float* l, float* r, int k) { m.process (l, r, k); }));
    gateFeed();
    runModule (acidFader,  left, right, n, runner (acid,    [] (AcidFilter& m, float* l, float* r, int k) { m.process (l, r, k); }));
    gateFeed();
    runModule (buildFader, left, right, n, runner (buildUp, [] (BuildUp& m, float* l, float* r, int k) { m.process (l, r, k); }));

    // Space section: Punch shapes the dry hits and ducks every wet tail after it.
    gateFeed();
    punch.setDetectorHold (fading);
    punch.process (left, right, n, duck.data());
    gateFeed();
    cloud.process (left, right, n, duck.data());
    gateFeed();
    delay.process (left, right, n, duck.data());
    gateFeed();
    abyss.process (left, right, n, duck.data());

    // Astral Projection: slow 8-bar low-pass sweep (300 Hz .. 9 kHz) over everything.
    if (astralSm.getTarget() > 0.0f || astralSm.isSmoothing())
    {
        if (clock.jumped()) astralGlide.arm();
        const double ph = wrap01 (astralGlide.next (clock.ppqAt (0) / 32.0, n));
        astralCoeffs.set (300.0f * std::pow (30.0f, 0.5f + 0.5f * (float) std::sin (kTwoPiD * ph)), 0.9f, sampleRate);
        for (int i = 0; i < n; ++i)
        {
            const float a = astralSm.next();
            left[i]  += (astralLp[0].process (left[i],  astralCoeffs).lp - left[i])  * a;
            right[i] += (astralLp[1].process (right[i], astralCoeffs).lp - right[i]) * a;
        }
    }
    else
    {
        astralLp[0].reset(); astralLp[1].reset();
    }

    // Global dry/wet, then output gain.
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSm.next();
        const float out = outGainSm.next();
        left[i]  = (dryL[(size_t) i] + (left[i]  - dryL[(size_t) i]) * mix) * out;
        right[i] = (dryR[(size_t) i] + (right[i] - dryR[(size_t) i]) * mix) * out;
        if (fading)
        {
            left[i]  += fade[i] * (dryL[(size_t) i] - left[i]);
            right[i] += fade[i] * (dryR[(size_t) i] - right[i]);
        }
    }

    clock.endBlock (n);
}

void DeepTranceEngine::process (float* left, float* right, int numSamples, const TransportInfo& transport) noexcept
{
    ScopedFlushDenormals noDenormals;
    beginBlockSoundChange();
    if (! holdingSound) params = incoming;   // a held sound keeps its params until the swap
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i) peak = std::max (peak, std::max (std::abs (left[i]), std::abs (right[i])));
    const double ppqPerSample = (transport.bpm > 0.0 ? transport.bpm : 138.0) / 60.0 / sampleRate;

    for (int offset = 0; offset < numSamples; offset += kChunk)
    {
        const int n = std::min (kChunk, numSamples - offset);
        TransportInfo t = transport;
        t.ppqAtBlockStart += offset * ppqPerSample; // keep host sync exact across chunks
        processChunk (left + offset, right + offset, n, t);
    }
    for (int i = 0; i < numSamples; ++i) peak = std::max (peak, std::max (std::abs (left[i]), std::abs (right[i])));
    lastBlockPeak = peak;
}
} // namespace dt
