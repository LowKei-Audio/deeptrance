#pragma once
// Phase 1 editor: functional, always-dark UI with an Easy/Pro switch.
// Controls are generated from the parameter tree, grouped per module.
// Phase 5 replaces the Easy view with the Deep Space XY pad and macros.

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace dt::ui
{
/** Palette: always dark (hardware style). Text colours keep >= 7:1 contrast on the background. */
namespace colours
{
    inline const juce::Colour bg      { 0xff0e1016 };
    inline const juce::Colour panel   { 0xff171a23 };
    inline const juce::Colour outline { 0xff2a2f3d };
    inline const juce::Colour text    { 0xffeceef4 };
    inline const juce::Colour textDim { 0xffb9bfcc };
    inline const juce::Colour accent  { 0xff8b6cff }; // violet
    inline const juce::Colour accent2 { 0xff22d3ee }; // cyan
}

class DeepTranceLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DeepTranceLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
};

/** One parameter: a knob, toggle or dropdown with its label, bound to the APVTS. */
class ParamControl final : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState&, juce::RangedAudioParameter&);
    void resized() override;

private:
    juce::Label label;
    std::unique_ptr<juce::Slider>       slider;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox>     combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   sliderAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   toggleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
};

/** Draw-to-edit 16-step gate pattern (Pro view).
    Always shows the pattern that is actually playing. Drawing while a factory
    pattern is selected copies it into the custom steps and switches to Custom
    in one move, so every factory pattern is an editable starting point. */
class StepEditor final : public juce::Component, private juce::Timer
{
public:
    explicit StepEditor (juce::AudioProcessorValueTreeState&);
    ~StepEditor() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setFromMouse (juce::Point<float>);
    int  activePattern() const;          // 0 = Custom
    float displayedLevel (int step) const;
    void setStep (size_t i, float v);
    void convertToCustom();

    juce::AudioProcessorValueTreeState& state;
    juce::RangedAudioParameter* pattern = nullptr;
    std::array<juce::RangedAudioParameter*, 16> steps {};
    std::array<bool, 16> inGesture {};
};

/** A card holding one module's controls. */
class ModuleSection final : public juce::Component
{
public:
    ModuleSection (const juce::String& title, juce::AudioProcessorValueTreeState&,
                   const juce::Array<juce::RangedAudioParameter*>&, bool withStepEditor);
    int  heightForWidth (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kHeader = 30, kCellW = 116, kCellH = 104, kStepH = 110, kPad = 10;
    int columnsFor (int width) const { return juce::jmax (1, (width - 2 * kPad) / kCellW); }

    juce::String title;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::unique_ptr<StepEditor> stepEditor;
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
    void rebuild();
    void layoutContent();

    DeepTranceProcessor& processor;
    dt::ui::DeepTranceLookAndFeel lnf;

    juce::Label        titleLabel, phaseLabel;
    juce::TextButton   easyButton { "EASY" }, proButton { "PRO" };
    juce::Viewport     viewport;
    juce::Component    content;
    std::vector<std::unique_ptr<dt::ui::ModuleSection>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepTranceEditor)
};
