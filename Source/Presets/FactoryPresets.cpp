// DeepTrance factory library. Each preset lists only what differs from the
// defaults, in displayed units (%, ms, Hz, semitones; choices by index).

#include "PresetManager.h"
#include "DSP/DeepTranceEngine.h"
#include "Params/ParameterLayout.h"

namespace dt
{
namespace
{
    using V = std::vector<std::pair<juce::String, float>>;

    // Choice indices, named for readability.
    namespace pat  { constexpr float classic = 1, offbeat = 2, driving = 3, gallop = 4, stutter = 5, accent = 6, half = 7, roll = 8; }
    namespace gr   { constexpr float n8 = 0, n16 = 1, n32 = 2; }
    namespace sync { constexpr float q16 = 0, q8 = 1, q4 = 2, q2 = 3, bar1 = 4, bar2 = 5, bar4 = 6, bar8 = 7; }
    namespace dly  { constexpr float d16 = 2, d16D = 3, d8 = 5, d8D = 6, d4 = 8, d4D = 9; }
    namespace lfo  { constexpr float q8 = 4, q4 = 6, bar1 = 8, bar2 = 9, bar4 = 10, bar8 = 11; }
    namespace shp  { constexpr float sine = 0, tri = 1, sawUp = 2, square = 4, sh = 5, smooth = 6, custom = 7; }
    namespace src  { constexpr float lfo1 = 1, lfo2 = 2, env = 3, gate = 4, pump = 5, rnd = 6; }
    // 1.1
    namespace ext  { constexpr float dotted = 1, tresillo = 2, psyRoll = 3, uplift = 4, synco = 5, eighths = 6, echo = 7, swell = 8; }
    namespace pr   { constexpr float e8 = 0, q4 = 1, h2 = 2, bar = 3; }
    namespace band { constexpr float full = 0, lows = 1, highs = 2; }

    float dest (ModDest d) { return (float) (int) d; }

    /** Adds one mod-matrix slot. */
    void slot (V& v, int s, float source, ModDest d, float amountPct)
    {
        v.push_back ({ modId (s, "source"), source });
        v.push_back ({ modId (s, "dest"),   dest (d) });
        v.push_back ({ modId (s, "amount"), amountPct });
    }

    FactoryPreset make (const char* category, const char* name, V values, std::vector<float> custom = {})
    {
        return { category, name, std::move (values), std::move (custom) };
    }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        namespace P = pid;
        std::vector<FactoryPreset> list;

        list.push_back (make ("", "Init", {}));

        // --- Pads ------------------------------------------------------------------
        list.push_back (make ("Pads", "Euphoric Wash", {
            { P::swirlDepth, 55 }, { P::swirlMix, 45 }, { P::driftAmount, 35 }, { P::driftMix, 40 },
            { P::abyssDecay, 10 }, { P::abyssSize, 85 }, { P::abyssMix, 40 }, { P::abyssShimmer, 25 },
            { P::delayMix, 15 } }));

        list.push_back (make ("Pads", "Gated Supersaw", {
            { P::gateOn, 1 }, { P::gatePattern, pat::driving }, { P::gateRate, gr::n16 }, { P::gateFilter, 35 },
            { P::gateLength, 70 }, { P::delayMix, 15 }, { P::abyssMix, 30 } }));

        list.push_back (make ("Pads", "Talking Pad", {
            { P::vowelOn, 1 }, { P::vowelMorph, 1 }, { P::vowelLfoDepth, 70 }, { P::vowelLfoRate, sync::bar2 },
            { P::vowelMix, 80 }, { P::abyssMix, 30 } }));

        list.push_back (make ("Pads", "Frozen Horizon", {
            { P::cloudOn, 1 }, { P::cloudSize, 220 }, { P::cloudDensity, 50 }, { P::cloudSpray, 50 },
            { P::cloudJitter, 25 }, { P::cloudShimmer, 30 }, { P::cloudMix, 60 },
            { P::abyssDecay, 20 }, { P::abyssShimmer, 40 }, { P::abyssMix, 45 }, { P::abyssSize, 100 } }));

        // --- Plucks -------------------------------------------------------------------
        list.push_back (make ("Plucks", "Classic 3/16 Echo", {
            { P::delayTimeSync, dly::d8D }, { P::delayFeedback, 50 }, { P::delayMix, 30 }, { P::delayTone, 5000 },
            { P::abyssMix, 20 }, { P::punchOn, 1 }, { P::punchAttack, 40 }, { P::punchDuck, 40 } }));

