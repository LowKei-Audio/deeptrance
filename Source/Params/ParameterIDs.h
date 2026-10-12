#pragma once
// Stable parameter IDs. NEVER rename or reuse an ID once released: DAW
// automation and saved presets reference these strings. New parameters get
// new IDs; choice lists may only be appended to.

namespace dt::pid
{
inline constexpr int kParamVersion = 1;

// Global
inline constexpr const char* inputGain  = "input_gain";
inline constexpr const char* outputGain = "output_gain";
inline constexpr const char* mix        = "mix";

// Performance (Easy view)
inline constexpr const char* macroGate    = "macro_gate";
inline constexpr const char* macroSwirl   = "macro_swirl";
inline constexpr const char* macroSpace   = "macro_space";
inline constexpr const char* macroDrift   = "macro_drift";
inline constexpr const char* buildUp      = "buildup";
inline constexpr const char* buildUpNoise = "buildup_noise";
inline constexpr const char* combo        = "combo";
inline constexpr const char* comboAmount  = "combo_amount";

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

// Acid
inline constexpr const char* acidOn        = "acid_on";
inline constexpr const char* acidCutoff    = "acid_cutoff";
inline constexpr const char* acidResonance = "acid_resonance";
inline constexpr const char* acidEnvMod    = "acid_env_mod";
inline constexpr const char* acidDecay     = "acid_decay";
inline constexpr const char* acidDrive     = "acid_drive";
inline constexpr const char* acidMix       = "acid_mix";

// Punch
inline constexpr const char* punchOn      = "punch_on";
inline constexpr const char* punchAttack  = "punch_attack";
inline constexpr const char* punchSustain = "punch_sustain";
inline constexpr const char* punchDuck    = "punch_duck";
inline constexpr const char* punchDuckRel = "punch_duck_release";

// Cloud
inline constexpr const char* cloudOn       = "cloud_on";
inline constexpr const char* cloudSize     = "cloud_size";
inline constexpr const char* cloudDensity  = "cloud_density";
inline constexpr const char* cloudPosition = "cloud_position";
inline constexpr const char* cloudSpray    = "cloud_spray";
inline constexpr const char* cloudPitch    = "cloud_pitch";
inline constexpr const char* cloudJitter   = "cloud_jitter";
inline constexpr const char* cloudShimmer  = "cloud_shimmer";
inline constexpr const char* cloudStereo   = "cloud_stereo";
inline constexpr const char* cloudMix      = "cloud_mix";
inline constexpr const char* cloudFreeze   = "cloud_freeze";

// Fractal Delay
inline constexpr const char* delayOn       = "delay_on";
inline constexpr const char* delaySync     = "delay_sync";
inline constexpr const char* delayTimeSync = "delay_time_sync";
inline constexpr const char* delayTimeMs   = "delay_time_ms";
inline constexpr const char* delayFeedback = "delay_feedback";
inline constexpr const char* delayPitch    = "delay_pitch";
inline constexpr const char* delayTone     = "delay_tone";
inline constexpr const char* delayPingPong = "delay_pingpong";
inline constexpr const char* delayMix      = "delay_mix";

// Abyss
inline constexpr const char* abyssOn       = "abyss_on";
inline constexpr const char* abyssDecay    = "abyss_decay";
inline constexpr const char* abyssSize     = "abyss_size";
inline constexpr const char* abyssDamping  = "abyss_damping";
inline constexpr const char* abyssPreDelay = "abyss_predelay";
inline constexpr const char* abyssMod      = "abyss_mod";
inline constexpr const char* abyssShimmer  = "abyss_shimmer";
inline constexpr const char* abyssWidth    = "abyss_width";
inline constexpr const char* abyssMix      = "abyss_mix";
inline constexpr const char* abyssFreeze   = "abyss_freeze";

// Modulation: "lfo1_shape", "lfo2_rate_hz", "mod3_dest", ... built by helpers in ParameterLayout.
inline constexpr const char* envAttack  = "env_attack";
inline constexpr const char* envRelease = "env_release";
inline constexpr const char* envGain    = "env_gain";
} // namespace dt::pid
