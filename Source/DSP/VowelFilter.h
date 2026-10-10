#pragma once
// Formant / Vowel Filter ("The Voice"): three parallel formant band-passes that
// morph smoothly through A-E-I-O-U. A built-in tempo-synced LFO sweeps the
// morph for "talking" leads; the mod matrix (Phase 4) adds more sources.

#include "Common.h"
#include "Filters.h"
#include "TransportClock.h"

namespace dt
{
struct VowelParams
{
    float morph     = 0.0f;  // 0..4  (A, E, I, O, U)
    int   lfoRate   = 4;     // index into kSyncDivs
    float lfoDepth  = 0.0f;  // 0..1, sweeps up to +/-2 vowels
    float resonance = 1.0f;  // 0.5..2, scales formant Q
    float mix       = 1.0f;  // 0..1
};

class VowelFilter
{
public:
    static constexpr int kNumFormants = 3;
    static constexpr std::array<const char*, 5> kVowelNames { "A", "E", "I", "O", "U" };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const VowelParams& p) noexcept;
    void process (float* left, float* right, int numSamples, const TransportClock& clock) noexcept;

    /** Formant frequencies for a (possibly fractional) vowel position. Exposed for tests. */
    static void formantsFor (float morph, float freqs[kNumFormants], float gains[kNumFormants]) noexcept;

private:
    void updateCoeffs (float morph) noexcept;

    double      sampleRate = 44100.0;
    VowelParams params;
    Smoother    morphSm, depthSm, mixSm, resSm;
    SvfCoeffs   coeffs[kNumFormants];
    float       gains[kNumFormants] {};
    float       makeup = 1.0f;
    SvfState    state[2][kNumFormants];
    int         coeffCountdown = 0;
};
} // namespace dt
