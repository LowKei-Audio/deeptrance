#include "ParameterLayout.h"
#include "DSP/Common.h"
#include "DSP/Swirl.h"
#include "DSP/TranceGate.h"
#include "DSP/VowelFilter.h"

namespace dt
{
namespace
{
    using APF   = juce::AudioParameterFloat;
    using APB   = juce::AudioParameterBool;
    using APC   = juce::AudioParameterChoice;
    using Attrs = juce::AudioParameterFloatAttributes;

    juce::ParameterID id (const juce::String& s) { return { s, pid::kParamVersion }; }

    std::unique_ptr<APF> percent (const char* pidStr, const juce::String& name, float def)
    {
        return std::make_unique<APF> (id (pidStr), name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                      Attrs().withLabel ("%").withStringFromValueFunction (
                                          [] (float v, int) { return juce::String (v, 0) + "%"; }));
    }

    std::unique_ptr<APF> ranged (const char* pidStr, const juce::String& name, float lo, float hi, float def,
                                 const juce::String& unit, float skewCentre = 0.0f, int decimals = 1)
    {
        juce::NormalisableRange<float> range (lo, hi, 0.01f);
        if (skewCentre > 0.0f) range.setSkewForCentre (skewCentre);
        return std::make_unique<APF> (id (pidStr), name, range, def,
                                      Attrs().withLabel (unit).withStringFromValueFunction (
                                          [unit, decimals] (float v, int)
                                          {
                                              // "%" hugs the number (matches percent()); other units get a space.
                                              return juce::String (v, decimals) + (unit == "%" ? "" : " ") + unit;
                                          }));
    }

    juce::StringArray syncNames()
    {
        juce::StringArray s;
        for (const auto& d : kSyncDivs) s.add (d.name);
        return s;
    }

    /** Index of a named sync division, so defaults read clearly below. */
    int syncIndex (const char* name)
    {
        return syncNames().indexOf (name);
    }

