#pragma once
// Acid module: resonant 303-style low-pass with an accent envelope (the
// "squelch"), followed by 3-band distortion. Waveshaping uses first-order
// antiderivative anti-aliasing (ADAA), which removes most aliasing with no
// added latency, so DeepTrance stays zero-latency.

#include "Common.h"
#include "Filters.h"

namespace dt
{
struct AcidParams
{
    float cutoffHz  = 800.0f; // 60..8000
    float resonance = 0.7f;   // 0..1
    float envMod    = 0.5f;   // 0..1 (up to +4 octaves on each note)
    float decayMs   = 200.0f; // 20..1000 envelope release
    float drive     = 0.4f;   // 0..1
    float mix       = 1.0f;
};

class AcidFilter
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const AcidParams& p) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    /** tanh(g*x) with ADAA1; one per band per channel. */
    struct AdaaTanh
    {
        float x1 = 0.0f;
        float process (float x, float g) noexcept;
    };

    double     sampleRate = 44100.0;
    AcidParams params;
    Smoother   cutoffSm, resSm, envModSm, driveSm, mixSm;
    float      env = 0.0f, envAtt = 0.0f, envRel = 0.0f;
    SvfCoeffs  c1, c2;
    SvfState   f1[2], f2[2];
    LR4Crossover xLow[2], xHigh[2];
    AdaaTanh   shaper[2][3];
    int        coeffCountdown = 0;
    // Auto-gain: tracks power before/after the distortion so Drive changes tone, not volume.
    float      powIn = 0.0f, powOut = 0.0f, powCoef = 0.0f;
};
} // namespace dt
