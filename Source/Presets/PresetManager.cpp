#include "PresetManager.h"

namespace dt
{
namespace
{
    const juce::Identifier kPresetName ("presetName");
    const juce::Identifier kFactoryIndex ("factoryIndex");
    const juce::Identifier kUiMode ("uiMode");
}

int PresetManager::numFactoryPresets() const { return (int) factoryPresets().size(); }

juce::String PresetManager::factoryName (int index) const
{
    const auto& all = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return {};
    const auto& p = all[(size_t) index];
    return p.category.isEmpty() ? p.name : p.category + " / " + p.name;
}

std::optional<float> PresetManager::factoryCustomPoint (int preset, int lfo, int point) const
{
    const auto& all = factoryPresets();
    if (lfo != 0 || ! juce::isPositiveAndBelow (preset, (int) all.size())) return std::nullopt;
    const auto& shape = all[(size_t) preset].customLfo1;
    if (! juce::isPositiveAndBelow (point, (int) shape.size())) return std::nullopt;
    return shape[(size_t) point];
}

int PresetManager::currentFactoryIndex() const
{
    return (int) apvts.state.getProperty (kFactoryIndex, 0);
}

juce::String PresetManager::currentPresetName() const
{
    return apvts.state.getProperty (kPresetName, "Init").toString();
}

void PresetManager::setPresetInfo (const juce::String& name, int factoryIndex)
{
    apvts.state.setProperty (kPresetName, name, nullptr);
    apvts.state.setProperty (kFactoryIndex, factoryIndex, nullptr);
}

void PresetManager::loadFactory (int index)
{
    const auto& all = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;

    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : all[(size_t) index].values)
    {
        auto* rp = apvts.getParameter (id);
        jassert (rp != nullptr); // a factory preset references an unknown parameter ID
        if (rp != nullptr) rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
    setPresetInfo (factoryName (index), index);
}

juce::File PresetManager::userPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DeepTrance Presets");
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    auto files = userPresetDirectory().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    files.sort();
    return files;
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    const auto dir = userPresetDirectory();
    if (! dir.createDirectory()) return false;

    setPresetInfo (name, -1);
    auto state = apvts.copyState();
    state.removeProperty (kUiMode, nullptr); // view choice is not part of a sound
    const auto xml = state.createXml();
    return xml != nullptr && xml->writeTo (dir.getChildFile (name + kExtension));
}

void PresetManager::syncParametersToState (juce::AudioProcessorValueTreeState& s)
{
    for (auto* p : s.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto child = s.state.getChildWithProperty ("id", rp->getParameterID());
            if (! child.isValid() || ! child.hasProperty ("value")) continue;
            const float target = rp->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (rp->getValue() - target) > 1.0e-6f)
                rp->setValue (target); // APVTS already holds this value; only the parameter object lags
        }
}

bool PresetManager::loadUser (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;

    const auto uiMode = apvts.state.getProperty (kUiMode);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    syncParametersToState (apvts);
    apvts.state.setProperty (kUiMode, uiMode, nullptr);
    setPresetInfo (file.getFileNameWithoutExtension(), -1);
    return true;
}
} // namespace dt
