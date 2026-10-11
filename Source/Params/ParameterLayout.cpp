#include "ParameterLayout.h"
#include "DSP/DeepTranceEngine.h"

namespace dt
{
namespace
{
    using APF   = juce::AudioParameterFloat;
    using APB   = juce::AudioParameterBool;
    using APC   = juce::AudioParameterChoice;
    using Attrs = juce::AudioParameterFloatAttributes;
    using Group = juce::AudioProcessorParameterGroup;

    juce::ParameterID id (const juce::String& s) { return { s, pid::kParamVersion }; }

    /** Number text. JUCE treats 0 decimal places as "default precision", so round explicitly. */
    juce::String num (float v, int decimals) { return decimals <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals); }

    std::unique_ptr<APF> percent (const juce::String& pidStr, const juce::String& name, float def)
    {
        return std::make_unique<APF> (id (pidStr), name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                      Attrs().withLabel ("%").withStringFromValueFunction (
                                          [] (float v, int) { return num (v, 0) + "%"; }));
    }

    /** -100..+100 %, shown with a sign. */
    std::unique_ptr<APF> bipolar (const juce::String& pidStr, const juce::String& name, float def)
    {
        return std::make_unique<APF> (id (pidStr), name, juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), def,
                                      Attrs().withLabel ("%").withStringFromValueFunction (
                                          [] (float v, int) { return (v >= 0.5f ? "+" : "") + num (v, 0) + "%"; }));
    }

    std::unique_ptr<APF> ranged (const juce::String& pidStr, const juce::String& name, float lo, float hi, float def,
                                 const juce::String& unit, float skewCentre = 0.0f, int decimals = 1)
    {
        juce::NormalisableRange<float> range (lo, hi, 0.01f);
        if (skewCentre > 0.0f) range.setSkewForCentre (skewCentre);
        return std::make_unique<APF> (id (pidStr), name, range, def,
                                      Attrs().withLabel (unit).withStringFromValueFunction (
                                          [unit, decimals] (float v, int)
                                          {
                                              // "%" hugs the number (matches percent()); other units get a space.
                                              if (unit == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                                              return num (v, decimals) + (unit == "%" ? "" : " ") + unit;
                                          }));
    }

    std::unique_ptr<APB> toggle (const juce::String& pidStr, const juce::String& name, bool def)
    {
        return std::make_unique<APB> (id (pidStr), name, def);
    }

    template <size_t N>
    juce::StringArray names (const std::array<SyncDiv, N>& divs)
    {
        juce::StringArray s;
        for (const auto& d : divs) s.add (d.name);
        return s;
    }

    template <typename Array>
    juce::StringArray names (const Array& arr)
    {
        juce::StringArray s;
        for (const char* n : arr) s.add (n);
        return s;
    }

    std::unique_ptr<APC> choice (const juce::String& pidStr, const juce::String& name, const juce::StringArray& items, const juce::String& def)
    {
        return std::make_unique<APC> (id (pidStr), name, items, juce::jmax (0, items.indexOf (def)));
    }
}

