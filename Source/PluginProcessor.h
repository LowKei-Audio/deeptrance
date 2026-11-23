#pragma once

#include "DSP/DeepTranceEngine.h"
#include "Presets/PresetManager.h"
#include "State/StateGuard.h"
#include <juce_audio_processors/juce_audio_processors.h>

class DeepTranceProcessor final : public juce::AudioProcessor
{
public:
    DeepTranceProcessor();
    ~DeepTranceProcessor() override = default;

    // Audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Info
    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi() const override  { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; } // long reverb tails (Freeze can hold longer)

    // Programs = factory presets, so hosts' own preset menus list them too.
    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // State
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    dt::PresetManager& getPresets() noexcept               { return presets; }
    const dt::DeepTranceEngine& getEngine() const noexcept { return engine; }

    /** Easy/Pro is a view choice, saved with the session but not automatable. */
    bool isProMode() const;
    void setProMode (bool pro);

    /** Loads a user preset file (parameters + drawn LFO shapes). */
    bool loadUserPreset (const juce::File& file);

    // --- A/B compare and Randomize (message thread) -------------------------------------
    /** Two complete sounds to compare. Switching keeps the edits made in each slot; the
        other slot starts as a copy of the first. Held in memory for this session. */
    int  getABSlot() const noexcept { return abActive; }
    void selectAB (int slot);
    /** Copies the sound you hear into the other slot. */
    void copyToOtherSlot();
    /** A new sound from safe, musical ranges (modules, rhythms, spaces; never levels).
        seed 0 = a different sound every time; a fixed seed repeats one (tests). */
    void randomize (juce::int64 seed = 0);

    /** Custom ("drawn") LFO shapes, 16 points 0..1. Thread-safe; saved with the session. */
    float getCustomPoint (int lfo, int point) const noexcept;
    void  setCustomPoint (int lfo, int point, float value);
    void  resetCustomShapes();

private:
    void updateEngineParams() noexcept;
    dt::TransportInfo readTransport() const;
    void syncCustomShapesToState();
    void loadCustomShapesFromState() { loadCustomShapesFrom (apvts.state); }
    void loadCustomShapesFrom (const juce::ValueTree& root);   // the live tree, or a state being loaded

    juce::AudioProcessorValueTreeState apvts;
    lowkei::StateGuard stateGuard { apvts };   // host save/load off the message thread
    dt::PresetManager presets;
    dt::DeepTranceEngine engine;
    juce::AudioBuffer<float> monoScratch; // right channel for mono layouts

    void applySound (const juce::ValueTree& tree);   // a whole sound, as a sound change
    std::array<juce::ValueTree, 2> abSlots;
    int abActive = 0;

    /** One parameter -> engine field mapping. Built once; read lock-free per block. */
    struct Binding
    {
        std::atomic<float>* value = nullptr;
        void (*apply) (dt::EngineParams&, float, int) = nullptr;
        int index = 0; // step / LFO / slot number for indexed parameters
    };
    std::vector<Binding> bindings;
    std::array<std::array<std::atomic<float>, dt::kCustomPoints>, 2> customShapes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepTranceProcessor)
};
