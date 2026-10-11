#include "FractalDelay.h"

namespace dt
{
namespace
{
    /** Gentle saturation in the feedback loop: transparent below ~0.5, never exceeds 1.5. */
    inline float softClip (float x) noexcept
    {
        const float a = std::fabs (x);
        if (a < 0.5f) return x;
        return std::copysign (0.5f + std::tanh (a - 0.5f), x);
    }
}

void FractalDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& l : line)    l.prepare ((int) (kMaxSeconds * sampleRate) + 16);
    for (auto& s : shifter) s.prepare (sampleRate);

    for (auto* s : { &fbSm, &mixSm, &ppSm }) s->prepare (sampleRate, 0.03);
    fbSm.setImmediate (params.feedback);
    mixSm.setImmediate (params.mix);
    ppSm.setImmediate (params.pingPong);

    delayCoef = onePoleCoef (60.0f, sampleRate); // time changes glide like tape, no clicks
    hpCoeffs.set (100.0f, 0.707f, sampleRate);   // keeps repeats from muddying the low end
    spill.prepare (sampleRate);
    setParams (params, 138.0);
    reset();
}

void FractalDelay::reset() noexcept
{
    for (auto& l : line)    l.reset();
    for (auto& s : shifter) s.reset();
    for (auto& s : lp) s.reset();
    for (auto& s : hp) s.reset();
    fbState[0] = fbState[1] = 0.0f;
    delaySamples = targetDelay;
    spill.snap();
}

void FractalDelay::setParams (const DelayParams& p, double bpm) noexcept
{
    params = p;
    const double seconds = p.sync
        ? kDelayDivs[(size_t) std::clamp (p.syncIndex, 0, (int) kDelayDivs.size() - 1)].quarters * 60.0 / std::max (20.0, bpm)
        : clampf (p.timeMs, 10.0f, 2000.0f) * 0.001;
    targetDelay = (float) std::clamp (seconds * sampleRate, 16.0, kMaxSeconds * sampleRate - 16.0);
    spill.setIdleSamples ((int) std::max (targetDelay, delaySamples) + (int) (0.05 * sampleRate));

    fbSm.setTarget (clampf (p.feedback, 0.0f, 0.95f));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
    ppSm.setTarget (clampf (p.pingPong, 0.0f, 1.0f));
    for (auto& s : shifter) s.setSemitones (clampf (p.pitch, -12.0f, 12.0f));
    lpCoeffs.set (clampf (p.toneHz, 500.0f, 18000.0f), 0.6f, sampleRate);
}

void FractalDelay::process (float* left, float* right, int numSamples, const float* duck) noexcept
{
    bool wake = false;
    if (spill.skipBlock (wake)) return;
    if (wake) { for (auto& l : line) l.reset(); for (auto& s : shifter) s.reset(); for (auto& s : lp) s.reset(); for (auto& s : hp) s.reset(); fbState[0] = fbState[1] = 0.0f; delaySamples = targetDelay; }

    const float shifterDelay = shifter[0].averageDelay();

    for (int i = 0; i < numSamples; ++i)
    {
        delaySamples = targetDelay + delayCoef * (delaySamples - targetDelay);
        const float fb  = fbSm.next();
        const float pp  = ppSm.next();
        const float on  = spill.nextGain();
        float dryG, wetG;
        mixGains (mixSm.next(), dryG, wetG);
        dryG = 1.0f + (dryG - 1.0f) * on; // disabled: dry returns to unity, tail keeps ringing

        const float inL = left[i] * on, inR = right[i] * on;

        // Wet taps at the full delay; feedback taps early by the shifter's own delay
        // so repeats stay on the grid after pitch shifting.
        const float wetL = line[0].read (delaySamples);
        const float wetR = line[1].read (delaySamples);
        const float fbDelay = std::max (DelayLine::kMinDelay, delaySamples - shifterDelay);
        float fbL = shifter[0].process (line[0].read (fbDelay));
        float fbR = shifter[1].process (line[1].read (fbDelay));

        fbL = softClip (lp[0].process (hp[0].process (fbL, hpCoeffs).hp, lpCoeffs).lp);
        fbR = softClip (lp[1].process (hp[1].process (fbR, hpCoeffs).hp, lpCoeffs).lp);

        // Ping-pong: input enters on the left (mono), feedback crosses sides.
        const float mono = 0.5f * (inL + inR);
        line[0].push ((1.0f - pp) * inL + pp * mono + fb * ((1.0f - pp) * fbL + pp * fbR));
        line[1].push ((1.0f - pp) * inR             + fb * ((1.0f - pp) * fbR + pp * fbL));

        const float d = duck ? duck[i] : 1.0f;
        left[i]  = left[i]  * dryG + wetL * wetG * d;
        right[i] = right[i] * dryG + wetR * wetG * d;
        spill.trackWet (std::fabs (wetL) + std::fabs (wetR) + std::fabs (fbL) + std::fabs (fbR));
    }
    spill.endBlock();
}
} // namespace dt
