#pragma once
// Fractional delay line with 4-point Hermite interpolation.
// prepare() allocates; push()/read() are allocation-free.

#include "Common.h"
#include <vector>

namespace dt
{
class DelayLine
{
public:
    static constexpr float kMinDelay = 3.0f; // samples needed for the 4-point interpolator

    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask  = size - 1;
        write = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); write = 0; }

    void push (float x) noexcept
    {
        buffer[(size_t) write] = x;
        write = (write + 1) & mask;
    }

    /** Reads `delay` samples behind the most recent push (clamped to the valid range). */
    float read (float delay) const noexcept
    {
        delay = clampf (delay, kMinDelay, (float) (mask - 4));
        const float pos = (float) write - delay;
        const int   i   = (int) std::floor (pos);
        const float f   = pos - (float) i;

        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) ( i      & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, write = 0;
};
} // namespace dt
