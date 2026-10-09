#include "DeepTranceEngine.h"
#include <cstring>

namespace dt
{
void DeepTranceEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    clock.prepare (sampleRate);

    gate.prepare (sampleRate);
    vowel.prepare (sampleRate);
    swirl.prepare (sampleRate);
    drift.prepare (sampleRate);

    for (auto* f : { &gateFader, &vowelFader, &swirlFader, &driftFader }) f->prepare (sampleRate);

    for (auto* s : { &inGainSm, &outGainSm, &mixSm }) s->prepare (sampleRate, 0.02);

    for (auto* v : { &dryL, &dryR, &modL, &modR }) v->assign (kChunk, 0.0f);

    setParams (params);
    reset();
}

void DeepTranceEngine::reset() noexcept
{
    gate.reset(); vowel.reset(); swirl.reset(); drift.reset();

    // Start in the requested state with no fades or ramps pending.
    for (auto* f : { &gateFader, &vowelFader, &swirlFader, &driftFader }) f->snap();
    inGainSm.setImmediate (dbToGain (params.inputGainDb));
    outGainSm.setImmediate (dbToGain (params.outputGainDb));
    mixSm.setImmediate (params.mix);
}

void DeepTranceEngine::setParams (const EngineParams& p) noexcept
{
    params = p;

    gateFader.setEnabled (p.gateOn);
    vowelFader.setEnabled (p.vowelOn);
    swirlFader.setEnabled (p.swirlOn);
    driftFader.setEnabled (p.driftOn);

    gate.setParams (p.gate);
    vowel.setParams (p.vowel);
    swirl.setParams (p.swirl);
    drift.setParams (p.drift);

    inGainSm.setTarget (dbToGain (clampf (p.inputGainDb, -24.0f, 24.0f)));
    outGainSm.setTarget (dbToGain (clampf (p.outputGainDb, -24.0f, 24.0f)));
    mixSm.setTarget (clampf (p.mix, 0.0f, 1.0f));
}

template <typename Fn>
void DeepTranceEngine::runModule (ModuleFader& fader, float* l, float* r, int n, Fn&& process) noexcept
{
    if (! fader.isActive())
        return; // fully off: skip, zero CPU

    if (fader.isWakingUp())
        process.reset(); // clear stale tails before fading back in

    if (fader.isFullyOn())
    {
        process (l, r, n);
        return;
    }

    // Fading: run on a copy, then crossfade with the untouched input.
    std::memcpy (modL.data(), l, sizeof (float) * (size_t) n);
    std::memcpy (modR.data(), r, sizeof (float) * (size_t) n);
    process (modL.data(), modR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        const float g = fader.next();
        l[i] += (modL[(size_t) i] - l[i]) * g;
        r[i] += (modR[(size_t) i] - r[i]) * g;
    }
}

namespace
{
    // Adapts a module to the small call shape runModule expects.
    template <typename Module, typename Call>
    struct Runner
    {
        Module& m; Call call;
        void reset() noexcept { m.reset(); }
        void operator() (float* l, float* r, int n) noexcept { call (m, l, r, n); }
    };
    template <typename Module, typename Call>
    Runner<Module, Call> runner (Module& m, Call c) { return { m, c }; }
}

void DeepTranceEngine::processChunk (float* left, float* right, int n, const TransportInfo& t) noexcept
{
    clock.beginBlock (t);

    // Input gain, then keep a dry copy for the global mix.
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.next();
        left[i]  *= g;
        right[i] *= g;
    }
    std::memcpy (dryL.data(), left,  sizeof (float) * (size_t) n);
    std::memcpy (dryR.data(), right, sizeof (float) * (size_t) n);

    const auto& clk = clock;
    runModule (gateFader,  left, right, n, runner (gate,  [&clk] (TranceGate& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    runModule (vowelFader, left, right, n, runner (vowel, [&clk] (VowelFilter& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    runModule (swirlFader, left, right, n, runner (swirl, [&clk] (Swirl& m, float* l, float* r, int k) { m.process (l, r, k, clk); }));
    runModule (driftFader, left, right, n, runner (drift, [] (Drift& m, float* l, float* r, int k) { m.process (l, r, k); }));

    // Global dry/wet, then output gain.
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSm.next();
        const float out = outGainSm.next();
        left[i]  = (dryL[(size_t) i] + (left[i]  - dryL[(size_t) i]) * mix) * out;
        right[i] = (dryR[(size_t) i] + (right[i] - dryR[(size_t) i]) * mix) * out;
    }

    clock.endBlock (n);
}

void DeepTranceEngine::process (float* left, float* right, int numSamples, const TransportInfo& transport) noexcept
{
    ScopedFlushDenormals noDenormals;
    const double ppqPerSample = (transport.bpm > 0.0 ? transport.bpm : 138.0) / 60.0 / sampleRate;

    for (int offset = 0; offset < numSamples; offset += kChunk)
    {
        const int n = std::min (kChunk, numSamples - offset);
        TransportInfo t = transport;
        t.ppqAtBlockStart += offset * ppqPerSample; // keep host sync exact across chunks
        processChunk (left + offset, right + offset, n, t);
    }
}
} // namespace dt
