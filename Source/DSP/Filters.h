#pragma once
// Filter building blocks: TPT state-variable filter (Cytomic/Zavalishin) and
// a first-order allpass. Coefficients are kept separate from state so one
// coefficient set can drive both stereo channels.

#include "Common.h"

namespace dt
{
struct SvfCoeffs
{
    float g = 0.0f, k = 1.4142f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void set (float cutoffHz, float q, double sampleRate) noexcept
    {
        const float fc = clampf (cutoffHz, 10.0f, (float) sampleRate * 0.49f);
        g  = std::tan (kPi * fc / (float) sampleRate);
        k  = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

class SvfState
{
public:
    struct Out { float lp, bp, hp; };

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    Out process (float v0, const SvfCoeffs& c) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - c.k * v1 - v2 };
    }

    /** Bandpass normalised to unity gain at the centre frequency. */
    float bandpassNorm (float x, const SvfCoeffs& c) noexcept { return c.k * process (x, c).bp; }

private:
    float ic1 = 0.0f, ic2 = 0.0f;
};

/** Linkwitz-Riley 4th-order crossover (two cascaded Butterworth SVFs per band).
    low + high sums to an allpass, so recombining keeps a flat magnitude. */
class LR4Crossover
{
public:
    void set (float freq, double sampleRate) noexcept { coeffs.set (freq, 0.70710678f, sampleRate); }
    void reset() noexcept { for (auto& s : lp) s.reset(); for (auto& s : hp) s.reset(); }

    void process (float x, float& low, float& high) noexcept
    {
        low  = lp[1].process (lp[0].process (x, coeffs).lp, coeffs).lp;
        high = hp[1].process (hp[0].process (x, coeffs).hp, coeffs).hp;
    }

private:
    SvfCoeffs coeffs;
    SvfState  lp[2], hp[2];
};

/** First-order allpass, H(z) = (a + z^-1) / (1 + a z^-1). */
class Allpass1
{
public:
    static float coefFor (float freq, double sampleRate) noexcept
    {
        const float t = std::tan (kPi * clampf (freq, 10.0f, (float) sampleRate * 0.49f) / (float) sampleRate);
        return (t - 1.0f) / (t + 1.0f);
    }
    void  reset() noexcept { s = 0.0f; }
    float process (float x, float a) noexcept
    {
        const float y = a * x + s;
        s = x - a * y;
        return y;
    }
private:
    float s = 0.0f;
};
} // namespace dt
