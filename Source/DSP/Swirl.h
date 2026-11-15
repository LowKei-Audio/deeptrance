#pragma once
// Dual-Band Phase & Flange ("The Swirl"). An LR4 crossover splits the signal;
// the high band gets the tempo-synced phaser and/or flanger, while the low band
// stays solid (blend some in with `lowAmount`). Dry is taken from the same
// crossover so dry/wet blending never notches at the crossover frequency.

#include "Common.h"
#include "DelayLine.h"
#include "Filters.h"
#include "TransportClock.h"

namespace dt
{
enum class SwirlMode { Phaser = 0, Flanger, Both };

struct SwirlParams
{
    SwirlMode mode      = SwirlMode::Phaser;
    int       rateIndex = 5;      // index into kSyncDivs (2 Bars)
    float     depth     = 0.4f;   // 0..1
    float     feedback  = 0.4f;   // 0..0.95
    float     stereoDeg = 90.0f;  // 0..180, LFO offset between channels
    float     xoverHz   = 250.0f; // 60..1000
    float     lowAmount = 0.0f;   // 0..1, how much of the low band is swirled
    float     mix       = 0.35f;  // 0..1
    float     speedMul  = 1.0f;   // set by Build-Up: >1 speeds the LFO up (free-running while > 1)
};

class Swirl
{
public:
    static constexpr int   kPhaserStages     = 6;
    static constexpr float kFlangerMaxDelayMs = 6.0f;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const SwirlParams& p) noexcept;
    void process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept;

private:
    struct Channel
    {
        LR4Crossover xover;
        Allpass1     stages[kPhaserStages];
        float        phaserFb = 0.0f, apCoef = 0.0f;
        DelayLine    flanger;
        float        flangerFb = 0.0f;
    };

    float runPhaser  (Channel& c, float x, float fb) noexcept;
    float runFlanger (Channel& c, float x, float delaySamples, float fb) noexcept;

    double      sampleRate = 44100.0;
    SwirlParams params;
    Smoother    depthSm, fbSm, lowSm, mixSm;
    Channel     ch[2];
    int         coeffCountdown = 0;
    float       currentXover = 0.0f;
    double      freePhase = 0.0; // continues smoothly from the synced phase when sped up
    CycleGlide  glide;           // the synced sweep across playhead jumps
};
} // namespace dt
