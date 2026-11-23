#include "TranceGate.h"

namespace dt
{
const std::array<TranceGate::Pattern, TranceGate::kNumPatterns>& TranceGate::patterns() noexcept
{
    static const std::array<Pattern, kNumPatterns> table {{
        { "Classic 16ths", { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 } },
        { "Offbeat Pump",  { 0,0,1,1, 0,0,1,1, 0,0,1,1, 0,0,1,1 } },
        { "Driving",       { 1,0,1,1, 1,0,1,1, 1,0,1,0, 1,1,0,1 } },
        { "Gallop",        { 1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1 } },
        { "Stutter",       { 1,1,1,1, 0,0,0,0, 1,0,1,0, 1,1,1,0 } },
        { "Accent Wave",   { 1,.5f,.75f,.5f, 1,.5f,.75f,.5f, 1,.5f,.75f,.5f, 1,.5f,.75f,.5f } },
        { "Half Time",     { 1,1,0,0, 0,0,0,0, 1,1,0,0, 1,0,0,0 } },
        { "Build Roll",    { .25f,.25f,.5f,.5f, .5f,.5f,.75f,.75f, .75f,.75f,1,1, 1,1,1,1 } },
        // 1.1
        { "Dotted Eighths",{ 1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,1 } },
        { "Tresillo",      { 1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,0 } },
        { "Psy Roll",      { 0,1,1,1, 0,1,1,1, 0,1,1,1, 0,1,1,1 } },
        { "Uplift Chop",   { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,1,1 } },
        { "Syncopated",    { 1,0,0,1, 0,1,0,1, 1,0,0,1, 0,1,1,0 } },
        { "Eighths",       { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 } },
        { "Echo Steps",    { 1,.6f,.35f,.2f, 1,.6f,.35f,.2f, 1,.6f,.35f,.2f, 1,.6f,.35f,.2f } },
        { "Swell",         { .1f,.15f,.2f,.25f, .3f,.35f,.45f,.5f, .55f,.6f,.7f,.75f, .8f,.85f,.95f,1 } } }};
    return table;
}

const float* TranceGate::stepsFor (const GateParams& p) noexcept
{
    if (p.patternExt > 0)
        return patterns()[(size_t) std::clamp (kNumLegacy + p.patternExt - 1, 0, kNumPatterns - 1)].steps.data();
    if (p.pattern > 0)
        return patterns()[(size_t) std::clamp (p.pattern - 1, 0, kNumLegacy - 1)].steps.data();
    return p.steps.data();
}

void TranceGate::stepAt (double ppq, double spq, float swing, int& index, float& frac) noexcept
{
    // The tiny epsilon makes exact grid boundaries round the same way regardless of how
    // the host splits blocks (determinism).
    const double stepPos = ppq * spq + 1.0e-7;
    double whole = std::floor (stepPos);
    double f     = stepPos - whole;
    if (swing > 0.0f)
    {
        // Within each pair of steps the second starts at b (1.0 = straight, 1.5 = full swing);
        // each step's own position runs 0..1 across its (now longer or shorter) length.
        const double pairStart = 2.0 * std::floor (whole * 0.5);
        const double u = stepPos - pairStart;                 // 0..2 within the pair
        const double b = 1.0 + 0.5 * (double) clampf (swing, 0.0f, 1.0f);
        if (u < b) { whole = pairStart;       f = u / b; }
        else       { whole = pairStart + 1.0; f = (u - b) / (2.0 - b); }
    }
    index = (int) ((int64_t) whole % kNumSteps + kNumSteps) % kNumSteps;
    frac  = (float) f;
}

void TranceGate::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    depthSm.prepare (sampleRate, 0.02);
    filterSm.prepare (sampleRate, 0.02);
    depthSm.setImmediate (params.depth);
    filterSm.setImmediate (params.filterAmount);
    setParams (params);
    reset();
}

void TranceGate::reset() noexcept
{
    env = envR = 1.0f;
    coeffCountdown = 0;
    for (auto& s : lp) s.reset();
}

void TranceGate::setParams (const GateParams& p) noexcept
{
    params = p;
    params.rateIndex = std::clamp (p.rateIndex, 0, (int) kRates.size() - 1);
    params.pattern   = std::clamp (p.pattern, 0, kNumLegacy);
    params.patternExt = std::clamp (p.patternExt, 0, kNumPatterns - kNumLegacy);
    params.length    = clampf (p.length, 0.05f, 1.0f);
    attackCoef  = onePoleCoef (clampf (p.attackMs,  0.5f, 200.0f), sampleRate);
    releaseCoef = onePoleCoef (clampf (p.releaseMs, 1.0f, 500.0f), sampleRate);
    depthSm.setTarget (clampf (p.depth, 0.0f, 1.0f));
    filterSm.setTarget (clampf (p.filterAmount, 0.0f, 1.0f));
}

void TranceGate::process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept
{
    const double spq    = kRates[(size_t) params.rateIndex].stepsPerQuarter;
    const float* steps  = stepsFor (params);
    const float  swing  = clampf (params.swing, 0.0f, 1.0f);
    const float  stereo = clampf (params.stereo, 0.0f, 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        int idx; float frac;
        stepAt (clock.ppqAt (i), spq, swing, idx, frac);

        // Envelopes follow the step target with separate attack/release. Ping-pong: the
        // off side of each step closes by `stereo` (0 = both sides identical, as in 1.0).
        const float target  = frac < params.length ? clampf (steps[idx], 0.0f, 1.0f) : 0.0f;
        const bool  odd     = (idx & 1) != 0;
        const float targetL = odd ? target * (1.0f - stereo) : target;
        const float targetR = odd ? target : target * (1.0f - stereo);
        env  = targetL + (targetL > env  ? attackCoef : releaseCoef) * (env  - targetL);
        envR = targetR + (targetR > envR ? attackCoef : releaseCoef) * (envR - targetR);

        const float depth = depthSm.next();
        const float volL  = 1.0f - depth * (1.0f - env);
        const float volR  = 1.0f - depth * (1.0f - envR);
        const float filt  = filterSm.next();

        float l = left[i], r = right[i];

        if (filt > 0.0001f)
        {
            // Closed steps pull the cutoff down up to 8 octaves (20 kHz -> ~78 Hz).
            if (--coeffCountdown <= 0)
            {
                const float e = 0.5f * (env + envR);
                lpCoeffs.set (20000.0f * std::exp2 (-8.0f * filt * (1.0f - e)), 0.8f, sampleRate);
                coeffCountdown = 8;
            }
            l = lp[0].process (l, lpCoeffs).lp;
            r = lp[1].process (r, lpCoeffs).lp;
            filterActive = true;
        }
        else if (filterActive)
        {
            // Filter just went idle: clear its memory so re-engaging starts clean.
            for (auto& s : lp) s.reset();
            coeffCountdown = 0;
            filterActive = false;
        }

        left[i]  = l * volL;
        right[i] = r * volR;
    }
}
} // namespace dt
