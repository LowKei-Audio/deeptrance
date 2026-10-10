#pragma once

#include "DSP/DeepTranceEngine.h"
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
    double getTailLengthSeconds() const override { return 0.1; } // Phase 2 reverb/delay will raise this

    // Programs (factory presets arrive in Phase 5)
    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    // State
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    /** Easy/Pro is a view choice, saved with the session but not automatable. */
    bool isProMode() const;
    void setProMode (bool pro);

private:
    void updateEngineParams() noexcept;
    dt::TransportInfo readTransport() const;

    juce::AudioProcessorValueTreeState apvts;
    dt::DeepTranceEngine engine;
    juce::AudioBuffer<float> monoScratch; // right channel for mono layouts

    // Cached raw parameter pointers (lock-free reads on the audio thread).
    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float get() const noexcept { return p->load (std::memory_order_relaxed); }
    };
    Raw inputGain, outputGain, mix;
    Raw gateOn, gateRate, gatePattern, gateDepth, gateLength, gateAttack, gateRelease, gateFilter;
    std::array<Raw, 16> gateSteps;
    Raw vowelOn, vowelMorph, vowelLfoRate, vowelLfoDepth, vowelResonance, vowelMix;
    Raw swirlOn, swirlMode, swirlRate, swirlDepth, swirlFeedback, swirlStereo, swirlXover, swirlLow, swirlMix;
    Raw driftOn, driftAmount, driftRate, driftFlutter, driftStereo, driftMix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepTranceProcessor)
};
