#pragma once
// Cloud Generator: real-time granular engine on the live input.
// A rolling 4-second capture buffer is read by up to 96 overlapping grains.
// Freeze stops capturing, so grains loop the frozen moment indefinitely.

#include "Common.h"
#include "DelayLine.h"
#include "PitchShifter.h"

namespace dt
{
struct CloudParams
{
    float sizeMs     = 80.0f;  // 5..500 grain length
    float density    = 25.0f;  // 1..200 grains per second
    float positionMs = 150.0f; // 0..2000 how far back grains start
    float spray      = 0.2f;   // 0..1 random start offset (up to 500 ms)
    float pitch      = 0.0f;   // -24..+24 semitones
    float jitter     = 0.1f;   // 0..1 random per-grain detune (cubic: small values = cents)
    float shimmer    = 0.0f;   // 0..1 chance a grain jumps up an octave (or two)
    float stereo     = 0.7f;   // 0..1 random pan spread
    float mix        = 0.5f;
    bool  freeze     = false;
};

class Cloud
{
public:
    static constexpr int    kMaxGrains   = 96;
    static constexpr double kBufferSecs  = 4.0;
    static constexpr float  kMaxJitterSt = 12.0f;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const CloudParams& p) noexcept;
    void setEnabled (bool on) noexcept { spill.setEnabled (on); }
    void snapEnabled() noexcept        { spill.snap(); }

    void process (float* left, float* right, int numSamples, const float* duck) noexcept;

    int activeGrains() const noexcept;

private:
    struct Grain
    {
        bool  active = false;
        float dist = 0.0f;   // samples behind the write head
        float ratio = 1.0f;  // playback speed (pitch)
        float pos = 0.0f;    // 0..1 progress through the grain
        float inc = 0.0f;
        float gl = 0.0f, gr = 0.0f;
    };

    void clearState() noexcept;
    void spawn() noexcept;

    double      sampleRate = 44100.0;
    CloudParams params;
    Spillover   spill;
    DelayLine   buffer[2];
    std::array<Grain, kMaxGrains> grains {};
    float       countdown = 0.0f;
    bool        frozen = false;
    Smoother    mixSm, normSm;
    Random      rng { 0xC10D5u };
};
} // namespace dt
