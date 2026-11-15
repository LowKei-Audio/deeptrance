#pragma once
// Abyss Reverb: 8-line feedback delay network (Hadamard mixing) with modulated
// lines, per-line damping, input diffusion, pre-delay and an octave-up shimmer
// loop. Freeze holds the current tail indefinitely (lossless loop, input muted).

#include "Common.h"
#include "DelayLine.h"
#include "Filters.h"
#include "PitchShifter.h"

namespace dt
{
struct AbyssParams
{
    float decaySec   = 6.0f;  // RT60, 0.5..60 s
    float size       = 0.6f;  // 0..1
    float damping    = 0.45f; // 0..1 (bright .. dark)
    float preDelayMs = 20.0f; // 0..250
    float mod        = 0.4f;  // 0..1 chorus on the tail
    float shimmer    = 0.0f;  // 0..1 octave-up feedback
    float width      = 1.0f;  // 0..1
    float mix        = 0.25f;
    bool  freeze     = false;
};

class AbyssReverb
{
public:
    static constexpr int kLines = 8;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const AbyssParams& p) noexcept;
    void setEnabled (bool on) noexcept { spill.setEnabled (on); }
    void snapEnabled() noexcept        { spill.snap(); }

    void process (float* left, float* right, int numSamples, const float* duck) noexcept;

private:
    void clearState() noexcept;

    struct Diffuser // Schroeder allpass
    {
        DelayLine line; float delay = 100.0f;
        float process (float x) noexcept
        {
            constexpr float g = 0.6f;
            const float d = line.read (delay);
            const float v = x + g * d;
            line.push (v);
            return d - g * v;
        }
    };

    double      sampleRate = 44100.0;
    AbyssParams params;
    Spillover   spill;

    DelayLine   preDelay[2];
    Diffuser    diffusers[2][4];
    SvfCoeffs   inHpCoeffs;
    SvfState    inHp[2];

    DelayLine   lines[kLines];
    float       baseLen[kLines] {};
    float       damp[kLines] {};
    float       lfoC[kLines] {}, lfoS[kLines] {}, lfoRotC[kLines] {}, lfoRotS[kLines] {};
    GrainPitchShifter shimmerShift[2];
    SvfCoeffs   shimHpCoeffs;
    SvfState    shimHp[2];
    float       lastOut[2] {};

    Smoother    sizeSm, mixSm, freezeSm, shimmerSm, preDelaySm, widthSm, modSm;
    float       decaySec = 6.0f, dampCoef = 0.3f;
    // The loop's gains move per sample, from last block's values to this block's: a gain
    // that stepped (Decay or Damping changed, a preset loaded) put a step into the tank,
    // which came back out of every line as a click tens of ms later. Decay also glides.
    float       decayCur = -1.0f, gCur[kLines] {}, aCur = 0.0f, levelCur = 1.0f, shimCur = 0.0f;
    bool        loopPrimed = false;
};
} // namespace dt
