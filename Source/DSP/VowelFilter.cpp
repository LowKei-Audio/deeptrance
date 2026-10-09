#include "VowelFilter.h"

namespace dt
{
namespace
{
    // Tenor formant table (Hz, dB). Classic values used in formant synthesis.
    struct Vowel { float f[3]; float db[3]; };
    constexpr Vowel kVowels[5] = {
        { { 650.0f, 1080.0f, 2650.0f }, { 0.0f,  -6.0f,  -7.0f } }, // A
        { { 400.0f, 1700.0f, 2600.0f }, { 0.0f, -14.0f, -12.0f } }, // E
        { { 290.0f, 1870.0f, 2800.0f }, { 0.0f, -15.0f, -18.0f } }, // I
        { { 400.0f,  800.0f, 2600.0f }, { 0.0f, -10.0f, -12.0f } }, // O
        { { 350.0f,  600.0f, 2700.0f }, { 0.0f, -20.0f, -17.0f } }  // U
    };
    constexpr float kBandwidthHz[3] = { 80.0f, 90.0f, 120.0f };

    // Narrow formant bands pass little energy; this restores roughly unity
    // loudness for broadband material (tuned against noise/saw in the tests).
    constexpr float kMakeupDb = 12.0f;
}

void VowelFilter::formantsFor (float morph, float freqs[kNumFormants], float outGains[kNumFormants]) noexcept
{
    morph = clampf (morph, 0.0f, 4.0f);
    const int   a = std::min (3, (int) morph);
    const float t = morph - (float) a;

    for (int i = 0; i < kNumFormants; ++i)
    {
        // Interpolate pitch in the log domain so sweeps sound even.
        const float la = std::log2 (kVowels[a].f[i]), lb = std::log2 (kVowels[a + 1].f[i]);
        freqs[i]    = std::exp2 (lerpf (la, lb, t));
        outGains[i] = dbToGain (lerpf (kVowels[a].db[i], kVowels[a + 1].db[i], t));
    }
}

void VowelFilter::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto* s : { &morphSm, &depthSm, &mixSm, &resSm }) s->prepare (sampleRate, 0.03);
    morphSm.setImmediate (params.morph);
    depthSm.setImmediate (params.lfoDepth);
    mixSm.setImmediate (params.mix);
    resSm.setImmediate (params.resonance);
    reset();
}

void VowelFilter::reset() noexcept
{
    for (auto& ch : state) for (auto& s : ch) s.reset();
    coeffCountdown = 0;
}

void VowelFilter::setParams (const VowelParams& p) noexcept
{
    params = p;
    params.lfoRate = std::clamp (p.lfoRate, 0, (int) kSyncDivs.size() - 1);
    morphSm.setTarget (clampf (p.morph, 0.0f, 4.0f));
    depthSm.setTarget (clampf (p.lfoDepth, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
    resSm.setTarget (clampf (p.resonance, 0.5f, 2.0f));
}

void VowelFilter::updateCoeffs (float morph) noexcept
{
    float freqs[kNumFormants];
    formantsFor (morph, freqs, gains);

    const float res = resSm.getTarget();
    for (int i = 0; i < kNumFormants; ++i)
        coeffs[i].set (freqs[i], clampf (freqs[i] / kBandwidthHz[i] * res, 0.5f, 40.0f), sampleRate);

    // Higher resonance narrows the bands; compensate so loudness stays put.
    makeup = dbToGain (kMakeupDb) * std::sqrt (res);
}

void VowelFilter::process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept
{
    const double cycleQuarters = syncQuarters (params.lfoRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float base  = morphSm.next();
        const float depth = depthSm.next();
        const float mix   = mixSm.next();
        resSm.next();

        // Control-rate coefficient updates (every 8 samples) keep CPU low.
        if (--coeffCountdown <= 0)
        {
            const float lfo = (float) std::sin (kTwoPiD * wrap01 (clock.ppqAt (i) / cycleQuarters));
            updateCoeffs (base + depth * 2.0f * lfo);
            coeffCountdown = 8;
        }

        float* io[2] = { left + i, right + i };
        for (int ch = 0; ch < 2; ++ch)
        {
            const float dry = *io[ch];
            float wet = 0.0f;
            for (int f = 0; f < kNumFormants; ++f)
                wet += gains[f] * state[ch][f].bandpassNorm (dry, coeffs[f]);
            *io[ch] = dry + (wet * makeup - dry) * mix;
        }
    }
}
} // namespace dt