        list.push_back (make ("Plucks", "Crystal Ascend", {
            { P::delayPitch, 12 }, { P::delayFeedback, 55 }, { P::delayMix, 30 }, { P::delayTone, 9000 },
            { P::abyssShimmer, 30 }, { P::abyssMix, 25 } }));

        list.push_back (make ("Plucks", "Murky Descent", {
            { P::delayPitch, -12 }, { P::delayFeedback, 60 }, { P::delayTone, 2500 }, { P::delayMix, 30 },
            { P::abyssDamping, 70 }, { P::abyssMix, 30 } }));

        list.push_back (make ("Plucks", "Psy Punch", {
            { P::punchOn, 1 }, { P::punchAttack, 80 }, { P::punchSustain, -30 }, { P::punchDuck, 80 },
            { P::punchDuckRel, 60 }, { P::abyssDecay, 4 }, { P::abyssMix, 35 },
            { P::delayTimeSync, dly::d16 }, { P::delayFeedback, 35 }, { P::delayMix, 20 } }));

        // --- Leads --------------------------------------------------------------------
        list.push_back (make ("Leads", "Acid Squelch", {
            { P::acidOn, 1 }, { P::acidCutoff, 450 }, { P::acidResonance, 85 }, { P::acidEnvMod, 80 },
            { P::acidDecay, 160 }, { P::acidDrive, 60 },
            { P::delayTimeSync, dly::d16 }, { P::delayFeedback, 40 }, { P::delayMix, 25 } }));

        list.push_back (make ("Leads", "Vowel Lead", {
            { P::vowelOn, 1 }, { P::vowelMorph, 0 }, { P::vowelLfoDepth, 100 }, { P::vowelLfoRate, sync::q4 },
            { P::vowelResonance, 140 }, { P::vowelMix, 70 }, { P::delayMix, 25 } }));

        list.push_back (make ("Leads", "90s Anthem", {
            { P::driftAmount, 50 }, { P::driftFlutter, 25 }, { P::driftMix, 50 }, { P::swirlDepth, 40 },
            { P::delayFeedback, 45 }, { P::delayMix, 25 }, { P::abyssDecay, 5 }, { P::abyssMix, 25 } }));

        list.push_back (make ("Leads", "Flange Rider", {
            { P::swirlMode, 2 }, { P::swirlDepth, 70 }, { P::swirlFeedback, 75 }, { P::swirlRate, sync::bar1 },
            { P::swirlMix, 55 } }));

        // --- Vocals -------------------------------------------------------------------
        list.push_back (make ("Vocals", "Breathe Cloud", {
            { P::cloudOn, 1 }, { P::cloudSize, 300 }, { P::cloudDensity, 35 }, { P::cloudPosition, 500 },
            { P::cloudSpray, 60 }, { P::cloudJitter, 15 }, { P::cloudMix, 55 },
            { P::abyssDecay, 12 }, { P::abyssMix, 35 } }));

        list.push_back (make ("Vocals", "Angel Shimmer", {
            { P::abyssShimmer, 60 }, { P::abyssDecay, 15 }, { P::abyssSize, 100 }, { P::abyssMix, 40 },
            { P::abyssPreDelay, 60 }, { P::delayMix, 15 } }));

        list.push_back (make ("Vocals", "Stutter Chop", {
            { P::cloudOn, 1 }, { P::cloudSize, 40 }, { P::cloudDensity, 8 }, { P::cloudSpray, 10 }, { P::cloudMix, 60 },
            { P::gateOn, 1 }, { P::gatePattern, pat::stutter }, { P::gateRate, gr::n16 } }));

        // --- Build-Ups (automate the Build-Up knob over 8-16 bars) ---------------------
        list.push_back (make ("Build-Ups", "8-Bar Riser", {
            { P::buildUpNoise, 70 }, { P::abyssMix, 30 }, { P::delayMix, 20 } }));

        list.push_back (make ("Build-Ups", "Tension Gate", {
            { P::gateOn, 1 }, { P::gatePattern, pat::roll }, { P::gateRate, gr::n16 }, { P::gateFilter, 50 },
            { P::buildUpNoise, 60 } }));

