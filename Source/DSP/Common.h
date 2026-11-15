#pragma once
// Shared DSP utilities. Header-only, allocation-free, no JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
  #include <xmmintrin.h>
  #define DT_HAS_SSE 1
#endif

namespace dt
{
constexpr float  kPi     = 3.14159265358979323846f;
constexpr float  kTwoPi  = 2.0f * kPi;
constexpr double kTwoPiD = 6.28318530717958647692;

inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
inline float lerpf  (float a, float b, float t)   noexcept { return a + (b - a) * t; }
inline float dbToGain (float db) noexcept                  { return std::pow (10.0f, db * 0.05f); }

/** Wraps a phase into [0, 1). */
inline double wrap01 (double x) noexcept { return x - std::floor (x); }

/** Eases a cyclic position (in cycles) across a playhead jump: arm() at the jump,
    then the position continues from where it was and glides onto the new one, the
    shortest way round, over ~40 ms. A snap moves a sweep, a vowel or an LFO target
    in one sample, which is heard as a click. */
struct CycleGlide
{
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { offset = last = progress = 0.0; armed = gliding = primed = false; }
    void arm() noexcept { armed = primed; }
    /** cycles: the target position now; samples: how far this call advances. */
    double next (double cycles, int samples = 1) noexcept
    {
        if (armed)
        {
            // Start from where we are, so the sweep neither steps nor changes speed.
            double d = last - cycles;
            d -= std::round (d);   // the shorter way round the cycle
            offset = d;
            progress = 0.0;
            gliding = std::abs (d) > 1.0e-9;
            armed = false;
        }
        double out = cycles;
        if (gliding)
        {
            // Smootherstep ease over kGlideSec: zero extra speed at both ends, no kink.
            const double x = std::min (1.0, progress);
            out += offset * (1.0 - x * x * x * (x * (6.0 * x - 15.0) + 10.0));
            progress += (double) samples / (kGlideSec * sr);
            if (x >= 1.0) { offset = 0.0; gliding = false; }
        }
        last = out;
        primed = true;
        return out;
    }
    static constexpr double kGlideSec = 0.04;
    double sr = 44100.0, offset = 0.0, last = 0.0, progress = 0.0;
    bool armed = false, gliding = false, primed = false;
};

/** Coefficient for a one-pole smoother reaching ~63% in `ms` milliseconds. */
inline float onePoleCoef (float ms, double sampleRate) noexcept
{
    const double samples = std::max (1.0, (double) ms * 0.001 * sampleRate);
    return (float) std::exp (-1.0 / samples);
}

/** Sets flush-to-zero / denormals-are-zero for the current scope.
    The plugin also uses juce::ScopedNoDenormals; this keeps the DSP safe on its own (tests, other hosts). */
struct ScopedFlushDenormals
{
#if DT_HAS_SSE
    ScopedFlushDenormals() noexcept : prev (_mm_getcsr()) { _mm_setcsr (prev | 0x8040); }
    ~ScopedFlushDenormals() noexcept                      { _mm_setcsr (prev); }
    unsigned int prev;
#endif
};

/** Linear ramp smoother for parameters (avoids zipper noise). */
class Smoother
{
public:
    void prepare (double sampleRate, double rampSeconds) noexcept
    {
        rampLen = std::max (1, (int) (sampleRate * rampSeconds));
        countdown = 0;
        current = target;
    }

    void setImmediate (float v) noexcept { current = target = v; countdown = 0; }

    void setTarget (float v) noexcept
    {
        if (std::abs (v - target) < 1.0e-7f) return; // unchanged: don't restart the ramp
        target = v;
        countdown = rampLen;
        step = (target - current) / (float) rampLen;
    }

    float next() noexcept
    {
        if (countdown <= 0) return target;
        current += step;
        if (--countdown == 0) current = target;
        return current;
    }

    float getTarget() const noexcept    { return target; }
    bool  isSmoothing() const noexcept  { return countdown > 0; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int   rampLen = 1, countdown = 0;
};

/** Tempo-synced divisions shared by every LFO-style control.
    Value = length of one cycle in quarter notes. */
struct SyncDiv { const char* name; double quarters; };

inline constexpr std::array<SyncDiv, 8> kSyncDivs {{
    { "1/16",   0.25 }, { "1/8",    0.5 }, { "1/4",    1.0 }, { "1/2",    2.0 },
    { "1 Bar",  4.0 },  { "2 Bars", 8.0 }, { "4 Bars", 16.0 }, { "8 Bars", 32.0 } }};

inline double syncQuarters (int index) noexcept
{
    return kSyncDivs[(size_t) std::clamp (index, 0, (int) kSyncDivs.size() - 1)].quarters;
}

/** Tiny RT-safe pseudo random generator (xorshift32). */
class Random
{
public:
    explicit Random (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}
    /** Uniform in [-1, 1]. */
    float bipolar() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) ((double) state / 4294967295.0) * 2.0f - 1.0f;
    }
private:
    uint32_t state;
};
} // namespace dt
