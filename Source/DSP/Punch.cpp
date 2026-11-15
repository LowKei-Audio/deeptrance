#include "Punch.h"

namespace dt
{
void Punch::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fastAtt = onePoleCoef (0.5f, sampleRate);
    fastRel = onePoleCoef (30.0f, sampleRate);
    slowAtt = onePoleCoef (15.0f, sampleRate);
    slowRel = onePoleCoef (180.0f, sampleRate);
    holdCoef = onePoleCoef (5.0f, sampleRate);
    amountStep = 1.0f / (float) std::max (1.0, sampleRate * 0.015);
    for (auto* s : { &attackSm, &sustainSm, &duckSm }) s->prepare (sampleRate, 0.03);
    attackSm.setImmediate (params.attack);
    sustainSm.setImmediate (params.sustain);
    duckSm.setImmediate (params.duck);
    setParams (params);
    reset();
}

void Punch::reset() noexcept
{
    fastEnv = slowEnv = duckEnv = 0.0f;
    snapEnabled();
}

void Punch::setParams (const PunchParams& p) noexcept
{
    params = p;
    attackSm.setTarget (clampf (p.attack, -1.0f, 1.0f));
    sustainSm.setTarget (clampf (p.sustain, -1.0f, 1.0f));
    duckSm.setTarget (clampf (p.duck, 0.0f, 1.0f));
    duckRel = onePoleCoef (clampf (p.duckRelMs, 20.0f, 500.0f), sampleRate);
}

void Punch::process (float* left, float* right, int numSamples, float* duckOut) noexcept
{
    if (isIdle())
    {
        std::fill (duckOut, duckOut + numSamples, 1.0f);
        return;
    }

    constexpr float kFloor = 1.0e-5f; // -100 dB: below this nothing is shaped

    for (int i = 0; i < numSamples; ++i)
    {
        amount = enabled ? std::min (1.0f, amount + amountStep) : std::max (0.0f, amount - amountStep);
        const float att = attackSm.next() * amount;
        const float sus = sustainSm.next() * amount;
        const float dk  = duckSm.next() * amount;

        const float lvl = std::max (std::fabs (left[i]), std::fabs (right[i]));
        // A sound-change fade is never taken for a hit: the detector eases to neutral (no
        // shaping, no duck) over ~5 ms, and when the fade is over it starts again from the
        // level actually there, so it neither jumps nor sees a false attack.
        if (detectorHold)
        {
            slowEnv = fastEnv + holdCoef * (slowEnv - fastEnv);
            heldLast = true;
        }
        else
        {
            if (heldLast) { fastEnv = slowEnv = lvl; heldLast = false; }
            fastEnv = lvl + (lvl > fastEnv ? fastAtt : fastRel) * (fastEnv - lvl);
            slowEnv = lvl + (lvl > slowEnv ? slowAtt : slowRel) * (slowEnv - lvl);
        }

        // Positive while the level is rising faster than the slow follower (an attack),
        // negative in the decaying body of the sound.
        float diffDb = 0.0f;
        if (fastEnv > kFloor && slowEnv > kFloor)
            diffDb = 20.0f * std::log10 (fastEnv / slowEnv);

        const float rise = clampf (diffDb / 12.0f, 0.0f, 1.0f);  // 0..1 attack strength
        const float fall = clampf (-diffDb / 12.0f, 0.0f, 1.0f); // 0..1 body/tail strength
        const float gainDb = clampf (att * rise * kMaxShapeDb + sus * fall * kMaxShapeDb, -kMaxShapeDb, kMaxShapeDb);
        const float g = dbToGain (gainDb);
        left[i]  *= g;
        right[i] *= g;

        // Ducking: instant on the attack, configurable release.
        duckEnv = rise > duckEnv ? rise : rise + duckRel * (duckEnv - rise);
        duckOut[i] = dbToGain (-kMaxDuckDb * dk * duckEnv);
    }
}
} // namespace dt
