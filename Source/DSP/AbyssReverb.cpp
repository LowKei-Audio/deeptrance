#include "AbyssReverb.h"

namespace dt
{
namespace
{
    // Mutually prime-ish line lengths (ms) for a dense, colourless tail.
    constexpr float kLineMs[AbyssReverb::kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 79.3f };
    constexpr float kLfoHz[AbyssReverb::kLines]  = { 0.13f, 0.17f, 0.23f, 0.29f, 0.31f, 0.37f, 0.41f, 0.47f };
    constexpr float kDiffMs[2][4] = { { 4.77f, 3.59f, 12.73f, 9.31f }, { 4.95f, 3.71f, 13.11f, 8.93f } };
    constexpr float kMaxModMs   = 1.2f;
    constexpr float kMinSize    = 0.35f, kSizeSpan = 1.25f; // size 0..1 -> 0.35x .. 1.6x line lengths
    constexpr float kInputScale = 0.3f;
    constexpr float kOutScale   = 0.5f;
    // Shimmer loop scale. Measured sweep (sine + noise, size 0..1, decay 2..60 s): 1.5 lifts the
    // octave band from -19 dB to -2..-12 dB at 100% shimmer with peaks <= 1.25 (never reaching the
    // loop limiter); 3.0 already saturates large rooms. Re-measure if the network changes.
    constexpr float kShimmerLoop = 1.5f;

    inline float sizeFactor (float size) noexcept { return kMinSize + kSizeSpan * size; }

    /** Loop safety: linear below 1, saturates smoothly towards 2. Never active in normal use. */
    inline float loopSafe (float x) noexcept
    {
        const float a = std::fabs (x);
        return a < 1.0f ? x : std::copysign (1.0f + std::tanh (a - 1.0f), x);
    }

    /** In-place 8-point fast Walsh-Hadamard transform, orthonormal (lossless mixing). */
    inline void hadamard8 (float* v) noexcept
    {
        for (int h = 1; h < 8; h *= 2)
            for (int i = 0; i < 8; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b; v[j + h] = a - b;
                }
        for (int i = 0; i < 8; ++i) v[i] *= 0.35355339f;
    }
}

void AbyssReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    const float msToS = (float) (0.001 * sampleRate);

    for (auto& p : preDelay) p.prepare ((int) (0.3 * sampleRate) + 16);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k)
        {
            diffusers[c][k].delay = kDiffMs[c][k] * msToS;
            diffusers[c][k].line.prepare ((int) diffusers[c][k].delay + 16);
        }

    const float maxLen = kLineMs[kLines - 1] * msToS * (kMinSize + kSizeSpan) + kMaxModMs * msToS + 16.0f;
    for (int i = 0; i < kLines; ++i)
    {
        baseLen[i] = kLineMs[i] * msToS;
        lines[i].prepare ((int) maxLen);
        const double w = kTwoPiD * kLfoHz[i] / sampleRate;
        lfoRotC[i] = (float) std::cos (w);
        lfoRotS[i] = (float) std::sin (w);
    }
    for (auto& s : shimmerShift) { s.prepare (sampleRate); s.setSemitones (12.0f); }
    inHpCoeffs.set (90.0f, 0.707f, sampleRate);
    shimHpCoeffs.set (200.0f, 0.707f, sampleRate); // octave-up bass only muddies the tail

    sizeSm.prepare (sampleRate, 0.5);   // slow: size changes re-pitch the tail
    preDelaySm.prepare (sampleRate, 0.2);
    for (auto* s : { &mixSm, &freezeSm, &shimmerSm, &widthSm, &modSm }) s->prepare (sampleRate, 0.05);
    sizeSm.setImmediate (params.size);
    preDelaySm.setImmediate (params.preDelayMs * msToS);
    mixSm.setImmediate (params.mix);
    freezeSm.setImmediate (params.freeze ? 1.0f : 0.0f);
    shimmerSm.setImmediate (params.shimmer);
    widthSm.setImmediate (params.width);
    modSm.setImmediate (params.mod);

    spill.prepare (sampleRate);
    spill.setIdleSamples ((int) (0.3 * sampleRate)); // longer than pre-delay + longest line
    setParams (params);
    reset();
}

