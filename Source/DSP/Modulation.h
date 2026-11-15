#pragma once
// Modulation sources: two LFOs (incl. a drawable 16-point shape) and an input
// envelope follower. Sources are evaluated once per engine chunk (32 samples,
// < 1 ms), which is smooth because every destination is itself smoothed.

#include "Common.h"
#include "TransportClock.h"

namespace dt
{
enum class LfoShape { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, SmoothRandom, Custom };
inline constexpr std::array<const char*, 8> kLfoShapeNames {
    "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random", "Custom" };

inline constexpr std::array<SyncDiv, 13> kLfoDivs {{
    { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 },
    { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 Bar", 4.0 }, { "2 Bars", 8.0 },
    { "4 Bars", 16.0 }, { "8 Bars", 32.0 }, { "16 Bars", 64.0 } }};

inline constexpr int kCustomPoints = 16;

struct LfoParams
{
    LfoShape shape     = LfoShape::Sine;
    bool     sync      = true;
    int      syncIndex = 8;     // 1 Bar
    float    rateHz    = 1.0f;  // free-running rate when sync is off
    std::array<float, kCustomPoints> custom { 0.5f, 0.69f, 0.85f, 0.96f, 1.0f, 0.96f, 0.85f, 0.69f,
                                              0.5f, 0.31f, 0.15f, 0.04f, 0.0f, 0.04f, 0.15f, 0.31f }; // 0..1
};

class Lfo
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; glide.prepare (sr); reset(); }
    void reset() noexcept { freePhase = 0.0; freeCycle = 0; lastCycle = INT64_MIN; prevRand = nextRand = 0.0f; glide.reset(); }
    void setParams (const LfoParams& p) noexcept { params = p; }

    /** Bipolar value (-1..1) at the start of the chunk; advances the free-running phase by n samples. */
    float tick (const TransportClock& clk, int n) noexcept
    {
        double phase;
        int64_t cycle;
        if (params.sync)
        {
            const double q   = kLfoDivs[(size_t) std::clamp (params.syncIndex, 0, (int) kLfoDivs.size() - 1)].quarters;
            // Synced: eases across playhead jumps (a loop wrapping round) instead of
            // moving every destination at once.
            if (clk.jumped()) glide.arm();
            const double pos = glide.next (clk.ppqAt (0) / q, n);
            cycle = (int64_t) std::floor (pos);
            phase = pos - (double) cycle;
        }
        else
        {
            phase = freePhase;
            cycle = freeCycle;
            freePhase += clampf (params.rateHz, 0.01f, 40.0f) * n / sampleRate;
            while (freePhase >= 1.0) { freePhase -= 1.0; ++freeCycle; }
        }

        if (cycle != lastCycle) // new cycle: next random value
        {
            prevRand  = (lastCycle == INT64_MIN) ? rng.bipolar() : nextRand;
            nextRand  = rng.bipolar();
            lastCycle = cycle;
        }
        value = shape ((float) phase);
        return value;
    }

    float getValue() const noexcept { return value; }

    float shape (float p) const noexcept
    {
        switch (params.shape)
        {
            case LfoShape::Sine:         return std::sin (kTwoPi * p);
            case LfoShape::Triangle:     return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case LfoShape::SawUp:        return 2.0f * p - 1.0f;
            case LfoShape::SawDown:      return 1.0f - 2.0f * p;
            case LfoShape::Square:       return p < 0.5f ? 1.0f : -1.0f;
            case LfoShape::SampleHold:   return nextRand;
            case LfoShape::SmoothRandom: return prevRand + (nextRand - prevRand) * (0.5f - 0.5f * std::cos (kPi * p));
            case LfoShape::Custom:
            {
                const float x = p * kCustomPoints;
                const int   a = std::min (kCustomPoints - 1, (int) x);
                const float t = x - (float) a;
                const float v = lerpf (params.custom[(size_t) a], params.custom[(size_t) ((a + 1) % kCustomPoints)], t);
                return 2.0f * clampf (v, 0.0f, 1.0f) - 1.0f;
            }
        }
        return 0.0f;
    }

private:
    double    sampleRate = 44100.0;
    LfoParams params;
    double    freePhase = 0.0;
    int64_t   freeCycle = 0, lastCycle = INT64_MIN;
    CycleGlide glide;   // the synced position across playhead jumps
    float     prevRand = 0.0f, nextRand = 0.0f, value = 0.0f;
    Random    rng { 0x1F0u };
};

struct EnvParams
{
    float attackMs  = 10.0f;
    float releaseMs = 150.0f;
    float gainDb    = 0.0f; // sensitivity, -24..+24
};

/** Input level follower, output 0..1 (dB-mapped so it responds evenly). */
class EnvelopeFollower
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; setParams (params); reset(); }
    void reset() noexcept { env = 0.0f; value = 0.0f; }
    void setParams (const EnvParams& p) noexcept
    {
        params = p;
        att = onePoleCoef (clampf (p.attackMs, 0.1f, 500.0f), sampleRate);
        rel = onePoleCoef (clampf (p.releaseMs, 5.0f, 5000.0f), sampleRate);
    }

    float process (const float* l, const float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::fabs (l[i]), std::fabs (r[i]));
            env = lvl + (lvl > env ? att : rel) * (env - lvl);
        }
        const float db = 20.0f * std::log10 (env + 1.0e-6f) + clampf (params.gainDb, -24.0f, 24.0f);
        value = clampf ((db + 48.0f) / 48.0f, 0.0f, 1.0f); // -48 dBFS .. 0 dBFS -> 0..1
        return value;
    }

    float getValue() const noexcept { return value; }

private:
    double    sampleRate = 44100.0;
    EnvParams params;
    float     env = 0.0f, value = 0.0f, att = 0.0f, rel = 0.0f;
};

enum class ModSource { None = 0, Lfo1, Lfo2, Envelope };
inline constexpr std::array<const char*, 4> kModSourceNames { "None", "LFO 1", "LFO 2", "Envelope" };

struct ModSlot
{
    int   source = 0;   // ModSource
    int   dest   = 0;   // index into modDestinations()
    float amount = 0.0f; // -1..1
};

inline constexpr int kModSlots = 8;
} // namespace dt
