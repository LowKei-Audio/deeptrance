#pragma once
// Click-free on/off switching for a module. While fading, the engine blends
// the module's output with its input; once fully off, the module is skipped
// entirely (zero CPU) and its state is cleared before it next turns on.

#include <algorithm>

namespace dt
{
class ModuleFader
{
public:
    void prepare (double sampleRate, double fadeSeconds = 0.015) noexcept
    {
        step = 1.0f / (float) std::max (1.0, sampleRate * fadeSeconds);
    }

    void setEnabled (bool shouldBeOn) noexcept { targetOn = shouldBeOn; }

    /** Jump straight to the target (used on prepare / reset). */
    void snap() noexcept { gain = targetOn ? 1.0f : 0.0f; }

    /** True if the module needs to run this block. */
    bool isActive() const noexcept { return targetOn || gain > 0.0f; }

    /** True when fully on: no dry copy or blend is required. */
    bool isFullyOn() const noexcept { return targetOn && gain >= 1.0f; }

    /** True when the module is about to come back from fully off (state should be reset). */
    bool isWakingUp() const noexcept { return targetOn && gain <= 0.0f; }

    float next() noexcept
    {
        gain = targetOn ? std::min (1.0f, gain + step) : std::max (0.0f, gain - step);
        return gain;
    }

private:
    float gain = 0.0f, step = 0.001f;
    bool  targetOn = false;
};
} // namespace dt
