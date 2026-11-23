#pragma once
// Pro view: every module as a card (on/off switch in the card header), the
// drawable gate, and the modulation area (2 LFOs with draw editors, envelope
// follower, 8-slot matrix). Scrolls vertically; the mouse wheel never changes knobs.

#include "Controls.h"

namespace dt::ui
{
/** A card holding one parameter group. A "*_on" bool becomes the header switch. */
class ModuleSection final : public juce::Component
{
public:
    ModuleSection (const juce::String& title, DeepTranceProcessor&, const juce::Array<juce::RangedAudioParameter*>&,
                   juce::RangedAudioParameter* onParam, bool withStepEditor);
    int  heightForWidth (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kHeader = 34, kCellW = 116, kCellH = 104, kStepH = 110, kPad = 10;
    int columnsFor (int width) const { return juce::jmax (1, (width - 2 * kPad) / kCellW); }

    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> controls;   // ParamControl, PatternBox or PadButton
    std::unique_ptr<PillToggle> onSwitch;
    std::unique_ptr<StepEditor> stepEditor;
};

/** LFO 1/2, envelope follower and the 8-slot matrix. */
class ModulationSection final : public juce::Component
{
public:
    explicit ModulationSection (DeepTranceProcessor&);
    int  heightForWidth (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct MatrixRow
    {
        juce::Label number;
        juce::ComboBox source, dest;
        juce::Slider amount;
        std::unique_ptr<APVTS::ComboBoxAttachment> sourceAtt, destAtt;
        std::unique_ptr<APVTS::SliderAttachment> amountAtt;
    };

    static constexpr int kHeader = 34, kPad = 10, kLfoH = 230, kEnvH = 130, kRowH = 32;

    std::vector<std::unique_ptr<ParamControl>> lfoControls[2], envControls;
    std::unique_ptr<ShapeEditor> shapeEditors[2];
    std::array<std::unique_ptr<MatrixRow>, 8> rows;
    juce::Rectangle<int> lfoAreas[2], envArea, matrixArea;
};

class ProView final : public juce::Component
{
public:
    explicit ProView (DeepTranceProcessor&);
    void resized() override;

private:
    void layoutContent();

    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<ModuleSection>> sections;
    std::unique_ptr<ModulationSection> modulation;
};
} // namespace dt::ui
