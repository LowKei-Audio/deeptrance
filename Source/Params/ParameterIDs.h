#pragma once
// Stable parameter IDs. NEVER rename or reuse an ID once released: DAW
// automation and saved presets reference these strings. New parameters in
// later phases get new IDs and bump kParamVersion for their ParameterID.

namespace dt::pid
{
inline constexpr int kParamVersion = 1;

// Global
inline constexpr const char* inputGain  = "input_gain";
inline constexpr const char* outputGain = "output_gain";
inline constexpr const char* mix        = "mix";

// Trance Gate
inline constexpr const char* gateOn      = "gate_on";
inline constexpr const char* gateRate    = "gate_rate";
inline constexpr const char* gatePattern = "gate_pattern";
inline constexpr const char* gateDepth   = "gate_depth";
inline constexpr const char* gateLength  = "gate_length";
inline constexpr const char* gateAttack  = "gate_attack";
inline constexpr const char* gateRelease = "gate_release";
inline constexpr const char* gateFilter  = "gate_filter";
inline constexpr const char* gateStepPrefix = "gate_step_"; // gate_step_01 .. gate_step_16

// Vowel Filter
inline constexpr const char* vowelOn        = "vowel_on";
inline constexpr const char* vowelMorph     = "vowel_morph";
inline constexpr const char* vowelLfoRate   = "vowel_lfo_rate";
inline constexpr const char* vowelLfoDepth  = "vowel_lfo_depth";
inline constexpr const char* vowelResonance = "vowel_resonance";
inline constexpr const char* vowelMix       = "vowel_mix";

// Swirl
inline constexpr const char* swirlOn       = "swirl_on";
inline constexpr const char* swirlMode     = "swirl_mode";
inline constexpr const char* swirlRate     = "swirl_rate";
inline constexpr const char* swirlDepth    = "swirl_depth";
inline constexpr const char* swirlFeedback = "swirl_feedback";
inline constexpr const char* swirlStereo   = "swirl_stereo";
inline constexpr const char* swirlXover    = "swirl_xover";
inline constexpr const char* swirlLow      = "swirl_low";
inline constexpr const char* swirlMix      = "swirl_mix";

// Drift
inline constexpr const char* driftOn      = "drift_on";
inline constexpr const char* driftAmount  = "drift_amount";
inline constexpr const char* driftRate    = "drift_rate";
inline constexpr const char* driftFlutter = "drift_flutter";
inline constexpr const char* driftStereo  = "drift_stereo";
inline constexpr const char* driftMix     = "drift_mix";
} // namespace dt::pid
