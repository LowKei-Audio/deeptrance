#pragma once
// Musical time for tempo-synced modules.
// Follows the host's PPQ while the transport plays; free-runs from the last
// position (at the last known tempo) while stopped, so the gate and LFOs keep
// moving when auditioning with the transport off.

#include <algorithm>
#include <cmath>

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
        wasPlaying = jump = false;
    }

    /** Call at the start of every processed chunk. soundFlowing: is anything sounding
        (in or out)? A start over sound is a jump; a start from silence is exact. */
    void beginBlock (const TransportInfo& t, bool soundFlowing = false) noexcept
    {
        bpm = (t.bpm > 0.0) ? std::clamp (t.bpm, 20.0, 999.0) : 138.0;
        ppqPerSample = bpm / 60.0 / sampleRate;
        const bool playing = t.isPlaying && t.hasPpq;
        ppq = playing ? t.ppqAtBlockStart : freePpq;
        // A jump: while playing, more than 64 samples from where the last chunk ended (a
        // loop wrapping round, a seek); or a start while the music still sounds (some
        // hosts report "stopped" for a block when a loop restarts).
        jump = playing && (wasPlaying ? std::abs (ppq - freePpq) > 64.0 * ppqPerSample : soundFlowing);
        wasPlaying = playing;
    }

    /** True for the chunk in which the playhead jumped under sound: synced modules ease
        onto the new position (see CycleGlide) instead of snapping to it. */
    bool jumped() const noexcept { return jump; }

    /** Call after processing `numSamples` of the chunk. */
    void endBlock (int numSamples) noexcept { freePpq = ppq + numSamples * ppqPerSample; }

    double ppqAt (int sampleIndex) const noexcept { return ppq + sampleIndex * ppqPerSample; }
    double getPpqPerSample() const noexcept       { return ppqPerSample; }
    double getBpm() const noexcept                { return bpm; }
    double getSampleRate() const noexcept         { return sampleRate; }

private:
    double sampleRate = 44100.0, bpm = 138.0;
    double ppq = 0.0, freePpq = 0.0, ppqPerSample = 0.0;
    bool wasPlaying = false, jump = false;
};
} // namespace dt
