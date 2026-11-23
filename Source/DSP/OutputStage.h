#pragma once
// Output stage, last in the chain: stereo Width, Bass Mono (lows below a set frequency
// summed to mono, the club-safe trance low end), the Drop pad's low cut, and the safety
// clipper. Each part is skipped entirely while neutral, so a sound that uses none of
// them comes out bit-for-bit as it did in 1.0.3.

#include "Common.h"
#include "Filters.h"

namespace dt
{
struct OutputParams
{
    float width      = 1.0f;   // 0..2 (0 = mono, 1 = as is, 2 = twice the side signal)
    float bassMonoHz = 0.0f;   // 0 = off, else 40..400 Hz
    bool  clip       = false;  // safety clipper (the plugin turns it on for new sounds)
};

class OutputStage
{
public:
    static constexpr float kClipKnee = 0.708f;  // -3 dBFS: untouched below
    static constexpr float kDropHz   = 320.0f;  // the Drop pad's low cut

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        for (auto* s : { &widthSm, &monoSm, &dropSm, &clipSm }) s->prepare (sr, 0.03);
        widthSm.setImmediate (clampf (params.width, 0.0f, 2.0f));
        monoSm.setImmediate (params.bassMonoHz > 0.0f ? 1.0f : 0.0f);
        clipSm.setImmediate (params.clip ? 1.0f : 0.0f);
        dropSm.setImmediate (0.0f);
        dropCoeffs.set (kDropHz, 0.70710678f, sr);
        setParams (params, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& st : monoHp) st.reset();
        for (auto& ch : dropHp) for (auto& s : ch) s.reset();
    }

    /** drop: the Drop pad, 0..1. */
    void setParams (const OutputParams& p, float drop) noexcept
    {
        params = p;
        widthSm.setTarget (clampf (p.width, 0.0f, 2.0f));
        monoSm.setTarget (p.bassMonoHz > 0.0f ? 1.0f : 0.0f);
        if (p.bassMonoHz > 0.0f) monoCoeffs.set (clampf (p.bassMonoHz, 40.0f, 400.0f), 0.70710678f, sampleRate);
        dropSm.setTarget (clampf (drop, 0.0f, 1.0f));
        clipSm.setTarget (p.clip ? 1.0f : 0.0f);
    }

    /** Snaps every blend to its target (prepare, a sound swap). */
    void snap() noexcept
    {
        widthSm.setImmediate (widthSm.getTarget());
        monoSm.setImmediate (monoSm.getTarget());
        dropSm.setImmediate (dropSm.getTarget());
        clipSm.setImmediate (clipSm.getTarget());
    }

    void process (float* left, float* right, int n) noexcept
    {
        const bool width = widthSm.isSmoothing() || widthSm.getTarget() != 1.0f;
        const bool mono  = monoSm.isSmoothing()  || monoSm.getTarget()  > 0.0f;
        const bool drop  = dropSm.isSmoothing()  || dropSm.getTarget()  > 0.0f;
        const bool clip  = clipSm.isSmoothing()  || clipSm.getTarget()  > 0.0f;

        if (width || mono)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (left[i] + right[i]);
                float s = 0.5f * (left[i] - right[i]) * widthSm.next();
                // Bass Mono: the side signal through a 24 dB/oct high-pass, blended in (a
                // 12 dB split left -6 dB of a 50 Hz side signal behind).
                const float a = monoSm.next();
                if (mono) s += a * (monoHp[1].process (monoHp[0].process (s, monoCoeffs).hp, monoCoeffs).hp - s);
                left[i]  = m + s;
                right[i] = m - s;
            }
        if (! mono) for (auto& st : monoHp) st.reset();

        if (drop)
            for (int i = 0; i < n; ++i)
            {
                const float a = dropSm.next();
                float* io[2] = { &left[i], &right[i] };
                for (int ch = 0; ch < 2; ++ch)
                {
                    // 24 dB/oct high-pass: the kick and bass vanish, the top end stays.
                    const float hp = dropHp[ch][1].process (dropHp[ch][0].process (*io[ch], dropCoeffs).hp, dropCoeffs).hp;
                    *io[ch] += a * (hp - *io[ch]);
                }
            }
        else
            for (auto& ch : dropHp) for (auto& s : ch) s.reset();

        if (clip)
            for (int i = 0; i < n; ++i)
            {
                const float a = clipSm.next();
                left[i]  += a * (softClip (left[i])  - left[i]);
                right[i] += a * (softClip (right[i]) - right[i]);
            }
    }

    /** Transparent below the knee, then a tanh curve that never passes 0 dBFS. Its slope
        is continuous at the knee, so it bends instead of breaking. */
    static float softClip (float x) noexcept
    {
        const float a = std::fabs (x);
        if (a <= kClipKnee) return x;
        constexpr float room = 1.0f - kClipKnee;
        return std::copysign (kClipKnee + room * std::tanh ((a - kClipKnee) / room), x);
    }

private:
    double       sampleRate = 44100.0;
    OutputParams params;
    Smoother     widthSm, monoSm, dropSm, clipSm;
    SvfCoeffs    monoCoeffs, dropCoeffs;
    SvfState     monoHp[2], dropHp[2][2];
};
} // namespace dt
