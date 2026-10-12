#include "Swirl.h"

namespace dt
{
void Swirl::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& c : ch)
        c.flanger.prepare ((int) std::ceil (kFlangerMaxDelayMs * 0.001 * sampleRate) + 8);

    for (auto* s : { &depthSm, &fbSm, &lowSm, &mixSm }) s->prepare (sampleRate, 0.03);
    depthSm.setImmediate (params.depth);
    fbSm.setImmediate (params.feedback);
    lowSm.setImmediate (params.lowAmount);
    mixSm.setImmediate (params.mix);

    currentXover = 0.0f; // force crossover coefficient refresh
    setParams (params);
    reset();
}

void Swirl::reset() noexcept
{
    for (auto& c : ch)
    {
        c.xover.reset();
        for (auto& s : c.stages) s.reset();
        c.flanger.reset();
        c.phaserFb = c.flangerFb = 0.0f;
    }
    coeffCountdown = 0;
}

void Swirl::setParams (const SwirlParams& p) noexcept
{
    params = p;
    params.rateIndex = std::clamp (p.rateIndex, 0, (int) kSyncDivs.size() - 1);
    params.stereoDeg = clampf (p.stereoDeg, 0.0f, 180.0f);

    const float x = clampf (p.xoverHz, 60.0f, 1000.0f);
    if (x != currentXover)
    {
        currentXover = x;
        for (auto& c : ch) c.xover.set (x, sampleRate);
    }

    depthSm.setTarget (clampf (p.depth, 0.0f, 1.0f));
    fbSm.setTarget (clampf (p.feedback, 0.0f, 0.95f));
    lowSm.setTarget (clampf (p.lowAmount, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
}

float Swirl::runPhaser (Channel& c, float x, float fb) noexcept
{
    // Feedback is soft-clipped so extreme settings resonate instead of exploding.
    float y = x + std::tanh (c.phaserFb * fb);
    for (auto& s : c.stages) y = s.process (y, c.apCoef);
    c.phaserFb = y;
    return 0.5f * (x + y); // notches come from summing with the input
}

float Swirl::runFlanger (Channel& c, float x, float delaySamples, float fb) noexcept
{
    const float d = c.flanger.read (delaySamples);
    c.flanger.push (x + std::tanh (d * fb));
    return 0.5f * (x + d);
}

void Swirl::process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept
{
    const double cycleQuarters = syncQuarters (params.rateIndex);
    const double stereoOffset  = params.stereoDeg / 360.0;
    const bool   usePhaser     = params.mode != SwirlMode::Flanger;
    const bool   useFlanger    = params.mode != SwirlMode::Phaser;
    const float  msToSamples   = (float) (0.001 * sampleRate);

    float flangerDelay[2] {};
    float* io[2] = { left, right };

    // At normal speed the LFO is locked to the host grid. Sped up (Build-Up), it
    // free-runs from wherever it was, so speeding up never jumps the sweep.
    const double speed  = std::max (1.0, (double) params.speedMul);
    const bool   synced = speed < 1.0001;
    const double freeInc = clock.getPpqPerSample() / cycleQuarters * speed;

    for (int i = 0; i < numSamples; ++i)
    {
        const float depth = depthSm.next();
        const float fb    = fbSm.next();
        const float low   = lowSm.next();
        const float mix   = mixSm.next();

        const double phase = synced ? wrap01 (clock.ppqAt (i) / cycleQuarters)
                                    : wrap01 (freePhase + freeInc);
        freePhase = phase;
        const bool   updateCoeffs = (--coeffCountdown <= 0);
        if (updateCoeffs) coeffCountdown = 8;

        for (int c = 0; c < 2; ++c)
        {
            auto& st = ch[c];
            const float lfo = (float) std::sin (kTwoPiD * (phase + (c == 1 ? stereoOffset : 0.0)));

            if (usePhaser && updateCoeffs)
            {
                // Sweep +/- up to ~2.45 octaves around ~1.1 kHz (about 200 Hz .. 6 kHz at full depth).
                const float f = 1095.0f * std::exp2 (2.45f * depth * lfo);
                st.apCoef = Allpass1::coefFor (f, sampleRate);
            }
            if (useFlanger)
                flangerDelay[c] = (0.3f + depth * 5.0f * (0.5f + 0.5f * lfo)) * msToSamples;

            float lo, hi;
            st.xover.process (io[c][i], lo, hi);

            // Swirl the high band plus `low` of the low band; the rest of the low band stays clean.
            float fx = hi + low * lo;
            if (usePhaser)  fx = runPhaser (st, fx, fb);
            if (useFlanger) fx = runFlanger (st, fx, flangerDelay[c], fb);

            const float wet = fx + (1.0f - low) * lo;
            const float dry = lo + hi;
            io[c][i] = dry + (wet - dry) * mix;
        }
    }
}
} // namespace dt