        {
            // Grains climb in pitch over 8 bars while the cloud scatters.
            V v { { P::cloudOn, 1 }, { P::cloudJitter, 70 }, { P::cloudShimmer, 50 }, { P::cloudDensity, 80 },
                  { P::cloudSize, 120 }, { P::cloudMix, 50 }, { P::abyssMix, 35 },
                  { lfoId (0, "shape"), shp::sawUp }, { lfoId (0, "rate_sync"), lfo::bar8 } };
            slot (v, 0, src::lfo1, ModDest::CloudPitch, 50);
            list.push_back (make ("Build-Ups", "Infinite Riser", v));
        }

        // --- FX -------------------------------------------------------------------------
        list.push_back (make ("FX", "Astral Projection", { { P::combo, 1 } }));

        list.push_back (make ("FX", "Acid Bath", {
            { P::combo, 2 }, { P::gateOn, 1 }, { P::gatePattern, pat::driving } }));

        list.push_back (make ("FX", "Frozen Abyss (hit Freeze)", {
            { P::abyssDecay, 30 }, { P::abyssSize, 100 }, { P::abyssShimmer, 50 }, { P::abyssMix, 60 },
            { P::abyssMod, 70 } }));

        list.push_back (make ("FX", "Glitch Tops", {
            { P::cloudOn, 1 }, { P::cloudSize, 25 }, { P::cloudDensity, 12 }, { P::cloudSpray, 80 },
            { P::cloudJitter, 40 }, { P::cloudMix, 70 }, { P::gateOn, 1 }, { P::gatePattern, pat::stutter } }));

        {
            V v { { P::delayMix, 25 }, { P::delayFeedback, 50 },
                  { lfoId (0, "rate_sync"), lfo::q4 }, { lfoId (1, "shape"), shp::sh }, { lfoId (1, "rate_sync"), lfo::q8 } };
            slot (v, 0, src::lfo1, ModDest::AbyssSize, 30);
            slot (v, 1, src::lfo2, ModDest::DelayPitch, 20);
            list.push_back (make ("FX", "Wobble Space", v));
        }

        {
            V v { { P::swirlMix, 60 }, { P::swirlDepth, 30 }, { P::envGain, 6 } };
            slot (v, 0, src::env, ModDest::SwirlDepth, 60);
            list.push_back (make ("FX", "Breathing Swirl", v));
        }

        {
            // Drawn LFO: a stepped "trance pump" shape on the global mix.
            V v { { lfoId (0, "shape"), shp::custom }, { lfoId (0, "rate_sync"), lfo::q4 },
                  { P::abyssMix, 40 }, { P::delayMix, 25 } };
            slot (v, 0, src::lfo1, ModDest::AbyssMix, 40);
            list.push_back (make ("FX", "Pumping Space (drawn LFO)", v,
                                  { 0.0f, 0.1f, 0.3f, 0.5f, 0.65f, 0.75f, 0.85f, 0.9f, 0.95f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f, 0.2f }));
        }

        // ===== 1.1 "Euphoria" =====================================================
        // Appended, never inserted: hosts list factory presets as numbered programs.

        // --- Uplifting -------------------------------------------------------------
        list.push_back (make ("Uplifting", "Sidechain Supersaw", {
            { P::pumpOn, 1 }, { P::pumpRate, pr::q4 }, { P::pumpDepth, 65 }, { P::pumpShape, 55 },
            { P::abyssDecay, 8 }, { P::abyssMix, 30 }, { P::delayMix, 18 }, { P::outBassMono, 120 } }));

        list.push_back (make ("Uplifting", "Euphoria Pump Pad", {
            { P::pumpOn, 1 }, { P::pumpBand, band::lows }, { P::pumpDepth, 80 }, { P::pumpShape, 60 },
            { P::abyssSize, 90 }, { P::abyssDecay, 12 }, { P::abyssMix, 40 }, { P::abyssShimmer, 20 },
            { P::driftAmount, 35 }, { P::outWidth, 130 } }));

