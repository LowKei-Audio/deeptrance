#pragma once
// Shared UI building blocks: look-and-feel, parameter controls, the Deep Space
// XY pad, the 16-step gate editor and the drawable LFO shape editor.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class DeepTranceProcessor;

namespace dt::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Always-dark hardware palette. Text colours keep >= 7:1 contrast on the backgrounds. */
namespace colours
{
    inline const juce::Colour bg      { 0xff0b0d13 };
    inline const juce::Colour panel   { 0xff151823 };
    inline const juce::Colour panelHi { 0xff1c2030 };
    inline const juce::Colour outline { 0xff2a2f40 };
    inline const juce::Colour text    { 0xffeceef4 };
    inline const juce::Colour textDim { 0xffb9bfcc };
    inline const juce::Colour accent  { 0xff8b6cff }; // violet
    inline const juce::Colour accent2 { 0xff22d3ee }; // cyan
    inline const juce::Colour warm    { 0xffff6b9a }; // pink: freeze / combo highlights
}

class DeepTranceLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DeepTranceLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
};

/** One parameter: a knob, toggle or dropdown with its label, bound to the APVTS. */
class ParamControl final : public juce::Component
{
public:
    enum class Size { Normal, Large };
    ParamControl (APVTS&, juce::RangedAudioParameter&, Size size = Size::Normal, const juce::String& labelOverride = {});
    void resized() override;

private:
    Size size;
    juce::Label label;
    std::unique_ptr<juce::Slider>       slider;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::ComboBox>     combo;
    std::unique_ptr<APVTS::SliderAttachment>   sliderAtt;
    std::unique_ptr<APVTS::ButtonAttachment>   toggleAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> comboAtt;
};

/** One-click on/off pill bound to a bool parameter (module switches, Freeze). */
class PillToggle final : public juce::Button
{
public:
    PillToggle (APVTS&, const juce::String& paramId, const juce::String& caption, juce::Colour onColour);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour onColour;
    APVTS::ButtonAttachment attachment;
};

/** The "Deep Space" pad: X = Swirl macro, Y = Space macro. Double-click recentres. */
class XYPad final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    XYPad (APVTS&, const juce::String& xParamId, const juce::String& yParamId);
    ~XYPad() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> padArea() const;
    void setFromMouse (juce::Point<float>);

    juce::RangedAudioParameter* xParam = nullptr;
    juce::RangedAudioParameter* yParam = nullptr;
    bool dragging = false;
    std::vector<juce::Point<float>> stars; // normalised star field, generated once
};

/** Momentary performance pad bound to a bool parameter (Throw, Wash, Drop, Riser).
    Held = on, released = off; Shift-click latches it on until the next click. The
    optional `progress` (0..1) is drawn as a fill (the Riser's climb). */
class PadButton final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    PadButton (APVTS&, const juce::String& paramId, const juce::String& caption, juce::Colour colour,
               std::function<float()> progress = {});
    ~PadButton() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override
    {
        if (latched && param->getValue() < 0.5f) latched = false;   // automation or a preset released it
        repaint();
    }
    void set (bool on);
    juce::RangedAudioParameter* param = nullptr;
    juce::String caption;
    juce::Colour colour;
    std::function<float()> progress;
    bool latched = false, held = false;
};

/** One menu for the gate pattern: Custom, the 1.0 patterns and the 1.1 ones, written to
    "Gate Pattern" and "Gate Pattern+" (kept as two parameters for DAW automation). */
class PatternBox final : public juce::Component, private juce::Timer
{
public:
    PatternBox (APVTS&, const juce::String& labelText = "Gate Pattern");
    ~PatternBox() override;
    void resized() override;

private:
    void timerCallback() override;
    int  currentItem() const;   // combo item id
    juce::Label label;
    juce::ComboBox box;
    juce::RangedAudioParameter* pattern = nullptr;
    juce::RangedAudioParameter* patternExt = nullptr;
};

/** Draw-to-edit 16-step gate pattern. Always shows the pattern that is actually
    playing; drawing on a factory pattern copies it into Custom first. With a
    `playhead` (current step, or -1) the playing step is lit as the music runs. */
class StepEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit StepEditor (APVTS&, std::function<int()> playhead = {});
    ~StepEditor() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setFromMouse (juce::Point<float>);
    int   activePattern() const;          // 0 = Custom
    float displayedLevel (int step) const;
    void  setStep (size_t i, float v);
    void  convertToCustom();

    juce::RangedAudioParameter* pattern = nullptr;
    juce::RangedAudioParameter* patternExt = nullptr;
    std::array<juce::RangedAudioParameter*, 16> steps {};
    std::array<bool, 16> inGesture {};
    std::function<int()> playhead;
};

/** Drawable 16-point LFO shape (used when the LFO's shape is "Custom"). Also shows the live LFO value. */
class ShapeEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ShapeEditor (DeepTranceProcessor&, int lfoIndex);
    ~ShapeEditor() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setFromMouse (juce::Point<float>);
    bool isCustom() const;

    DeepTranceProcessor& processor;
    int lfo;
    juce::RangedAudioParameter* shapeParam = nullptr;
    int lastPoint = -1;
};
} // namespace dt::ui
