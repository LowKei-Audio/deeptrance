#include "AcidFilter.h"

namespace dt
{
namespace
{
    /** Numerically stable log(cosh(u)). */
    inline float logCosh (float u) noexcept
    {
        const float a = std::fabs (u);
        return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
    }

    // Band drive weights: bass stays tighter, highs get the most grit.
    constexpr float kBandDrive[3] = { 6.0f, 18.0f, 12.0f };
}

float AcidFilter::AdaaTanh::process (float x, float g) noexcept
{
    // y = (F(x) - F(x1)) / (x - x1), F = antiderivative of tanh(g*x) = logcosh(g*x) / g
    const float dx = x - x1;
    float y;
    if (std::fabs (dx) < 1.0e-4f)
        y = std::tanh (g * 0.5f * (x + x1));
    else
        y = (logCosh (g * x) - logCosh (g * x1)) / (g * dx);
    x1 = x;
    return y;
}

void AcidFilter::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto* s : { &cutoffSm, &resSm, &envModSm, &driveSm, &mixSm }) s->prepare (sampleRate, 0.03);
    cutoffSm.setImmediate (std::log2 (params.cutoffHz));
    resSm.setImmediate (params.resonance);
    envModSm.setImmediate (params.envMod);
    driveSm.setImmediate (params.drive);
    mixSm.setImmediate (params.mix);
    envAtt = onePoleCoef (1.0f, sampleRate);
    powCoef = onePoleCoef (300.0f, sampleRate); // slow enough not to pump on single notes
    for (auto& x : xLow)  x.set (250.0f, sampleRate);
    for (auto& x : xHigh) x.set (2500.0f, sampleRate);
    setParams (params);
    reset();
}

void AcidFilter::reset() noexcept
{
    env = 0.0f;
    powIn = powOut = 0.0f;
    coeffCountdown = 0;
    for (auto& s : f1) s.reset();
    for (auto& s : f2) s.reset();
    for (auto& x : xLow) x.reset();
    for (auto& x : xHigh) x.reset();
    for (auto& ch : shaper) for (auto& s : ch) s.x1 = 0.0f;
}

void AcidFilter::setParams (const AcidParams& p) noexcept
{
    params = p;
    cutoffSm.setTarget (std::log2 (clampf (p.cutoffHz, 60.0f, 8000.0f))); // smooth in octaves
    resSm.setTarget (clampf (p.resonance, 0.0f, 1.0f));
    envModSm.setTarget (clampf (p.envMod, 0.0f, 1.0f));
    driveSm.setTarget (clampf (p.drive, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
    envRel = onePoleCoef (clampf (p.decayMs, 20.0f, 1000.0f), sampleRate);
}

void AcidFilter::process (float* left, float* right, int numSamples) noexcept
{
    float* io[2] = { left, right };

    for (int i = 0; i < numSamples; ++i)
    {
        const float octaves = cutoffSm.next();
        const float res     = resSm.next();
        const float envMod  = envModSm.next();
        const float drive   = driveSm.next();
        const float mix     = mixSm.next();

        // Accent envelope: level in dB mapped to 0..1 (-48 dB .. 0 dB), so it reacts
        // the same to quiet and loud sources.
        const float lvl = std::max (std::fabs (left[i]), std::fabs (right[i]));
        env = lvl + (lvl > env ? envAtt : envRel) * (env - lvl);
        const float envNorm = clampf ((20.0f * std::log10 (env + 1.0e-6f) + 48.0f) / 48.0f, 0.0f, 1.0f);

        if (--coeffCountdown <= 0)
        {
            const float fc = clampf (std::exp2 (octaves + envMod * 4.0f * envNorm), 20.0f, 18000.0f);
            c1.set (fc, 0.5f + res * 11.5f, sampleRate); // resonant stage
            c2.set (fc, 0.6f, sampleRate);               // second pole pair: 24 dB/oct slope
            coeffCountdown = 4;
        }

        float filtered[2], shaped[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            // Gentle input saturation keeps high resonance from exploding on loud input.
            float x = std::tanh (io[ch][i] * 1.2f) / 1.2f;
            x = f2[ch].process (f1[ch].process (x, c1).lp, c2).lp;
            filtered[ch] = x;

            // 3-band distortion: <250 Hz, 250-2500 Hz, >2500 Hz.
            float low, rest, mid, high;
            xLow[ch].process (x, low, rest);
            xHigh[ch].process (rest, mid, high);
            const float bands[3] = { low, mid, high };
            float y = 0.0f;
            for (int b = 0; b < 3; ++b)
                y += shaper[ch][b].process (bands[b], 1.0f + drive * kBandDrive[b]);
            shaped[ch] = y;
        }

        // Level-match the distortion to its input (+2 dB at full drive for a sense of "more").
        powIn  = 0.5f * (filtered[0] * filtered[0] + filtered[1] * filtered[1]) + powCoef * (powIn  - 0.5f * (filtered[0] * filtered[0] + filtered[1] * filtered[1]));
        powOut = 0.5f * (shaped[0] * shaped[0] + shaped[1] * shaped[1])         + powCoef * (powOut - 0.5f * (shaped[0] * shaped[0] + shaped[1] * shaped[1]));
        const float match = clampf (std::sqrt ((powIn + 1.0e-9f) / (powOut + 1.0e-9f)), 0.125f, 2.0f);
        const float comp  = lerpf (1.0f, match, std::min (1.0f, drive * 4.0f)) * (1.0f + 0.25f * drive);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float dry = io[ch][i];
            io[ch][i] = dry + (shaped[ch] * comp - dry) * mix;
        }
    }
}
} // namespace dt
