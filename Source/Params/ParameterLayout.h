#pragma once

#include "ParameterIDs.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace dt
{
/** Builds the full APVTS layout with musical defaults. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** "gate_step_01" .. "gate_step_16" for step index 0..15. */
juce::String gateStepId (int index);

/** "lfo1_shape", "lfo2_rate_hz" ... for LFO index 0..1. Suffixes: shape, sync, rate_sync, rate_hz. */
juce::String lfoId (int index, const char* suffix);

/** "mod1_source" .. "mod8_amount" for slot 0..7. Suffixes: source, dest, amount. */
juce::String modId (int slot, const char* suffix);

/** Parameter group IDs, in display order. */
namespace group
{
    inline constexpr const char* global = "global";
    inline constexpr const char* performance = "performance";
    inline constexpr const char* lfo1 = "lfo1";
    inline constexpr const char* lfo2 = "lfo2";
    inline constexpr const char* env  = "env";
    inline constexpr const char* mod  = "mod";
}
} // namespace dt
