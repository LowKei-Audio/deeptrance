#pragma once
#include "../State/SoundChangeGate.h"
// DeepTrance signal chain (fixed order):
//
//   Input -> Gate -> Vowel -> Swirl -> Drift -> Acid -> Build-Up (HPF + riser)
//         -> Punch (transient shaping + wet ducking)
//         -> Cloud -> Fractal Delay -> Abyss       (wet parts ducked by Punch)
//         -> Astral sweep (combo) -> Mix -> Output
//
// Parameter flow every 32-sample chunk:
//   user values -> mod matrix -> Easy macros -> Build-Up -> Combo -> modules
// The engine is real-time safe: all memory is allocated in prepare().

#include "AbyssReverb.h"
#include "AcidFilter.h"
#include "BuildUp.h"
#include "Cloud.h"
#include "Common.h"
#include "Drift.h"
#include "FractalDelay.h"
#include "Modulation.h"
#include "ModuleFader.h"
#include "Punch.h"
#include "Swirl.h"
#include "TranceGate.h"
#include "TransportClock.h"
#include "VowelFilter.h"
#include <vector>

namespace dt
{
enum class Combo { Off = 0, AstralProjection, AcidBath };
inline constexpr std::array<const char*, 3> kComboNames { "Off", "Astral Projection", "Acid Bath" };

/** Easy-view intensity macros. 0.5 = exactly the Pro settings, 0 = none, 1 = double. */
struct MacroParams
{
    float gate = 0.5f, swirl = 0.5f, space = 0.5f, drift = 0.5f;
};

struct EngineParams
{
    float inputGainDb  = 0.0f;
    float outputGainDb = 0.0f;
    float mix          = 1.0f;

    bool gateOn = false, vowelOn = false, swirlOn = true, driftOn = true, acidOn = false;
    bool punchOn = false, cloudOn = false, delayOn = true, abyssOn = true;

    GateParams  gate;
    VowelParams vowel;
    SwirlParams swirl;
    DriftParams drift;
    AcidParams  acid;
    PunchParams punch;
    CloudParams cloud;
    DelayParams delay;
    AbyssParams abyss;

    float buildUp      = 0.0f; // 0..1
    float buildUpNoise = 0.5f; // 0..1 riser level
    MacroParams macros;
    Combo combo        = Combo::Off;
    float comboAmount  = 1.0f;

    std::array<LfoParams, 2>        lfo {};
    EnvParams                       env;
    std::array<ModSlot, kModSlots>  mod {};
};

/** Modulation destinations. Order is part of the saved-state format: append only. */
enum class ModDest
{
    None = 0,
    GateDepth, GateFilter,
    VowelMorph, VowelMix,
    SwirlDepth, SwirlFeedback, SwirlMix,
    DriftAmount, DriftMix,
    AcidCutoff, AcidResonance, AcidDrive,
    CloudPosition, CloudSize, CloudDensity, CloudPitch, CloudJitter, CloudMix,
    DelayFeedback, DelayPitch, DelayTone, DelayMix,
    AbyssSize, AbyssDecay, AbyssShimmer, AbyssMix,
    PunchAttack, BuildUp, GlobalMix,
    Count
};
const std::array<const char*, (size_t) ModDest::Count>& modDestinationNames() noexcept;

class DeepTranceEngine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate);
    void reset() noexcept;
    /** The params to use. Taken at the start of the next process() call, unless a sound
        change is fading out: then the old sound keeps its params until the swap. */
    void setParams (const EngineParams& p) noexcept { incoming = p; }

    /** Sound changes: wrap every bulk parameter change (preset, host program, state load)
        in a lowkei::SoundChangeGate::Scope on this gate (the processor does). The engine
        keeps the old sound, fades to dry (10 ms), feeds every module silence for 10 ms,
        takes every new setting at once and fades back in (30 ms). Over silence the
        change is taken at once. */
    lowkei::SoundChangeGate soundGate;
    float soundTransition() const noexcept { return transition; }   // for tests: 1 = fully dry

    /** Stereo, in-place. Real-time safe. */
    void process (float* left, float* right, int numSamples, const TransportInfo& transport) noexcept;

    /** Zero latency: every module is latency-free (wet delays are part of the effect). */
    static constexpr int getLatencySamples() noexcept { return 0; }

    /** Applies modulation, macros, Build-Up and combo to user params (public for tests). */
    static EngineParams derive (const EngineParams& user, float lfo1, float lfo2, float env) noexcept;

    // Live values for the UI (read from the message thread; benign races on floats).
    float getLfoValue (int i) const noexcept { return lfoOut[i]; }
    float getEnvValue() const noexcept       { return envOut; }
    float getGateEnvelope() const noexcept   { return gate.getEnvelope(); }
    int   getActiveGrains() const noexcept   { return cloud.activeGrains(); }

private:
    void processChunk (float* left, float* right, int n, const TransportInfo& t) noexcept;
    void applyToModules (const EngineParams& e, bool snapEnables) noexcept;

    template <typename Fn>
    void runModule (ModuleFader& fader, float* l, float* r, int n, Fn&& process) noexcept;

    void beginBlockSoundChange() noexcept;
    void swapSound() noexcept;

    double       sampleRate = 44100.0;
    EngineParams params, incoming;

    // Sound change state (audio thread). transition: 0 = normal, 1 = fully dry.
    uint32_t seenSoundChange = 0;
    bool  holdingSound = false;
    float transition = 0.0f, transitionTarget = 0.0f, transitionOut = 0.0f, transitionIn = 0.0f;
    int   swapHold = 0, swapHoldSamples = 480;
    float lastBlockPeak = 0.0f;   // in or out, last block: is anything sounding?
    CycleGlide astralGlide;       // Astral Projection's sweep across playhead jumps

    TransportClock clock;
    TranceGate     gate;
    VowelFilter    vowel;
    Swirl          swirl;
    Drift          drift;
    AcidFilter     acid;
    BuildUp        buildUp;
    Punch          punch;
    Cloud          cloud;
    FractalDelay   delay;
    AbyssReverb    abyss;
    ModuleFader    gateFader, vowelFader, swirlFader, driftFader, acidFader, buildFader;

    Lfo              lfo[2];
    EnvelopeFollower envFollower;
    float            lfoOut[2] {}, envOut = 0.0f;

    // Astral Projection's slow 8-bar low-pass sweep.
    Smoother  astralSm;
    SvfCoeffs astralCoeffs;
    SvfState  astralLp[2];

    Smoother inGainSm, outGainSm, mixSm;
    std::vector<float> dryL, dryR, modL, modR, duck; // kChunk-sized scratch
};
} // namespace dt
