#pragma once
// DeepTrance editor: header (logo, preset browser, Easy/Pro) above either the
// Easy view or the Pro view. Always dark. Resizable.

#include "PluginProcessor.h"
#include "UI/Controls.h"
#include "UI/EasyView.h"
#include "UI/ProView.h"

namespace dt::ui
{
/** Prev / name (menu) / next, A/B compare, Random, Save. Factory presets by category,
    then user presets. */
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (DeepTranceProcessor&);
    ~PresetBar() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void showMenu();
    void step (int delta);
    void saveAs();

    DeepTranceProcessor& processor;
    juce::TextButton prev { "<" }, next { ">" }, name, save { "SAVE" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copy { "COPY" }, dice { "RANDOM" };
};
} // namespace dt::ui

class DeepTranceEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DeepTranceEditor (DeepTranceProcessor&);
    ~DeepTranceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showView (bool pro);

    DeepTranceProcessor& processor;
    dt::ui::DeepTranceLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    dt::ui::PresetBar presetBar;
    juce::TextButton  easyButton { "EASY" }, proButton { "PRO" };
    std::unique_ptr<juce::Component> view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepTranceEditor)
};
