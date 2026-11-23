#pragma once
// Easy view: the Deep Space XY pad, six performance macros, combo selector, the
// performance pads (Throw, Wash, Drop, Riser) with Pump and a live gate strip,
// one-click module switches and the two Freeze buttons. Everything here is a
// real (automatable) parameter, so performances record into the DAW.

#include "Controls.h"

namespace dt::ui
{
class EasyView final : public juce::Component
{
public:
    explicit EasyView (DeepTranceProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    XYPad pad;
    std::vector<std::unique_ptr<ParamControl>> macros;  // Gate, Swirl, Space, Drift, Build-Up, Mix
    std::unique_ptr<ParamControl> combo, comboAmount;
    std::unique_ptr<PatternBox> gatePattern;
    std::vector<std::unique_ptr<PadButton>> pads;        // Throw, Wash, Drop, Riser
    std::unique_ptr<ParamControl> riserBars, pumpDepth;
    std::unique_ptr<StepEditor> gateStrip;
    std::vector<std::unique_ptr<PillToggle>> modules, freezes;
    juce::Rectangle<int> macroArea, comboArea, padArea, stripArea;
};
} // namespace dt::ui