    using Group = juce::AudioProcessorParameterGroup;
}

juce::String gateStepId (int index)
{
    return juce::String (pid::gateStepPrefix) + juce::String (index + 1).paddedLeft ('0', 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Global ---------------------------------------------------------------
    auto global = std::make_unique<Group> ("global", "Global", "|");
    global->addChild (ranged (pid::inputGain,  "Input",  -24.0f, 24.0f, 0.0f, "dB"),
                      ranged (pid::outputGain, "Output", -24.0f, 24.0f, 0.0f, "dB"),
                      percent (pid::mix, "Mix", 100.0f));

    // --- Trance Gate (off by default: it changes rhythm, so it should be a choice) ---
    juce::StringArray rates;
    for (const auto& r : TranceGate::kRates) rates.add (r.name);
    juce::StringArray patterns { "Custom" };
    for (const auto& p : TranceGate::patterns()) patterns.add (p.name);

    auto gate = std::make_unique<Group> ("gate", "Trance Gate", "|");
    gate->addChild (std::make_unique<APB> (id (pid::gateOn), "Gate On", false),
                    std::make_unique<APC> (id (pid::gateRate), "Gate Rate", rates, 1),
                    std::make_unique<APC> (id (pid::gatePattern), "Gate Pattern", patterns, 1),
                    percent (pid::gateDepth, "Gate Depth", 100.0f),
                    ranged (pid::gateLength,  "Gate Length",   5.0f, 100.0f, 60.0f, "%", 0.0f, 0),
                    ranged (pid::gateAttack,  "Gate Attack",   0.5f,  50.0f,  2.0f, "ms", 5.0f),
                    ranged (pid::gateRelease, "Gate Release",  1.0f, 200.0f, 30.0f, "ms", 30.0f),
                    percent (pid::gateFilter, "Gate Filter", 0.0f));
    for (int i = 0; i < TranceGate::kNumSteps; ++i)
        gate->addChild (percent (gateStepId (i).toRawUTF8(), "Gate Step " + juce::String (i + 1), 100.0f));

    // --- Vowel Filter (off by default: strongly colours the sound) --------------
    auto vowel = std::make_unique<Group> ("vowel", "Vowel Filter", "|");
    vowel->addChild (
        std::make_unique<APB> (id (pid::vowelOn), "Vowel On", false),
        std::make_unique<APF> (id (pid::vowelMorph), "Vowel", juce::NormalisableRange<float> (0.0f, 4.0f, 0.01f), 0.0f,
                               Attrs().withStringFromValueFunction ([] (float v, int)
                               {
                                   const int a = juce::jlimit (0, 3, (int) v);
                                   const float t = v - (float) a;
                                   if (t < 0.05f) return juce::String (VowelFilter::kVowelNames[(size_t) a]);
                                   if (t > 0.95f) return juce::String (VowelFilter::kVowelNames[(size_t) a + 1]);
                                   return juce::String (VowelFilter::kVowelNames[(size_t) a]) + ">"
                                        + VowelFilter::kVowelNames[(size_t) a + 1];
                               })),
        std::make_unique<APC> (id (pid::vowelLfoRate), "Vowel LFO Rate", syncNames(), syncIndex ("1 Bar")),
        percent (pid::vowelLfoDepth, "Vowel LFO Depth", 0.0f),
        ranged (pid::vowelResonance, "Vowel Resonance", 50.0f, 200.0f, 100.0f, "%", 0.0f, 0),
        percent (pid::vowelMix, "Vowel Mix", 100.0f));

    // --- Swirl (on: gentle high-band phaser is the signature default motion) ----
    auto swirl = std::make_unique<Group> ("swirl", "Swirl", "|");
    swirl->addChild (std::make_unique<APB> (id (pid::swirlOn), "Swirl On", true),
                     std::make_unique<APC> (id (pid::swirlMode), "Swirl Mode",
                                            juce::StringArray { "Phaser", "Flanger", "Phaser + Flanger" }, 0),
                     std::make_unique<APC> (id (pid::swirlRate), "Swirl Rate", syncNames(), syncIndex ("2 Bars")),
                     percent (pid::swirlDepth, "Swirl Depth", 40.0f),
                     ranged (pid::swirlFeedback, "Swirl Feedback", 0.0f, 95.0f, 40.0f, "%", 0.0f, 0),
                     ranged (pid::swirlStereo,   "Swirl Stereo",   0.0f, 180.0f, 90.0f, "deg", 0.0f, 0),
                     ranged (pid::swirlXover,    "Swirl Crossover", 60.0f, 1000.0f, 250.0f, "Hz", 250.0f, 0),
                     percent (pid::swirlLow, "Swirl Low Band", 0.0f),
                     percent (pid::swirlMix, "Swirl Mix", 35.0f));

    // --- Drift (on: subtle analog width) -------------------------------------------
    auto drift = std::make_unique<Group> ("drift", "Drift", "|");
    drift->addChild (std::make_unique<APB> (id (pid::driftOn), "Drift On", true),
                     percent (pid::driftAmount, "Drift Amount", 25.0f),
                     ranged (pid::driftRate, "Drift Rate", 0.1f, 3.0f, 0.6f, "Hz", 0.6f, 2),
                     percent (pid::driftFlutter, "Drift Flutter", 15.0f),
                     percent (pid::driftStereo,  "Drift Stereo", 60.0f),
                     percent (pid::driftMix,     "Drift Mix", 35.0f));

    layout.add (std::move (global), std::move (gate), std::move (vowel), std::move (swirl), std::move (drift));
    return layout;
}

const juce::StringArray& easyParameterIds()
{
    // Phase 5 replaces this with the XY pad + macros; until then Easy shows the
    // one or two controls per module that matter most.
    static const juce::StringArray ids {
        pid::gateOn,  pid::gatePattern, pid::gateRate, pid::gateDepth,
        pid::vowelOn, pid::vowelMorph,  pid::vowelLfoDepth,
        pid::swirlOn, pid::swirlMode,   pid::swirlDepth, pid::swirlMix,
        pid::driftOn, pid::driftAmount, pid::driftMix,
        pid::mix,     pid::outputGain };
    return ids;
}
} // namespace dt