void AbyssReverb::clearState() noexcept
{
    loopPrimed = false;
    decayCur = -1.0f;
    for (auto& p : preDelay) p.reset();
    for (auto& ch : diffusers) for (auto& d : ch) d.line.reset();
    for (auto& s : inHp) s.reset();
    for (auto& l : lines) l.reset();
    for (auto& s : shimmerShift) s.reset();
    for (auto& s : shimHp) s.reset();
    for (int i = 0; i < kLines; ++i)
    {
        damp[i] = 0.0f;
        lfoC[i] = std::cos (0.7f * (float) i);
        lfoS[i] = std::sin (0.7f * (float) i);
    }
    lastOut[0] = lastOut[1] = 0.0f;
}

void AbyssReverb::reset() noexcept
{
    clearState();
    spill.snap();
}

void AbyssReverb::setParams (const AbyssParams& p) noexcept
{
    params = p;
    decaySec = clampf (p.decaySec, 0.5f, 60.0f);
    // Damping 0% = no high-frequency loss at all; above that, cutoff falls 18 kHz -> 1.5 kHz.
    const float damping = clampf (p.damping, 0.0f, 1.0f);
    const float fc = 18000.0f * std::pow (1500.0f / 18000.0f, damping);
    dampCoef = (float) std::exp (-kTwoPiD * fc / sampleRate) * std::min (1.0f, damping * 10.0f);

    sizeSm.setTarget (clampf (p.size, 0.0f, 1.0f));
    preDelaySm.setTarget (clampf (p.preDelayMs, 0.0f, 250.0f) * 0.001f * (float) sampleRate);
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
    freezeSm.setTarget (p.freeze ? 1.0f : 0.0f);
    shimmerSm.setTarget (clampf (p.shimmer, 0.0f, 1.0f));
    widthSm.setTarget (clampf (p.width, 0.0f, 1.0f));
    modSm.setTarget (clampf (p.mod, 0.0f, 1.0f));
}

