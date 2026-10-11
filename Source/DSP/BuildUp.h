#pragma once
// Build-Up ("Tension Generator") audio stage: a 24 dB/oct high-pass that sweeps
// from 20 Hz to 1.5 kHz plus a band-passed white-noise riser that follows the
// sweep. The other Build-Up moves (more reverb, faster swirl) are parameter
// changes applied by the engine.

#include "Common.h"
#include "Filters.h"

namespace dt
{
class BuildUp
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (float amount, float noiseLevel) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    static float cutoffFor (float amount) noexcept { return 20.0f * std::pow (75.0f, std::pow (clampf (amount, 0.0f, 1.0f), 1.5f)); }

private:
    double    sampleRate = 44100.0;
    Smoother  amountSm, noiseSm;
    SvfCoeffs hp1, hp2, bp;
    SvfState  hpState[2][2], bpState[2];
    Random    rngL { 0xB111Du }, rngR { 0x5EEDu };
    int       coeffCountdown = 0;
};
} // namespace dt
