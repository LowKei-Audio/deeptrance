#pragma once
// Pitch Drift / Wow & Flutter: a short modulated delay that detunes its signal
// like an aging tape machine. Wow = slow sine + random wander, flutter = fast
// shimmer. Blended with the dry signal it gives the wide, slightly detuned
// 90s-trance width. Depth is specified in cents and converted to delay swing.

#include "Common.h"
#include "DelayLine.h"

namespace dt
{
struct DriftParams
{
    float amount  = 0.25f; // 0..1  -> wow up to 25 cents
    float rateHz  = 0.6f;  // 0.1..3 wow speed (free-running, like a real tape transport)
    float flutter = 0.15f; // 0..1  -> flutter up to 8 cents
    float stereo  = 0.6f;  // 0..1  channel decorrelation
    float mix     = 0.35f; // 0..1
};

class Drift
{
public:
    static constexpr float kMaxWowCents     = 25.0f;
    static constexpr float kMaxFlutterCents = 8.0f;
    static constexpr float kFlutterHz       = 7.0f;
    static constexpr float kMaxSwingMs      = 6.0f; // caps delay swing (and its timing offset) at slow rates
    static constexpr float kBaseMs          = 0.5f;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const DriftParams& p) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    double      sampleRate = 44100.0;
    DriftParams params;
    Smoother    amountSm, flutterSm, stereoSm, mixSm, rateSm;
    DelayLine   line[2];
    double      wowPhase = 0.0, flutterPhase = 0.0;
    float       wander[2] {}, wanderTarget[2] {};
    int         wanderCountdown = 0;
    Random      rng { 0xD71F7u };
};
} // namespace dt
