#pragma once
// DeepTrance signal chain (fixed order in v1):
//   Input -> Gate -> Vowel -> Swirl -> Drift -> [Phase 2+: Cloud, Delay, Abyss, Punch] -> Mix -> Output
// Host blocks of any size are processed in fixed internal chunks, so module
// scratch memory is allocated once in prepare() and never on the audio thread.

#include "Common.h"
#include "Drift.h"
#include "ModuleFader.h"
#include "Swirl.h"
#include "TranceGate.h"
#include "TransportClock.h"
#include "VowelFilter.h"
#include <vector>

namespace dt
{
struct EngineParams
{
    float inputGainDb  = 0.0f;
    float outputGainDb = 0.0f;
    float mix          = 1.0f; // global dry/wet, 0..1

    bool gateOn  = false;
    bool vowelOn = false;
    bool swirlOn = true;
    bool driftOn = true;

    GateParams  gate;
    VowelParams vowel;
    SwirlParams swirl;
    DriftParams drift;
};

class DeepTranceEngine
{
public:
    static constexpr int kChunk = 256;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const EngineParams& p) noexcept;

    /** Stereo, in-place. Real-time safe. */
    void process (float* left, float* right, int numSamples, const TransportInfo& transport) noexcept;

    /** Zero latency: every module is latency-free (wet delays are part of the effect). */
    static constexpr int getLatencySamples() noexcept { return 0; }

    const TranceGate& getGate() const noexcept { return gate; }

private:
    void processChunk (float* left, float* right, int n, const TransportInfo& t) noexcept;

    template <typename Fn>
    void runModule (ModuleFader& fader, float* l, float* r, int n, Fn&& process) noexcept;

    double       sampleRate = 44100.0;
    EngineParams params;

    TransportClock clock;
    TranceGate     gate;
    VowelFilter    vowel;
    Swirl          swirl;
    Drift          drift;
    ModuleFader    gateFader, vowelFader, swirlFader, driftFader;

    Smoother inGainSm, outGainSm, mixSm;
    std::vector<float> dryL, dryR, modL, modR; // kChunk-sized scratch
};
} // namespace dt
