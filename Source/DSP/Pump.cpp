#include "Pump.h"

namespace dt
{
void Pump::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    glide.prepare (sampleRate);
    depthSm.prepare (sampleRate, 0.03);
    depthSm.setImmediate (clampf (params.depth, 0.0f, 1.0f));
    for (auto& b : band) b.set (kBandHz, sampleRate);
    setParams (params);
    reset();
}

void Pump::reset() noexcept
{
    glide.reset();
    for (auto& b : band) b.reset();
}

void Pump::setParams (const PumpParams& p) noexcept
{
    params = p;
    params.rateIndex = std::clamp (p.rateIndex, 0, (int) kPumpDivs.size() - 1);
    params.band      = std::clamp (p.band, 0, 2);
    // Shape 0..1 -> swell exponent 0.5 (back up at once) .. 4 (holds low, swells late).
    exponent = std::pow (8.0, (double) clampf (p.shape, 0.0f, 1.0f)) * 0.5;
    depthSm.setTarget (clampf (p.depth, 0.0f, 1.0f));
}

void Pump::advance (const TransportClock& clk, int n) noexcept
{
    const double q = kPumpDivs[(size_t) params.rateIndex].quarters;
    const double cycleSec = q * 60.0 / clk.getBpm();
    attackFrac = std::min (0.25, kAttackSec / cycleSec);

    // Same easing as the synced LFOs: a loop wrapping round glides the pump onto the new
    // position over about one cycle instead of jumping mid-swell (a click under a pad).
    if (clk.jumped()) glide.arm();
    glide.setCycleSeconds (cycleSec);
    const double raw = clk.ppqAt (0) / q;
    pos0 = glide.next (raw, n);
    glide.expect (raw + glide.offsetNow() + (clk.ppqAt (n) - clk.ppqAt (0)) / q);
    posStep = clk.getPpqPerSample() / q;
}

void Pump::process (float* left, float* right, int n) noexcept
{
    const auto mode = (PumpBand) params.band;
    for (int i = 0; i < n; ++i)
    {
        const float g = 1.0f - depthSm.next() * duckAt (wrap01 (pos0 + i * posStep));
        if (mode == PumpBand::Full)
        {
            left[i]  *= g;
            right[i] *= g;
            continue;
        }
        // 24 dB/oct Linkwitz-Riley split: steep enough that a 50 Hz bass really ducks
        // (a 12 dB complementary split leaked -8 dB of it into the other band).
        float* io[2] = { &left[i], &right[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float low, high;
            band[ch].process (*io[ch], low, high);
            *io[ch] = mode == PumpBand::Lows ? low * g + high : low + high * g;
        }
    }
}
} // namespace dt
