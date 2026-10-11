#pragma once
// Zero-latency grain pitch shifter (two crossfaded read heads sweeping a short
// delay). Used inside feedback loops (Fractal Delay, Abyss shimmer), where its
// small average delay is compensated by the caller.
//
// Near 0 semitones the shifter blends to a plain tap at the same average delay,
// so "no shift" never comb-filters and the knob can sweep through zero smoothly.

#include "Common.h"
#include "DelayLine.h"

namespace dt
{
class GrainPitchShifter
{
public:
    static constexpr float kWindowMs = 60.0f;

    void prepare (double sampleRate)
    {
        window = (float) (kWindowMs * 0.001 * sampleRate);
        line.prepare ((int) window + 16);
        ratioCoef = onePoleCoef (30.0f, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        phase = 0.0f;
        ratio = targetRatio;
        shiftAmt = targetShift;
    }

    void setSemitones (float st) noexcept
    {
        targetRatio = std::exp2 (st / 12.0f);
        targetShift = clampf (std::fabs (st) / 0.5f, 0.0f, 1.0f); // full shift beyond half a semitone
    }

    /** Average delay (samples) the shifter adds; callers subtract it from their own delay. */
    float averageDelay() const noexcept { return DelayLine::kMinDelay + 0.5f * window; }

    float process (float x) noexcept
    {
        line.push (x);
        ratio    = targetRatio + ratioCoef * (ratio - targetRatio);
        shiftAmt = targetShift + ratioCoef * (shiftAmt - targetShift);

        // Tap delay changes at (1 - ratio) samples per sample -> pitch ratio `ratio`.
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        // sin^2 windows of two heads half a window apart sum to exactly 1.
        const float s1 = std::sin (kPi * phase), s2 = std::sin (kPi * p2);
        const float shifted = s1 * s1 * line.read (DelayLine::kMinDelay + phase * window)
                            + s2 * s2 * line.read (DelayLine::kMinDelay + p2 * window);
        const float plain = line.read (averageDelay());
        return plain + (shifted - plain) * shiftAmt;
    }

private:
    DelayLine line;
    float window = 2880.0f, phase = 0.0f;
    float ratio = 1.0f, targetRatio = 1.0f, ratioCoef = 0.99f;
    float shiftAmt = 0.0f, targetShift = 0.0f;
};

/** Click-free enable for effects with tails (delay, reverb, granular).
    Disabling stops feeding the effect and restores the dry signal, but the wet
    tail keeps ringing out naturally ("spillover"). Once the tail is silent the
    module reports idle and the engine skips it (zero CPU, bit-exact dry). */
class Spillover
{
public:
    void prepare (double sampleRate) noexcept
    {
        step = 1.0f / (float) std::max (1.0, sampleRate * 0.015);
        idleAfter = idleAfter0 = (int) (sampleRate * 0.05); // 50 ms of silence
    }

    void setEnabled (bool shouldBeOn) noexcept { on = shouldBeOn; }
    bool isEnabled() const noexcept            { return on; }

    /** Silence needed before going idle. Must exceed the longest delay inside the
        effect, or an echo still travelling through a delay line would be cut. */
    void setIdleSamples (int n) noexcept { idleAfter = std::max (idleAfter0, n); }

    /** Jump to the target state (prepare/reset). */
    void snap() noexcept
    {
        gain = on ? 1.0f : 0.0f;
        silentCount = idleAfter;
        idle = ! on;
    }

    /** Call at block start. Returns true if the block can be skipped entirely.
        `wakeUp` is set when the module must clear its state before running. */
    bool skipBlock (bool& wakeUp) noexcept
    {
        wakeUp = false;
        if (! idle) return false;
        if (! on) return true;
        idle = false;
        silentCount = 0;
        wakeUp = true;
        return false;
    }

    float nextGain() noexcept
    {
        gain = on ? std::min (1.0f, gain + step) : std::max (0.0f, gain - step);
        return gain;
    }

    /** Feed the absolute wet level of each output sample. */
    void trackWet (float wetAbs) noexcept
    {
        silentCount = wetAbs > 1.0e-5f ? 0 : silentCount + 1;
    }

    /** Call at block end. */
    void endBlock() noexcept
    {
        if (! on && gain <= 0.0f && silentCount >= idleAfter) idle = true;
    }

private:
    float gain = 0.0f, step = 0.001f;
    int   silentCount = 0, idleAfter = 2400, idleAfter0 = 2400;
    bool  on = false, idle = true;
};

/** Dry/wet gains for the series "mix" control: 0% = dry, 50% = both at full,
    100% = wet only. Keeps loudness steady across the knob. */
inline void mixGains (float mix, float& dry, float& wet) noexcept
{
    dry = std::min (1.0f, 2.0f * (1.0f - mix));
    wet = std::min (1.0f, 2.0f * mix);
}
} // namespace dt
