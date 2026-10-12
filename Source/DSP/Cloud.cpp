#include "Cloud.h"

namespace dt
{
void Cloud::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& b : buffer) b.prepare ((int) (kBufferSecs * sampleRate) + 16);
    mixSm.prepare (sampleRate, 0.03);
    normSm.prepare (sampleRate, 0.1);
    mixSm.setImmediate (params.mix);
    spill.prepare (sampleRate);
    spill.setIdleSamples ((int) (0.05 * sampleRate));
    setParams (params);
    normSm.setImmediate (normSm.getTarget());
    reset();
}

void Cloud::clearState() noexcept
{
    for (auto& b : buffer) b.reset();
    for (auto& g : grains) g.active = false;
    countdown = 0.0f;
}

void Cloud::reset() noexcept
{
    clearState();
    spill.snap();
}

void Cloud::setParams (const CloudParams& p) noexcept
{
    params = p;
    params.sizeMs     = clampf (p.sizeMs, 5.0f, 500.0f);
    params.density    = clampf (p.density, 1.0f, 200.0f);
    params.positionMs = clampf (p.positionMs, 0.0f, 2000.0f);
    params.spray      = clampf (p.spray, 0.0f, 1.0f);
    params.pitch      = clampf (p.pitch, -24.0f, 24.0f);
    params.jitter     = clampf (p.jitter, 0.0f, 1.0f);
    params.shimmer    = clampf (p.shimmer, 0.0f, 1.0f);
    params.stereo     = clampf (p.stereo, 0.0f, 1.0f);
    frozen            = p.freeze;
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));

    // Overlapping grains add up, so scale by the expected overlap. How they add depends on
    // whether grains read the same audio: with no spray/pitch change every grain reads the
    // same delayed signal (coherent, sums like amplitudes); spray, pitch, jitter and shimmer
    // decorrelate them (sums like powers). Constants are the bell window's mean (0.533) and
    // mean-square (0.406). Measured within a few dB from 5 to 200 grains/s.
    const float overlap = params.density * params.sizeMs * 0.001f;
    const float decorrelation = clampf (params.spray / 0.15f + std::fabs (params.pitch) / 0.5f
                                        + params.jitter * params.jitter * params.jitter * kMaxJitterSt / 0.5f
                                        + params.shimmer * 2.0f, 0.0f, 1.0f);
    const float coherentNorm = 1.0f / std::max (0.533f, overlap * 0.533f);
    const float randomNorm   = 1.0f / std::sqrt (std::max (0.406f, overlap * 0.406f));
    normSm.setTarget (std::exp (lerpf (std::log (coherentNorm), std::log (randomNorm), decorrelation)));
}

void Cloud::spawn() noexcept
{
    Grain* g = nullptr;
    for (auto& cand : grains) if (! cand.active) { g = &cand; break; }
    if (g == nullptr) return; // all voices busy: skip this grain

    // Pitch: base + cubic jitter (fine detune at low settings) + optional octave shimmer.
    const float j = params.jitter;
    float st = params.pitch + j * j * j * kMaxJitterSt * rng.bipolar();
    if (0.5f + 0.5f * rng.bipolar() < params.shimmer)
        st += (0.5f + 0.5f * rng.bipolar() < 0.3f * params.shimmer) ? 24.0f : 12.0f;

    const float ms2s   = (float) (0.001 * sampleRate);
    const float len    = params.sizeMs * ms2s;
    g->ratio = std::exp2 (st / 12.0f);
    g->inc   = 1.0f / len;
    g->pos   = 0.0f;

    // Start point: position + spray, kept far enough back that a faster-than-realtime
    // grain never overtakes the write head (or, when frozen, the end of the buffer).
    float dist = params.positionMs * ms2s + params.spray * 500.0f * ms2s * (0.5f + 0.5f * rng.bipolar());
    const float travel = frozen ? len * g->ratio : len * std::max (0.0f, g->ratio - 1.0f);
    const float maxDist = (float) (kBufferSecs * sampleRate) - 16.0f;
    g->dist = clampf (std::max (dist, travel + DelayLine::kMinDelay + 1.0f), DelayLine::kMinDelay, maxDist);

    // Equal-power random pan.
    const float pan = 0.5f + 0.5f * params.stereo * rng.bipolar();
    g->gl = std::cos (pan * kPi * 0.5f) * 1.41421356f;
    g->gr = std::sin (pan * kPi * 0.5f) * 1.41421356f;
    g->active = true;
}

int Cloud::activeGrains() const noexcept
{
    int n = 0;
    for (const auto& g : grains) n += g.active ? 1 : 0;
    return n;
}

void Cloud::process (float* left, float* right, int numSamples, const float* duck) noexcept
{
    bool wake = false;
    if (spill.skipBlock (wake)) return;
    if (wake) clearState();

    const float interval = (float) sampleRate / params.density;
    const float maxDist  = (float) (kBufferSecs * sampleRate) - 8.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float on   = spill.nextGain();
        const float norm = normSm.next();
        float dryG, wetG;
        mixGains (mixSm.next(), dryG, wetG);
        dryG = 1.0f + (dryG - 1.0f) * on;

        // Capture (unless frozen).
        if (! frozen)
        {
            buffer[0].push (left[n] * on);
            buffer[1].push (right[n] * on);
        }

        // Schedule grains (10% timing jitter keeps dense clouds from buzzing).
        if (spill.isEnabled() && (countdown -= 1.0f) <= 0.0f)
        {
            spawn();
            countdown += interval * (1.0f + 0.1f * rng.bipolar());
        }

        float wl = 0.0f, wr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            // Smooth bell window (squared Welch): zero slope at both ends, no clicks.
            const float p = g.pos, w = 16.0f * p * p * (1.0f - p) * (1.0f - p);
            const float d = clampf (g.dist, DelayLine::kMinDelay, maxDist);
            wl += buffer[0].read (d) * w * g.gl;
            wr += buffer[1].read (d) * w * g.gr;

            // Read head moves at `ratio`; the write head moves at 1 (or 0 when frozen).
            g.dist += (frozen ? 0.0f : 1.0f) - g.ratio;
            g.pos  += g.inc;
            if (g.pos >= 1.0f) g.active = false;
        }

        const float dk = (duck ? duck[n] : 1.0f) * wetG * norm;
        left[n]  = left[n]  * dryG + wl * dk;
        right[n] = right[n] * dryG + wr * dk;
        spill.trackWet (std::fabs (wl) + std::fabs (wr));
    }
    if (activeGrains() > 0) spill.trackWet (1.0f); // never go idle mid-grain
    spill.endBlock();
}
} // namespace dt
