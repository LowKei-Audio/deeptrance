#include "TranceGate.h"

namespace dt
{
const std::array<TranceGate::Pattern, 8>& TranceGate::patterns() noexcept
{
    static const std::array<Pattern, 8> table {{
        { "Classic 16ths", { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 } },
        { "Offbeat Pump",  { 0,0,1,1, 0,0,1,1, 0,0,1,1, 0,0,1,1 } },
        { "Driving",       { 1,0,1,1, 1,0,1,1, 1,0,1,0, 1,1,0,1 } },
        { "Gallop",        { 1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1 } },
        { "Stutter",       { 1,1,1,1, 0,0,0,0, 1,0,1,0, 1,1,1,0 } },
        { "Accent Wave",   { 1,.5f,.75f,.5f, 1,.5f,.75f,.5f, 1,.5f,.75f,.5f, 1,.5f,.75f,.5f } },
        { "Half Time",     { 1,1,0,0, 0,0,0,0, 1,1,0,0, 1,0,0,0 } },
        { "Build Roll",    { .25f,.25f,.5f,.5f, .5f,.5f,.75f,.75f, .75f,.75f,1,1, 1,1,1,1 } } }};
    return table;
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
    env = 1.0f;
    coeffCountdown = 0;
    for (auto& s : lp) s.reset();
}

void TranceGate::setParams (const GateParams& p) noexcept
{
    params = p;
    params.rateIndex = std::clamp (p.rateIndex, 0, (int) kRates.size() - 1);
    params.pattern   = std::clamp (p.pattern, 0, (int) patterns().size());
    params.length    = clampf (p.length, 0.05f, 1.0f);
    attackCoef  = onePoleCoef (clampf (p.attackMs,  0.5f, 200.0f), sampleRate);
    releaseCoef = onePoleCoef (clampf (p.releaseMs, 1.0f, 500.0f), sampleRate);
    depthSm.setTarget (clampf (p.depth, 0.0f, 1.0f));
    filterSm.setTarget (clampf (p.filterAmount, 0.0f, 1.0f));
}

void TranceGate::process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept
{
    const double spq   = kRates[(size_t) params.rateIndex].stepsPerQuarter;
    const auto*  steps = params.pattern > 0 ? patterns()[(size_t) params.pattern - 1].steps.data()
                                            : params.steps.data();

    for (int i = 0; i < numSamples; ++i)
    {
        // Step position from musical time. The tiny epsilon makes exact grid boundaries
        // round the same way regardless of how the host splits blocks (determinism).
        const double stepPos = clock.ppqAt (i) * spq + 1.0e-7;
        const double whole   = std::floor (stepPos);
        const int    idx     = (int) ((int64_t) whole % kNumSteps + kNumSteps) % kNumSteps;
        const float  frac    = (float) (stepPos - whole);

        // Envelope follows the step target with separate attack/release.
        const float target = frac < params.length ? clampf (steps[idx], 0.0f, 1.0f) : 0.0f;
        const float coef   = target > env ? attackCoef : releaseCoef;
        env = target + coef * (env - target);

        const float depth = depthSm.next();
        const float vol   = 1.0f - depth * (1.0f - env);
        const float filt  = filterSm.next();

        float l = left[i], r = right[i];

        if (filt > 0.0001f)
        {
            // Closed steps pull the cutoff down up to 8 octaves (20 kHz -> ~78 Hz).
            if (--coeffCountdown <= 0)
            {
                lpCoeffs.set (20000.0f * std::exp2 (-8.0f * filt * (1.0f - env)), 0.8f, sampleRate);
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

        left[i]  = l * vol;
        right[i] = r * vol;
    }
}
} // namespace dt
