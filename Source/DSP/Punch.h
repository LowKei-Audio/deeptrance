#pragma once
// Psy-Transient Shaper ("Punch"). A fast and a slow envelope follower compare
// how quickly the level is rising: the difference marks attacks. Attack boosts
// (or softens) those moments, Sustain shapes the body, and the same detector
// briefly ducks the delay/reverb/granular wet so hits cut through the space.

#include "Common.h"

namespace dt
{
struct PunchParams
{
    float attack     = 0.4f;  // -1..1
    float sustain    = 0.0f;  // -1..1
    float duck       = 0.5f;  // 0..1  (up to 24 dB of wet ducking)
    float duckRelMs  = 80.0f; // 20..500
};

class Punch
{
public:
    static constexpr float kMaxShapeDb = 12.0f;
    static constexpr float kMaxDuckDb  = 24.0f;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const PunchParams& p) noexcept;
    void setEnabled (bool on) noexcept { enabled = on; }
    void snapEnabled() noexcept        { amount = enabled ? 1.0f : 0.0f; }
    /** True while the engine fades a sound change in or out: the detector holds still, so
        that fade is neither punched nor allowed to duck the tails. */
    void setDetectorHold (bool hold) noexcept { detectorHold = hold; }
    bool isIdle() const noexcept       { return ! enabled && amount <= 0.0f; }

    /** Shapes left/right in place and writes the wet-duck gain (0..1) for each sample. */
    void process (float* left, float* right, int numSamples, float* duckOut) noexcept;

private:
    double      sampleRate = 44100.0;
    PunchParams params;
    bool        enabled = false, detectorHold = false, heldLast = false;
    float       holdCoef = 0.99f;
    float       amount = 0.0f, amountStep = 0.001f;
    float       fastEnv = 0.0f, slowEnv = 0.0f, duckEnv = 0.0f;
    float       fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, duckRel = 0;
    Smoother    attackSm, sustainSm, duckSm;
};
} // namespace dt
