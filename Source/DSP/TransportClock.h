#pragma once
// Musical time for tempo-synced modules.
// Follows the host's PPQ while the transport plays; free-runs from the last
// position (at the last known tempo) while stopped, so the gate and LFOs keep
// moving when auditioning with the transport off.

#include <algorithm>

namespace dt
{
struct TransportInfo
{
    double bpm             = 138.0; // trance-friendly fallback when the host gives none
    double ppqAtBlockStart = 0.0;
    bool   isPlaying       = false;
    bool   hasPpq          = false;
};

class TransportClock
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        ppq = freePpq = 0.0;
    }

    /** Call at the start of every processed chunk. */
    void beginBlock (const TransportInfo& t) noexcept
    {
        bpm = (t.bpm > 0.0) ? std::clamp (t.bpm, 20.0, 999.0) : 138.0;
        ppqPerSample = bpm / 60.0 / sampleRate;
        ppq = (t.isPlaying && t.hasPpq) ? t.ppqAtBlockStart : freePpq;
    }

    /** Call after processing `numSamples` of the chunk. */
    void endBlock (int numSamples) noexcept { freePpq = ppq + numSamples * ppqPerSample; }

    double ppqAt (int sampleIndex) const noexcept { return ppq + sampleIndex * ppqPerSample; }
    double getPpqPerSample() const noexcept       { return ppqPerSample; }
    double getBpm() const noexcept                { return bpm; }
    double getSampleRate() const noexcept         { return sampleRate; }

private:
    double sampleRate = 44100.0, bpm = 138.0;
    double ppq = 0.0, freePpq = 0.0, ppqPerSample = 0.0;
};
} // namespace dt
