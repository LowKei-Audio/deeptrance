#pragma once

#include "ParameterIDs.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace dt
{
/** Builds the full APVTS layout with musical defaults. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** "gate_step_01" .. "gate_step_16" for step index 0..15. */
juce::String gateStepId (int index);

/** Parameters shown in the Easy view (Pro shows everything). */
const juce::StringArray& easyParameterIds();
} // namespace dt
