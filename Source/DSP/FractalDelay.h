#pragma once
// Ping-Pong Fractal Delay: tempo-synced stereo delay whose feedback loop runs
// through its own pitch shifter and filters. Each repeat bounces L/R and is
// shifted again, so echoes climb ("crystalline") or fall ("murky") in pitch
// while getting darker.

#include "Common.h"
#include "DelayLine.h"
#include "Filters.h"
#include "PitchShifter.h"
#include "TransportClock.h"

namespace dt
{
struct DelayParams
{
    bool  sync      = true;
    int   syncIndex = 6;       // index into kDelayDivs ("1/8D", the classic trance 3/16)
    float timeMs    = 375.0f;  // used when sync is off
    float feedback  = 0.45f;   // 0..0.95
    float pitch     = 0.0f;    // semitones added per repeat, -12..+12
    float toneHz    = 6000.0f; // low-pass in the feedback loop
    float pingPong  = 1.0f;    // 0 = straight stereo, 1 = full ping-pong
    float mix       = 0.2f;
};

inline constexpr std::array<SyncDiv, 12> kDelayDivs {{
    { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
    { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 },     { "1/8D", 0.75 }, { "1/4T", 2.0 / 3.0 },
    { "1/4", 1.0 },    { "1/4D", 1.5 },        { "1/2", 2.0 },   { "1 Bar", 4.0 } }};

class FractalDelay
{
public:
    static constexpr double kMaxSeconds = 4.0;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const DelayParams& p, double bpm) noexcept;
    void setEnabled (bool on) noexcept { spill.setEnabled (on); }
    void snapEnabled() noexcept        { spill.snap(); }

    /** In place. `duck` (may be null) scales the wet signal per sample. */
    void process (float* left, float* right, int numSamples, const float* duck) noexcept;

private:
    double      sampleRate = 44100.0;
    DelayParams params;
    Spillover   spill;
    DelayLine   line[2];
    GrainPitchShifter shifter[2];
    SvfCoeffs   lpCoeffs, hpCoeffs;
    SvfState    lp[2], hp[2];
    float       delaySamples = 1000.0f, targetDelay = 1000.0f, delayCoef = 0.999f;
    Smoother    fbSm, mixSm, ppSm;
    float       fbState[2] {};
};
} // namespace dt