        list.push_back (make ("Uplifting", "Ping-Pong Gate", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::uplift }, { P::gateStereo, 100 }, { P::gateLength, 65 },
            { P::delayMix, 15 }, { P::pumpOn, 1 }, { P::pumpDepth, 40 } }));

        list.push_back (make ("Uplifting", "Breakdown Wash", {
            { P::abyssDecay, 20 }, { P::abyssSize, 95 }, { P::abyssMix, 45 }, { P::abyssShimmer, 35 },
            { P::cloudOn, 1 }, { P::cloudMix, 30 }, { P::cloudShimmer, 25 }, { P::riserBars, 2 } }));

        // --- Psy -------------------------------------------------------------------
        list.push_back (make ("Psy", "Rolling Bass Gate", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::psyRoll }, { P::gateLength, 55 }, { P::gateRelease, 15 },
            { P::punchOn, 1 }, { P::punchAttack, 50 }, { P::delayOn, 0 }, { P::abyssMix, 10 },
            { P::outBassMono, 150 } }));

        list.push_back (make ("Psy", "Acid Triplets", {
            { P::acidOn, 1 }, { P::acidCutoff, 700 }, { P::acidResonance, 80 }, { P::acidEnvMod, 70 }, { P::acidDrive, 50 },
            { P::gateOn, 1 }, { P::gatePatternExt, ext::tresillo }, { P::delayTimeSync, dly::d16 },
            { P::delayFeedback, 45 }, { P::delayMix, 20 } }));

        {
            V v { { P::acidOn, 1 }, { P::acidCutoff, 1100 }, { P::acidResonance, 75 }, { P::gateOn, 1 },
                  { P::gatePattern, pat::classic }, { P::delayMix, 15 } };
            slot (v, 0, src::rnd, ModDest::AcidCutoff, 45);
            list.push_back (make ("Psy", "Step Random Filter", v));
        }

        // --- Progressive -------------------------------------------------------------
        list.push_back (make ("Progressive", "Swung Pluck Gate", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::synco }, { P::gateSwing, 35 }, { P::gateLength, 50 },
            { P::delayMix, 22 }, { P::delayFeedback, 50 }, { P::abyssMix, 25 } }));

        list.push_back (make ("Progressive", "Deep Pump Chords", {
            { P::pumpOn, 1 }, { P::pumpRate, pr::h2 }, { P::pumpDepth, 50 }, { P::pumpShape, 70 },
            { P::swirlDepth, 45 }, { P::abyssDecay, 7 }, { P::abyssMix, 30 }, { P::outWidth, 120 } }));

        list.push_back (make ("Progressive", "Dotted Echo Gate", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::dotted }, { P::gateLength, 70 }, { P::gateStereo, 50 },
            { P::delayMix, 25 } }));

        // --- Vocals (more) -------------------------------------------------------------
        list.push_back (make ("Vocals", "Throw-Ready Vocal (hold THROW)", {
            { P::delayTimeSync, dly::d4 }, { P::delayMix, 12 }, { P::abyssMix, 18 }, { P::abyssDecay, 5 },
            { P::swirlMix, 20 } }));

        list.push_back (make ("Vocals", "Wide Pump Vox", {
            { P::pumpOn, 1 }, { P::pumpDepth, 45 }, { P::pumpShape, 40 }, { P::outWidth, 140 },
            { P::abyssMix, 30 }, { P::delayMix, 15 } }));

        // --- Tech --------------------------------------------------------------------
        list.push_back (make ("Tech", "Hard Eighths", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::eighths }, { P::gateFilter, 40 }, { P::punchOn, 1 },
            { P::pumpOn, 1 }, { P::pumpBand, band::lows }, { P::pumpDepth, 55 }, { P::abyssMix, 15 } }));

        list.push_back (make ("Tech", "Stereo Stab Gate", {
            { P::gateOn, 1 }, { P::gatePatternExt, ext::synco }, { P::gateStereo, 100 }, { P::gateSwing, 20 },
            { P::delayTimeSync, dly::d16D }, { P::delayMix, 15 } }));

        // --- Build-Ups (more) --------------------------------------------------------------
        list.push_back (make ("Build-Ups", "16-Bar Swell (hold RISER)", {
            { P::riserBars, 2 }, { P::gateOn, 1 }, { P::gatePatternExt, ext::swell }, { P::abyssMix, 30 },
            { P::swirlDepth, 60 } }));

        {
            // A 4-bar saw opens the gate further each bar while the pump keeps breathing.
            V v { { P::gateOn, 1 }, { P::gatePatternExt, ext::echo }, { P::gateLength, 60 }, { P::gateDepth, 85 },
                  { P::pumpOn, 1 }, { P::pumpDepth, 35 },
                  { lfoId (0, "shape"), shp::sawUp }, { lfoId (0, "rate_sync"), lfo::bar4 } };
            slot (v, 0, src::lfo1, ModDest::GateLength, 35);
            list.push_back (make ("Build-Ups", "Gate Pump Riser", v));
        }

        return list;
    }();
    return presets;
}
} // namespace dt
