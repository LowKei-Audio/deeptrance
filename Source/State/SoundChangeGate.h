#pragma once
// SoundChangeGate: click-free preset changes (shared LowKei Audio pattern).
//
// Why this exists
//   A preset load sets ~100 parameters from the message thread, one by one: first
//   every one back to its default, then the preset's values. The audio thread can
//   run a block in the middle (a mix of Init and the new preset), and most settings
//   (delay times, filter cutoffs, stage counts, chain order, switches) jump in a
//   single sample. Both are heard as a click or a burst of static.
//
// What it does
//   Message thread: wrap every bulk change in a Scope (preset, host program change,
//   user preset, A/B, randomise, state load). While a scope is open, or once one has
//   closed since the audio thread last looked, the audio thread:
//     1. keeps playing the old settings and fades the output to the aligned dry signal,
//     2. once fully dry and no scope is open: reads all the new settings at once and
//        clears the old sound's tails,
//     3. fades the new sound in.
//   Lock-free: two atomics. Nested scopes are fine.
//
// Usage (message thread)            { SoundChangeGate::Scope s (gate); ...set parameters... }
// Usage (audio thread, per block)   if (gate.changePending (seen)) ...;  gate.busy();

#include <atomic>
#include <cstdint>

namespace lowkei
{
class SoundChangeGate
{
public:
    void begin() noexcept { depth.fetch_add (1, std::memory_order_acq_rel); serial.fetch_add (1, std::memory_order_acq_rel); }
    void end() noexcept   { serial.fetch_add (1, std::memory_order_acq_rel); depth.fetch_sub (1, std::memory_order_acq_rel); }

    struct Scope
    {
        explicit Scope (SoundChangeGate& g) noexcept : gate (g) { gate.begin(); }
        ~Scope() { gate.end(); }
        Scope (const Scope&) = delete;
        Scope& operator= (const Scope&) = delete;
        SoundChangeGate& gate;
    };

    /** True while a change is being written (do not read parameters yet). */
    bool busy() const noexcept { return depth.load (std::memory_order_acquire) > 0; }

    /** Current change counter; compare with the value seen at the last swap. */
    uint32_t current() const noexcept { return serial.load (std::memory_order_acquire); }

private:
    std::atomic<int> depth { 0 };
    std::atomic<uint32_t> serial { 0 };
};
} // namespace lowkei
