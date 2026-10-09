#pragma once
// Trance Gate ("Rhythm Enforcer"): a 16-step, tempo-synced volume + filter gate.
// Each step has a level; `length` sets how much of each step is open.
// Edges are shaped by attack/release so the gate never clicks.

#include "Common.h"
#include "Filters.h"
#include "TransportClock.h"

namespace dt
{
struct GateParams
{
    int   rateIndex    = 1;     // index into TranceGate::kRates
    int   pattern      = 1;     // 0 = custom steps, 1..N = factory pattern
    float depth        = 1.0f;  // 0..1, how far closed steps duck the volume
    float length       = 0.6f;  // 0.05..1, open portion of each step
    float attackMs     = 2.0f;
    float releaseMs    = 30.0f;
    float filterAmount = 0.0f;  // 0..1, closed steps also sweep a low-pass down
    std::array<float, 16> steps {}; // custom step levels, 0..1
};

class TranceGate
{
public:
    struct Rate { const char* name; double stepsPerQuarter; };
    static constexpr std::array<Rate, 5> kRates {{
        { "1/8", 2.0 }, { "1/16", 4.0 }, { "1/32", 8.0 }, { "1/8T", 3.0 }, { "1/16T", 6.0 } }};

    static constexpr int kNumSteps = 16;
    struct Pattern { const char* name; std::array<float, kNumSteps> steps; };
    static const std::array<Pattern, 8>& patterns() noexcept;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const GateParams& p) noexcept;
    void process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept;

    /** Current envelope (0..1); useful for UI metering and tests. */
    float getEnvelope() const noexcept { return env; }

private:
    double     sampleRate = 44100.0;
    GateParams params;
    Smoother   depthSm, filterSm;
    float      env = 1.0f, attackCoef = 0.0f, releaseCoef = 0.0f;
    SvfCoeffs  lpCoeffs;
    SvfState   lp[2];
    int        coeffCountdown = 0;
    bool       filterActive = false;
};
} // namespace dt