void AbyssReverb::process (float* left, float* right, int numSamples, const float* duck) noexcept
{
    // A disabled reverb must not stay frozen, or its tail would never end.
    if (! spill.isEnabled()) freezeSm.setTarget (0.0f);

    bool wake = false;
    if (spill.skipBlock (wake)) return;
    if (wake) clearState();

    // Per-block decay gains from the current size and a gliding decay; every gain then
    // ramps to its target across the block.
    if (decayCur < 0.0f) decayCur = decaySec;
    decayCur += (decaySec - decayCur) * (1.0f - std::exp (-(float) numSamples / (0.1f * (float) sampleRate)));
    float g[kLines], gStep[kLines], g2 = 0.0f;
    const float factor = sizeFactor (sizeSm.next());
    const float perSample = 1.0f / (float) std::max (1, numSamples);
    for (int i = 0; i < kLines; ++i)
    {
        g[i] = std::pow (10.0f, -3.0f * baseLen[i] * factor / (decayCur * (float) sampleRate));
        g2 += g[i] * g[i] / (float) kLines;
    }

    // Headroom left in the loop: sqrt(1 - g^2). Long decays store more energy, so:
    //  - the shimmer feedback is scaled by it, keeping total loop gain below 1 at any
    //    decay (and the octave-up share sounding the same for short and long tails);
    //  - the input is scaled by its square root, halving (in dB) the natural loudness
    //    growth of long tails on sustained input. Reference: a 2 s tail at mid size.
    const float headroom = std::sqrt (std::max (1.0e-6f, 1.0f - g2));
    constexpr float kRefHeadroom = 0.2f;
    const float levelComp  = std::sqrt (headroom / kRefHeadroom);
    const float shimLoop   = kShimmerLoop * headroom / (4.0f * kOutScale);
    if (! loopPrimed)
    {
        for (int i = 0; i < kLines; ++i) gCur[i] = g[i];
        aCur = dampCoef; levelCur = levelComp; shimCur = shimLoop;
        loopPrimed = true;
    }
    for (int i = 0; i < kLines; ++i) gStep[i] = (g[i] - gCur[i]) * perSample;
    const float aStep = (dampCoef - aCur) * perSample, levelStep = (levelComp - levelCur) * perSample,
                shimStep = (shimLoop - shimCur) * perSample;

    const float maxMod = kMaxModMs * 0.001f * (float) sampleRate;
    float v[kLines], o[kLines];

    for (int n = 0; n < numSamples; ++n)
    {
        const float on     = spill.nextGain();
        aCur += aStep; levelCur += levelStep; shimCur += shimStep;
        for (int i = 0; i < kLines; ++i) gCur[i] += gStep[i];
        const float size   = sizeFactor (sizeSm.next());
        const float freeze = freezeSm.next();
        const float shim   = shimmerSm.next() * (1.0f - freeze) * shimCur;
        const float pd     = preDelaySm.next();
        const float width  = widthSm.next();
        const float depth  = modSm.next() * maxMod;
        float dryG, wetG;
        mixGains (mixSm.next(), dryG, wetG);
        dryG = 1.0f + (dryG - 1.0f) * on;

        // Input: pre-delay -> low cut -> diffusion. Freeze mutes new input.
        const float inGain = on * (1.0f - freeze) * kInputScale * levelCur;
        float x[2] = { left[n] * inGain, right[n] * inGain };
        for (int c = 0; c < 2; ++c)
        {
            preDelay[c].push (x[c]);
            float s = preDelay[c].read (std::max (DelayLine::kMinDelay, pd));
            s = inHp[c].process (s, inHpCoeffs).hp;
            for (auto& d : diffusers[c]) s = d.process (s);
            x[c] = s;
        }

        // Shimmer: octave-up copy of the last output fed back into the network.
        const float shL = shim > 0.0f ? shimHp[0].process (shimmerShift[0].process (lastOut[0]), shimHpCoeffs).hp * shim : 0.0f;
        const float shR = shim > 0.0f ? shimHp[1].process (shimmerShift[1].process (lastOut[1]), shimHpCoeffs).hp * shim : 0.0f;

        const float a = aCur * (1.0f - freeze);
        for (int i = 0; i < kLines; ++i)
        {
            // Quadrature LFO (rotation) for cheap per-line modulation.
            const float c = lfoC[i] * lfoRotC[i] - lfoS[i] * lfoRotS[i];
            lfoS[i]       = lfoS[i] * lfoRotC[i] + lfoC[i] * lfoRotS[i];
            lfoC[i]       = c;

            o[i]    = lines[i].read (baseLen[i] * size + depth * (1.0f + lfoS[i]));
            damp[i] = o[i] + a * (damp[i] - o[i]);
            v[i]    = damp[i] * (gCur[i] + (1.0f - gCur[i]) * freeze);
        }

        hadamard8 (v);
        for (int i = 0; i < kLines; ++i)
            lines[i].push (loopSafe (v[i] + ((i & 1) ? x[1] + shR : x[0] + shL)));

        const float outL = (o[0] - o[2] + o[4] - o[6]) * kOutScale;
        const float outR = (o[1] - o[3] + o[5] - o[7]) * kOutScale;
        lastOut[0] = outL; lastOut[1] = outR;

        const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * width;
        const float d   = (duck ? duck[n] : 1.0f) * wetG;
        left[n]  = left[n]  * dryG + (mid + side) * d;
        right[n] = right[n] * dryG + (mid - side) * d;
        spill.trackWet (std::fabs (outL) + std::fabs (outR) + std::fabs (x[0]) + std::fabs (x[1]));
    }

    // Keep the rotating LFOs on the unit circle.
    for (int i = 0; i < kLines; ++i)
    {
        const float m = 1.0f / std::sqrt (lfoC[i] * lfoC[i] + lfoS[i] * lfoS[i]);
        lfoC[i] *= m; lfoS[i] *= m;
    }
    spill.endBlock();
}
} // namespace dt
