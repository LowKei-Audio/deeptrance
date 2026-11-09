#pragma once
// StateGuard: keeps every touch of apvts.state on the message thread.
//
// Why this exists
//   Hosts (and pluginval) may call getStateInformation / setStateInformation on a
//   background thread. The editor reads and writes non-parameter properties of
//   apvts.state (view mode, window size, preset name, ...) on the message thread.
//   juce::ValueTree is not thread-safe: replaceState() on one thread while another
//   thread reads or writes the live tree frees nodes that are still in use, and the
//   heap gets corrupted (seen as rare pluginval crashes; TSan reports it every run).
//
// What it does
//   setState(tree)  On the message thread: replaceState at once.
//                   Elsewhere: parameter values are applied at once (parameters are
//                   atomic, so the host reads back what it just loaded), and the whole
//                   tree is handed to the message thread, which calls replaceState.
//   getState()      On the message thread: copyState.
//                   Elsewhere: a copy of the pending tree, or of a snapshot the message
//                   thread keeps current, with live parameter values written in.
//   With no MessageManager (console unit tests) both act directly, as before.
//
// Usage
//   StateGuard guard { apvts };            // declare after apvts
//   getStateInformation: auto tree = guard.getState();  ... serialise tree
//   setStateInformation: ... parse / migrate tree ...;  guard.setState (tree);
//   guard.onApplied = [this] { ... };      // optional: runs on the message thread
//                                          // after a loaded state is in apvts.state
//   guard.onDeferredLoad = [this] (const juce::ValueTree& t) { ... };
//                                          // optional: audio-side data from a state
//                                          // loaded off the message thread

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace lowkei
{

class StateGuard : private juce::AsyncUpdater,
                   private juce::ValueTree::Listener
{
public:
    explicit StateGuard (juce::AudioProcessorValueTreeState& s)
        : apvts (s), type (s.state.getType())
    {
        apvts.state.addListener (this);   // the listener stays with apvts.state across replaceState
        refreshSnapshot();
    }

    ~StateGuard() override
    {
        cancelPendingUpdate();
        apvts.state.removeListener (this);
    }

    /** The root type of the state tree, safe to read from any thread. */
    const juce::Identifier& stateType() const noexcept { return type; }

    /** Load a state (already parsed and migrated). Safe from any thread. */
    void setState (const juce::ValueTree& tree)
    {
        if (onMessageThread())
        {
            {
                const juce::ScopedLock sl (lock);
                pending = {};               // a direct load supersedes a queued one
            }
            cancelPendingUpdate();
            apply (tree);
            return;
        }

        // Parameters first and at once: the host may read them back immediately.
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto child = tree.getChildWithProperty ("id", rp->paramID);
                // replaceState puts a parameter the tree does not mention at its default.
                const float norm = child.hasProperty ("value")
                                     ? rp->convertTo0to1 ((float) child.getProperty ("value"))
                                     : rp->getDefaultValue();
                if (rp->getValue() != norm)
                    rp->setValueNotifyingHost (norm);
            }

        if (onDeferredLoad) onDeferredLoad (tree);

        {
            const juce::ScopedLock sl (lock);
            pending = tree.createCopy();
        }
        triggerAsyncUpdate();
    }

    /** A copy of the current state, ready to serialise. Safe from any thread.
        fromPendingLoad (optional) is set when the copy is a load still on its way to the
        message thread: it already holds everything that load carried, session data included. */
    juce::ValueTree getState (bool* fromPendingLoad = nullptr)
    {
        if (fromPendingLoad != nullptr) *fromPendingLoad = false;
        if (onMessageThread())
        {
            handleUpdateNowIfNeeded();      // a queued load counts as loaded
            return apvts.copyState();
        }

        juce::ValueTree t;
        {
            const juce::ScopedLock sl (lock);
            if (fromPendingLoad != nullptr) *fromPendingLoad = pending.isValid();
            t = (pending.isValid() ? pending : snapshot).createCopy();
        }
        writeLiveParameters (t);
        return t;
    }

    /** Runs on the message thread after a loaded state has been put in apvts.state. */
    std::function<void()> onApplied;

    /** Runs on the loading thread when a load is handed to the message thread, after the
        parameters are set. For audio-side data derived from the tree: read it from the
        incoming tree here, so playback uses the loaded state at once. Must not touch
        apvts.state. */
    std::function<void (const juce::ValueTree&)> onDeferredLoad;

private:
    static bool onMessageThread()
    {
        // No MessageManager (console tests): there is no other thread to protect against.
        return juce::MessageManager::getInstanceWithoutCreating() == nullptr
            || juce::MessageManager::existsAndIsCurrentThread();
    }

    void handleAsyncUpdate() override
    {
        juce::ValueTree queued;
        {
            const juce::ScopedLock sl (lock);
            queued = pending;
        }
        if (! queued.isValid()) return;

        // Parameters may have moved (automation) since the load was queued; keep them.
        auto t = queued.createCopy();
        writeLiveParameters (t);
        apply (t);

        // The load stays "pending" until onApplied has run, so a save in between still
        // returns it whole. A newer load queued meanwhile stays queued.
        const juce::ScopedLock sl (lock);
        if (pending == queued) pending = {};
    }

    void apply (const juce::ValueTree& tree)
    {
        apvts.replaceState (tree);
        refreshSnapshot();
        if (onApplied) onApplied();
    }

    void writeLiveParameters (juce::ValueTree& t) const
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                auto child = t.getChildWithProperty ("id", rp->paramID);
                if (! child.isValid())
                {
                    child = juce::ValueTree ("PARAM");
                    child.setProperty ("id", rp->paramID, nullptr);
                    t.appendChild (child, nullptr);
                }
                // Keep a stored value the parameter still matches exactly: converting it
                // back from 0..1 can move it by a rounding step, and hosts and validators
                // expect a loaded state to save back bit for bit.
                const float norm = rp->getValue();
                if (child.hasProperty ("value") && rp->convertTo0to1 ((float) child.getProperty ("value")) == norm)
                    continue;
                child.setProperty ("value", rp->convertFrom0to1 (norm), nullptr);
            }
    }

    // Message thread only. The snapshot is replaced, never edited, so a reader holding
    // the lock always copies a complete tree.
    void refreshSnapshot()
    {
        auto copy = apvts.state.createCopy();
        const juce::ScopedLock sl (lock);
        snapshot = std::move (copy);
    }

    // Parameter values are written in from the parameters themselves, so the frequent
    // "value" flushes do not need a fresh snapshot. Everything else does.
    bool isParameterValue (juce::ValueTree& t, const juce::Identifier& prop) const
    {
        return prop == juce::Identifier ("value") && t.hasType ("PARAM") && t.getParent() == apvts.state;
    }

    void valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& prop) override
    {
        if (! isParameterValue (t, prop)) refreshSnapshot();
    }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { refreshSnapshot(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { refreshSnapshot(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override         { refreshSnapshot(); }
    void valueTreeRedirected (juce::ValueTree&) override                          { refreshSnapshot(); }

    juce::AudioProcessorValueTreeState& apvts;
    const juce::Identifier type;

    juce::CriticalSection lock;
    juce::ValueTree snapshot;   // (lock) last message-thread state
    juce::ValueTree pending;    // (lock) a load queued for the message thread
};

} // namespace lowkei
