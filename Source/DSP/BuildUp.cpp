#include "BuildUp.h"

namespace dt
{
void BuildUp::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    amountSm.prepare (sampleRate, 0.05);
    noiseSm.prepare (sampleRate, 0.05);
    reset();
}

void BuildUp::reset() noexcept
{
    for (auto& ch : hpState) for (auto& s : ch) s.reset();
    for (auto& s : bpState) s.reset();
    coeffCountdown = 0;
}

void BuildUp::setParams (float amount, float noiseLevel) noexcept
{
    amountSm.setTarget (clampf (amount, 0.0f, 1.0f));
    noiseSm.setTarget (clampf (noiseLevel, 0.0f, 1.0f));
}

void BuildUp::process (float* left, float* right, int numSamples) noexcept
{
    float* io[2] = { left, right };
    Random* rng[2] = { &rngL, &rngR };

    for (int i = 0; i < numSamples; ++i)
    {
        const float b = amountSm.next();
        const float noise = noiseSm.next() * 0.35f * b * b; // riser grows faster near the top

        if (--coeffCountdown <= 0)
        {
            const float fc = cutoffFor (b);
            hp1.set (fc, 0.5412f, sampleRate); // 4th-order Butterworth = Q 0.541 + 1.307
            hp2.set (fc, 1.3066f, sampleRate);
            bp.set (clampf (fc * 4.0f, 300.0f, 12000.0f), 0.9f, sampleRate);
            coeffCountdown = 16;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            float x = hpState[ch][1].process (hpState[ch][0].process (io[ch][i], hp1).hp, hp2).hp;
            if (noise > 0.0f)
                x += noise * bpState[ch].bandpassNorm (rng[ch]->bipolar(), bp);
            io[ch][i] = x;
        }
    }
}
} // namespace dt