juce::String gateStepId (int index) { return juce::String (pid::gateStepPrefix) + juce::String (index + 1).paddedLeft ('0', 2); }
juce::String lfoId (int index, const char* suffix) { return "lfo" + juce::String (index + 1) + "_" + suffix; }
juce::String modId (int slot, const char* suffix)  { return "mod" + juce::String (slot + 1) + "_" + suffix; }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Global -------------------------------------------------------------------
    auto global = std::make_unique<Group> (group::global, "Global", "|");
    global->addChild (ranged (pid::inputGain,  "Input",  -24.0f, 24.0f, 0.0f, "dB"),
                      ranged (pid::outputGain, "Output", -24.0f, 24.0f, 0.0f, "dB"),
                      percent (pid::mix, "Mix", 100.0f));

    // --- Performance: Easy-view macros, Build-Up, combos -----------------------------
    auto perf = std::make_unique<Group> (group::performance, "Performance", "|");
    perf->addChild (percent (pid::macroGate,  "Gate Macro",  50.0f),
                    percent (pid::macroSwirl, "Swirl Macro", 50.0f),
                    percent (pid::macroSpace, "Space Macro", 50.0f),
                    percent (pid::macroDrift, "Drift Macro", 50.0f),
                    percent (pid::buildUp, "Build-Up", 0.0f),
                    percent (pid::buildUpNoise, "Build-Up Noise", 50.0f),
                    choice (pid::combo, "Combo", names (kComboNames), "Off"),
                    percent (pid::comboAmount, "Combo Amount", 100.0f));

    // --- Trance Gate (off by default: it changes rhythm, so it should be a choice) ---
    juce::StringArray rates;
    for (const auto& r : TranceGate::kRates) rates.add (r.name);
    juce::StringArray patterns { "Custom" };
    for (const auto& p : TranceGate::patterns()) patterns.add (p.name);

    auto gate = std::make_unique<Group> ("gate", "Trance Gate", "|");
    gate->addChild (toggle (pid::gateOn, "Gate On", false),
                    choice (pid::gateRate, "Gate Rate", rates, "1/16"),
                    choice (pid::gatePattern, "Gate Pattern", patterns, "Classic 16ths"),
                    percent (pid::gateDepth, "Gate Depth", 100.0f),
                    ranged (pid::gateLength,  "Gate Length",   5.0f, 100.0f, 60.0f, "%", 0.0f, 0),
                    ranged (pid::gateAttack,  "Gate Attack",   0.5f,  50.0f,  2.0f, "ms", 5.0f),
                    ranged (pid::gateRelease, "Gate Release",  1.0f, 200.0f, 30.0f, "ms", 30.0f),
                    percent (pid::gateFilter, "Gate Filter", 0.0f));
    for (int i = 0; i < TranceGate::kNumSteps; ++i)
        gate->addChild (percent (gateStepId (i), "Gate Step " + juce::String (i + 1), 100.0f));

    // --- Vowel Filter (off by default: strongly colours the sound) --------------------
    auto vowel = std::make_unique<Group> ("vowel", "Vowel Filter", "|");
    vowel->addChild (
        toggle (pid::vowelOn, "Vowel On", false),
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
        choice (pid::vowelLfoRate, "Vowel LFO Rate", names (kSyncDivs), "1 Bar"),
        percent (pid::vowelLfoDepth, "Vowel LFO Depth", 0.0f),
        ranged (pid::vowelResonance, "Vowel Resonance", 50.0f, 200.0f, 100.0f, "%", 0.0f, 0),
        percent (pid::vowelMix, "Vowel Mix", 100.0f));

    // --- Swirl (on: gentle high-band phaser is the signature default motion) ----------
    auto swirl = std::make_unique<Group> ("swirl", "Swirl", "|");
    swirl->addChild (toggle (pid::swirlOn, "Swirl On", true),
                     choice (pid::swirlMode, "Swirl Mode", { "Phaser", "Flanger", "Phaser + Flanger" }, "Phaser"),
                     choice (pid::swirlRate, "Swirl Rate", names (kSyncDivs), "2 Bars"),
                     percent (pid::swirlDepth, "Swirl Depth", 40.0f),
                     ranged (pid::swirlFeedback, "Swirl Feedback", 0.0f, 95.0f, 40.0f, "%", 0.0f, 0),
                     ranged (pid::swirlStereo,   "Swirl Stereo",   0.0f, 180.0f, 90.0f, "deg", 0.0f, 0),
                     ranged (pid::swirlXover,    "Swirl Crossover", 60.0f, 1000.0f, 250.0f, "Hz", 250.0f, 0),
                     percent (pid::swirlLow, "Swirl Low Band", 0.0f),
                     percent (pid::swirlMix, "Swirl Mix", 35.0f));

    // --- Drift (on: subtle analog width) ----------------------------------------------
    auto drift = std::make_unique<Group> ("drift", "Drift", "|");
    drift->addChild (toggle (pid::driftOn, "Drift On", true),
                     percent (pid::driftAmount, "Drift Amount", 25.0f),
                     ranged (pid::driftRate, "Drift Rate", 0.1f, 3.0f, 0.6f, "Hz", 0.6f, 2),
                     percent (pid::driftFlutter, "Drift Flutter", 15.0f),
                     percent (pid::driftStereo,  "Drift Stereo", 60.0f),
                     percent (pid::driftMix,     "Drift Mix", 35.0f));

    // --- Acid (off: Acid Bath combo switches it on) -------------------------------------
    auto acid = std::make_unique<Group> ("acid", "Acid", "|");
    acid->addChild (toggle (pid::acidOn, "Acid On", false),
                    ranged (pid::acidCutoff, "Acid Cutoff", 60.0f, 8000.0f, 800.0f, "Hz", 800.0f, 0),
                    percent (pid::acidResonance, "Acid Resonance", 70.0f),
                    percent (pid::acidEnvMod, "Acid Env Mod", 50.0f),
                    ranged (pid::acidDecay, "Acid Decay", 20.0f, 1000.0f, 200.0f, "ms", 200.0f, 0),
                    percent (pid::acidDrive, "Acid Drive", 40.0f),
                    percent (pid::acidMix, "Acid Mix", 100.0f));

    // --- Punch (off: shapes the dry hits and ducks every tail after it) -----------------
    auto punch = std::make_unique<Group> ("punch", "Punch", "|");
    punch->addChild (toggle (pid::punchOn, "Punch On", false),
                     bipolar (pid::punchAttack, "Punch Attack", 40.0f),
                     bipolar (pid::punchSustain, "Punch Sustain", 0.0f),
                     percent (pid::punchDuck, "Punch Duck", 50.0f),
                     ranged (pid::punchDuckRel, "Punch Duck Release", 20.0f, 500.0f, 80.0f, "ms", 80.0f, 0));

    // --- Cloud (off: a texture tool you reach for) ---------------------------------------
    auto cloud = std::make_unique<Group> ("cloud", "Cloud", "|");
    cloud->addChild (toggle (pid::cloudOn, "Cloud On", false),
                     ranged (pid::cloudSize, "Cloud Size", 5.0f, 500.0f, 80.0f, "ms", 80.0f, 0),
                     ranged (pid::cloudDensity, "Cloud Density", 1.0f, 200.0f, 25.0f, "/s", 25.0f, 0),
                     ranged (pid::cloudPosition, "Cloud Position", 0.0f, 2000.0f, 150.0f, "ms", 300.0f, 0),
                     percent (pid::cloudSpray, "Cloud Spray", 20.0f),
                     ranged (pid::cloudPitch, "Cloud Pitch", -24.0f, 24.0f, 0.0f, "st", 0.0f, 1),
                     percent (pid::cloudJitter, "Cloud Jitter", 10.0f),
                     percent (pid::cloudShimmer, "Cloud Shimmer", 0.0f),
                     percent (pid::cloudStereo, "Cloud Stereo", 70.0f),
                     percent (pid::cloudMix, "Cloud Mix", 50.0f),
                     toggle (pid::cloudFreeze, "Cloud Freeze", false));

    // --- Fractal Delay (on: a dotted-eighth ping-pong is the trance default) -------------
    auto delay = std::make_unique<Group> ("delay", "Fractal Delay", "|");
    delay->addChild (toggle (pid::delayOn, "Delay On", true),
                     toggle (pid::delaySync, "Delay Sync", true),
                     choice (pid::delayTimeSync, "Delay Time", names (kDelayDivs), "1/8D"),
                     ranged (pid::delayTimeMs, "Delay Time (ms)", 10.0f, 2000.0f, 375.0f, "ms", 375.0f, 0),
                     ranged (pid::delayFeedback, "Delay Feedback", 0.0f, 95.0f, 45.0f, "%", 0.0f, 0),
                     ranged (pid::delayPitch, "Delay Pitch", -12.0f, 12.0f, 0.0f, "st", 0.0f, 1),
                     ranged (pid::delayTone, "Delay Tone", 500.0f, 18000.0f, 6000.0f, "Hz", 4000.0f, 0),
                     percent (pid::delayPingPong, "Delay Ping-Pong", 100.0f),
                     percent (pid::delayMix, "Delay Mix", 20.0f));

    // --- Abyss (on: medium space by default) ---------------------------------------------
    auto abyss = std::make_unique<Group> ("abyss", "Abyss Reverb", "|");
    abyss->addChild (toggle (pid::abyssOn, "Abyss On", true),
                     ranged (pid::abyssDecay, "Abyss Decay", 0.5f, 60.0f, 6.0f, "s", 6.0f, 1),
                     percent (pid::abyssSize, "Abyss Size", 60.0f),
                     percent (pid::abyssDamping, "Abyss Damping", 45.0f),
                     ranged (pid::abyssPreDelay, "Abyss Pre-Delay", 0.0f, 250.0f, 20.0f, "ms", 40.0f, 0),
                     percent (pid::abyssMod, "Abyss Modulation", 40.0f),
                     percent (pid::abyssShimmer, "Abyss Shimmer", 0.0f),
                     percent (pid::abyssWidth, "Abyss Width", 100.0f),
                     percent (pid::abyssMix, "Abyss Mix", 25.0f),
                     toggle (pid::abyssFreeze, "Abyss Freeze", false));

    // --- LFOs -----------------------------------------------------------------------------
    const char* lfoGroupIds[2] = { group::lfo1, group::lfo2 };
    const char* lfoDefaultRate[2] = { "1 Bar", "4 Bars" };
    const float lfoDefaultHz[2] = { 1.0f, 0.2f };
    std::unique_ptr<Group> lfos[2];
    for (int i = 0; i < 2; ++i)
    {
        const juce::String n = "LFO " + juce::String (i + 1);
        lfos[i] = std::make_unique<Group> (lfoGroupIds[i], n, "|");
        lfos[i]->addChild (choice (lfoId (i, "shape"), n + " Shape", names (kLfoShapeNames), "Sine"),
                           toggle (lfoId (i, "sync"), n + " Sync", true),
                           choice (lfoId (i, "rate_sync"), n + " Rate", names (kLfoDivs), lfoDefaultRate[i]),
                           ranged (lfoId (i, "rate_hz"), n + " Rate (Hz)", 0.01f, 20.0f, lfoDefaultHz[i], "Hz", 1.0f, 2));
    }

    auto env = std::make_unique<Group> (group::env, "Envelope Follower", "|");
    env->addChild (ranged (pid::envAttack, "Env Attack", 0.1f, 500.0f, 10.0f, "ms", 10.0f, 1),
                   ranged (pid::envRelease, "Env Release", 5.0f, 5000.0f, 150.0f, "ms", 150.0f, 0),
                   ranged (pid::envGain, "Env Sensitivity", -24.0f, 24.0f, 0.0f, "dB"));

    // --- Mod matrix ---------------------------------------------------------------------
    auto mod = std::make_unique<Group> (group::mod, "Mod Matrix", "|");
    for (int s = 0; s < kModSlots; ++s)
    {
        const juce::String n = "Mod " + juce::String (s + 1);
        mod->addChild (choice (modId (s, "source"), n + " Source", names (kModSourceNames), "None"),
                       choice (modId (s, "dest"), n + " Destination", names (modDestinationNames()), "None"),
                       bipolar (modId (s, "amount"), n + " Amount", 0.0f));
    }

    layout.add (std::move (global), std::move (perf), std::move (gate), std::move (vowel), std::move (swirl),
                std::move (drift), std::move (acid), std::move (punch), std::move (cloud), std::move (delay),
                std::move (abyss), std::move (lfos[0]), std::move (lfos[1]), std::move (env), std::move (mod));
    return layout;
}
} // namespace dt
