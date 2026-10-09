#include "Drift.h"

namespace dt
{
namespace
{
    // Peak delay swing (seconds) that produces `cents` of peak detune for a sine at `hz`.
    // For d(t) = A sin(2*pi*f*t) the peak pitch ratio deviation is 2*pi*f*A.
    float swingSecondsFor (float cents, float hz) noexcept
    {
        const float ratioDev = std::exp2 (cents / 1200.0f) - 1.0f;
        return ratioDev / (kTwoPi * std::max (0.05f, hz));
    }
}

void Drift::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const int maxSamples = (int) std::ceil ((kBaseMs + 2.0f * kMaxSwingMs + 2.0f) * 0.001 * sampleRate);
    for (auto& l : line) l.prepare (maxSamples);

    for (auto* s : { &amountSm, &flutterSm, &stereoSm, &mixSm, &rateSm }) s->prepare (sampleRate, 0.05);
    amountSm.setImmediate (params.amount);
    flutterSm.setImmediate (params.flutter);
    stereoSm.setImmediate (params.stereo);
    mixSm.setImmediate (params.mix);
    rateSm.setImmediate (params.rateHz);
    reset();
}

void Drift::reset() noexcept
{
    for (auto& l : line) l.reset();
    wowPhase = flutterPhase = 0.0;
    wander[0] = wander[1] = wanderTarget[0] = wanderTarget[1] = 0.0f;
    wanderCountdown = 0;
}

void Drift::setParams (const DriftParams& p) noexcept
{
    params = p;
    amountSm.setTarget (clampf (p.amount, 0.0f, 1.0f));
    flutterSm.setTarget (clampf (p.flutter, 0.0f, 1.0f));
    stereoSm.setTarget (clampf (p.stereo, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
    rateSm.setTarget (clampf (p.rateHz, 0.1f, 3.0f));
}

void Drift::process (float* left, float* right, int numSamples) noexcept
{
    const float  sr         = (float) sampleRate;
    const float  maxSwing   = kMaxSwingMs * 0.001f;
    const float  wanderCoef = onePoleCoef (400.0f, sampleRate); // slow, tape-like wander
    float* io[2] = { left, right };

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount  = amountSm.next();
        const float flutter = flutterSm.next();
        const float stereo  = stereoSm.next();
        const float mix     = mixSm.next();
        const float rate    = rateSm.next();

        wowPhase     = wrap01 (wowPhase + rate / sampleRate);
        flutterPhase = wrap01 (flutterPhase + kFlutterHz / sampleRate);

        // New random wander targets ~6 times a second, smoothed by a slow one-pole.
        if (--wanderCountdown <= 0)
        {
            wanderTarget[0] = rng.bipolar();
            wanderTarget[1] = rng.bipolar();
            wanderCountdown = (int) (sampleRate / 6.0);
        }

        const float wowSwing     = std::min (maxSwing, swingSecondsFor (amount  * kMaxWowCents,     rate))      * sr;
        const float flutterSwing = std::min (maxSwing, swingSecondsFor (flutter * kMaxFlutterCents, kFlutterHz)) * sr;
        const float base         = kBaseMs * 0.001f * sr + wowSwing + flutterSwing;

        for (int c = 0; c < 2; ++c)
        {
            wander[c] = wanderTarget[c] + wanderCoef * (wander[c] - wanderTarget[c]);

            // Right channel: phase-offset LFOs and its own wander, scaled by `stereo`.
            const double off   = (c == 1) ? 0.5 * stereo : 0.0;
            const float  w     = (c == 1) ? lerpf (wander[0], wander[1], stereo) : wander[0];
            const float  wow   = 0.7f * (float) std::sin (kTwoPiD * wrap01 (wowPhase + off)) + 0.3f * w;
            const float  flut  = (float) std::sin (kTwoPiD * wrap01 (flutterPhase + off * 0.37));

            const float dry = io[c][i];
            line[c].push (dry);
            const float wet = line[c].read (base + wowSwing * wow + flutterSwing * flut);
            io[c][i] = dry + (wet - dry) * mix;
        }
    }
}
} // namespace dt
