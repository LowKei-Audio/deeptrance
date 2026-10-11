#pragma once
// Factory presets (built into the plugin, also exposed to the host as programs)
// and user presets (.dtpreset XML files in Documents/DeepTrance Presets).

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace dt
{
struct FactoryPreset
{
    juce::String category, name;
    std::vector<std::pair<juce::String, float>> values; // plain (displayed) units; unlisted params = default
    std::vector<float> customLfo1;                      // optional drawn shape (16 points)
};

/** The built-in library, in browser order. Index 0 is "Init". */
const std::vector<FactoryPreset>& factoryPresets();

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

    int          numFactoryPresets() const;
    juce::String factoryName (int index) const;      // "Category / Name"
    std::optional<float> factoryCustomPoint (int preset, int lfo, int point) const;
    int          currentFactoryIndex() const;        // -1 when a user preset (or edited state) is loaded

    /** Resets every parameter to its default, then applies the preset's values. */
    void loadFactory (int index);

    juce::String currentPresetName() const;

    static juce::File userPresetDirectory();
    juce::Array<juce::File> userPresets() const;
    bool saveUser (const juce::String& name);
    bool loadUser (const juce::File& file);

    static constexpr const char* kExtension = ".dtpreset";

    /** After replaceState(): snaps each parameter's own value to the saved one.
        JUCE skips parameters whose rounded value already matches (e.g. a switch
        holding a raw 0.15 when "off" was saved), leaving the raw value reported
        to the host. This makes restored values exact. Message thread. */
    static void syncParametersToState (juce::AudioProcessorValueTreeState&);

private:
    void setPresetInfo (const juce::String& name, int factoryIndex);

    juce::AudioProcessorValueTreeState& apvts;
};
} // namespace dt
