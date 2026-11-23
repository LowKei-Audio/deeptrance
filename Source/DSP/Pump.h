#pragma once
// Pump: tempo-synced sidechain-style ducking, the trance "breathing" without routing a
// kick. Each cycle (1/8 .. 1 bar) the level dips at the downbeat and swells back.
// Shape sets how long the dip holds before the swell; Band pumps the full signal, only
// the lows (the classic bass-and-pad duck) or only the highs. Runs after the global mix,
// so it breathes the whole output, tails included.

#include "Common.h"
#include "Filters.h"
#include "TransportClock.h"

namespace dt
{
enum class PumpBand { Full = 0, Lows, Highs };
inline constexpr std::array<const char*, 3> kPumpBandNames { "Full", "Lows", "Highs" };

inline constexpr std::array<SyncDiv, 4> kPumpDivs {{
    { "1/8", 0.5 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 Bar", 4.0 } }};

struct PumpParams
{
    int   rateIndex = 1;     // index into kPumpDivs (1/4: one duck per beat)
    float depth     = 0.6f;  // 0..1, 1 = silence at the bottom of the dip
    float shape     = 0.5f;  // 0..1, 0 = quick swell back, 1 = long hold then a late swell
    int   band      = 0;     // PumpBand
};

class Pump
{
public:
    static constexpr float kBandHz   = 180.0f;  // Lows / Highs split
    static constexpr double kAttackSec = 0.004; // the dip itself: fast, but never a step

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void snap() noexcept { depthSm.setImmediate (depthSm.getTarget()); }
    void setParams (const PumpParams& p) noexcept;

    /** Where the pump is (in cycles) for this chunk. Call every chunk, on or off: the
        position also drives the "Pump" modulation source. Eases across playhead jumps. */
    void advance (const TransportClock& clk, int n) noexcept;

    /** 0..1 duck amount at the start of this chunk (1 = bottom of the dip), depth-free. */
    float getDuck() const noexcept { return duckAt (wrap01 (pos0)); }

    /** In place, using the position from advance(). */
    void process (float* left, float* right, int n) noexcept;

    /** Duck curve over one cycle, p in [0, 1). Public for tests and the UI. */
    float duckAt (double p) const noexcept
    {
        const double a = attackFrac;
        if (p < a)
        {
            const double x = p / a;              // smoothstep into the dip
            return (float) (x * x * (3.0 - 2.0 * x));
        }
        const double r = (p - a) / (1.0 - a);    // swell back out of it
        return (float) (1.0 - std::pow (r, exponent));
    }

private:
    double     sampleRate = 44100.0;
    PumpParams params;
    CycleGlide glide;
    double     pos0 = 0.0, posStep = 0.0, attackFrac = 0.02, exponent = 1.0;
    Smoother   depthSm;
    LR4Crossover band[2];   // per channel; low + high sums flat (an allpass)
};
} // namespace dt
