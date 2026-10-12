#include "Controls.h"
#include "DSP/Modulation.h"
#include "DSP/TranceGate.h"
#include "Params/ParameterLayout.h"
#include "PluginProcessor.h"

namespace dt::ui
{
// ============================================================================
// Look and feel
// ============================================================================
DeepTranceLookAndFeel::DeepTranceLookAndFeel()
{
    setColourScheme ({ colours::bg, colours::panel, colours::panel, colours::outline, colours::text,
                       colours::accent, colours::text, colours::accent, colours::text });

    setColour (juce::Slider::textBoxTextColourId,      colours::text);
    setColour (juce::Slider::textBoxOutlineColourId,   colours::outline);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::bg);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Label::textColourId,              colours::textDim);
    setColour (juce::ToggleButton::textColourId,       colours::text);
    setColour (juce::ToggleButton::tickColourId,       colours::accent2);
    setColour (juce::ComboBox::backgroundColourId,     colours::bg);
    setColour (juce::ComboBox::outlineColourId,        colours::outline);
    setColour (juce::ComboBox::textColourId,           colours::text);
    setColour (juce::ComboBox::arrowColourId,          colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId,    colours::panel);
    setColour (juce::PopupMenu::textColourId,          colours::text);
    setColour (juce::PopupMenu::headerTextColourId,    colours::accent2);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId,       colours::panel);
    setColour (juce::TextButton::buttonOnColourId,     colours::accent);
    setColour (juce::TextButton::textColourOffId,      colours::textDim);
    setColour (juce::TextButton::textColourOnId,       juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId,  colours::panel);
    setColour (juce::AlertWindow::textColourId,        colours::text);
    setColour (juce::AlertWindow::outlineColourId,     colours::outline);
    setColour (juce::TextEditor::backgroundColourId,   colours::bg);
    setColour (juce::TextEditor::textColourId,         colours::text);
    setColour (juce::TextEditor::outlineColourId,      colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::ScrollBar::thumbColourId,         colours::accent.withAlpha (0.6f));
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelHi);
    setColour (juce::TooltipWindow::textColourId,      colours::text);
    setColour (juce::TooltipWindow::outlineColourId,   colours::outline);
}

void DeepTranceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    const auto  bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float lineW  = juce::jlimit (3.0f, 8.0f, radius * 0.12f);
    const float arcR   = radius - lineW;

    // Bipolar parameters (e.g. -100..+100%) fill from the centre.
    const bool  bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
    const float angle  = startAngle + pos * (endAngle - startAngle);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);

    const juce::PathStrokeType stroke (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (colours::outline);
    g.strokePath (track, stroke);
    g.setGradientFill (juce::ColourGradient (colours::accent, centre.x - radius, centre.y,
                                             colours::accent2, centre.x + radius, centre.y, false));
    g.strokePath (value, stroke);

    // Knob body and pointer.
    const float bodyR = arcR - lineW * 1.6f;
    g.setGradientFill (juce::ColourGradient (colours::panelHi, centre.x, centre.y - bodyR,
                                             colours::bg, centre.x, centre.y + bodyR, false));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));
    g.setColour (colours::text);
    g.drawLine ({ centre.getPointOnCircumference (bodyR * 0.25f, angle), centre.getPointOnCircumference (bodyR * 0.9f, angle) },
                juce::jmax (2.0f, lineW * 0.6f));
}

void DeepTranceLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? colours::accent : colours::panel;
    if (over) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.1f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? colours::accent.brighter (0.3f) : colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void DeepTranceLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                              juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    // Bipolar bar (used for mod amounts): fills from the centre.
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (colours::outline);
    g.fillRoundedRectangle (r, 3.0f);
    const float zero = (float) x + (float) s.valueToProportionOfLength (s.getMinimum() < 0.0 ? 0.0 : s.getMinimum()) * (float) w;
    g.setColour (pos >= zero ? colours::accent2 : colours::warm);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (zero, pos), r.getY(), std::abs (pos - zero), r.getHeight()), 3.0f);
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, r.getCentreY() }));
}

// ============================================================================
// ParamControl
// ============================================================================
ParamControl::ParamControl (APVTS& state, juce::RangedAudioParameter& p, Size s, const juce::String& labelOverride)
    : size (s)
{
    const auto id = p.getParameterID();
    label.setText (labelOverride.isNotEmpty() ? labelOverride : p.getName (32), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (size == Size::Large ? 14.0f : 12.0f, size == Size::Large ? juce::Font::bold : juce::Font::plain));
    label.setColour (juce::Label::textColourId, size == Size::Large ? colours::text : colours::textDim);
    addAndMakeVisible (label);

    if (dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr)
    {
        toggle = std::make_unique<juce::ToggleButton> ("On");
        addAndMakeVisible (*toggle);
        toggleAtt = std::make_unique<APVTS::ButtonAttachment> (state, id, *toggle);
    }
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
    {
        combo = std::make_unique<juce::ComboBox>();
        combo->addItemList (choice->choices, 1);
        addAndMakeVisible (*combo);
        comboAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, id, *combo);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, size == Size::Large ? 84 : 90, 18);
        slider->setScrollWheelEnabled (false); // the wheel scrolls the page; it must never nudge a knob by accident
        // Set on the slider itself: its text box is built before the editor's look-and-feel reaches it.
        slider->setColour (juce::Slider::textBoxOutlineColourId, colours::outline);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, colours::bg);
        slider->setColour (juce::Slider::textBoxTextColourId, colours::text);
        addAndMakeVisible (*slider);
        sliderAtt = std::make_unique<APVTS::SliderAttachment> (state, id, *slider);
        slider->setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue())); // double-click = default
        slider->setTooltip ("Drag to change. Double-click to reset.");
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds().reduced (4);
    label.setBounds (r.removeFromTop (size == Size::Large ? 20 : 16));
    if (slider) slider->setBounds (r);
    if (toggle) toggle->setBounds (r.withSizeKeepingCentre (64, 28));
    if (combo)  combo->setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
}

// ============================================================================
// PillToggle
// ============================================================================
PillToggle::PillToggle (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour c)
    : juce::Button (caption), onColour (c), attachment (state, paramId, *this)
{
    setClickingTogglesState (true);
    setTooltip ("Click to switch " + caption.toLowerCase() + " on or off");
}

void PillToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r  = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const float radius = r.getHeight() * 0.5f;

    g.setColour (on ? onColour.withAlpha (0.22f) : colours::panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (on ? onColour : (over ? colours::textDim : colours::outline));
    g.drawRoundedRectangle (r, radius, on ? 1.6f : 1.0f);

    // Status LED, tucked into the left end so the label keeps most of the width.
    const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 11.0f, r.getCentreY() });
    g.setColour (on ? onColour : colours::outline);
    g.fillEllipse (led);
    if (on) { g.setColour (onColour.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.0f)); }

    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (getButtonText(), r.withTrimmedLeft (18.0f).withTrimmedRight (5.0f).toNearestInt(),
                      juce::Justification::centred, 1, 0.8f);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r, radius); }
}

// ============================================================================
// XYPad
// ============================================================================
XYPad::XYPad (APVTS& state, const juce::String& xId, const juce::String& yId)
    : xParam (state.getParameter (xId)), yParam (state.getParameter (yId))
{
    juce::Random rng (42);
    for (int i = 0; i < 90; ++i) stars.push_back ({ rng.nextFloat(), rng.nextFloat() });
    setTooltip ("Drag: left/right = Swirl, down/up = Space. Double-click to recentre.");
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30); // follow host automation
}

XYPad::~XYPad() { stopTimer(); }

juce::Rectangle<float> XYPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (2.0f).withTrimmedTop (22.0f).withTrimmedLeft (18.0f).withTrimmedBottom (18.0f);
}

void XYPad::resized() {}

void XYPad::paint (juce::Graphics& g)
{
    const auto r = padArea();

    // Deep-space backdrop.
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.28f), r.getCentreX(), r.getCentreY(),
                                             colours::bg, r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 10.0f);
    for (const auto& s : stars)
    {
        const float a = 0.25f + 0.5f * s.x * s.y;
        g.setColour (colours::text.withAlpha (a));
        g.fillEllipse (r.getX() + s.x * r.getWidth(), r.getY() + s.y * r.getHeight(), 1.6f, 1.6f);
    }
    g.setColour (colours::outline);
    for (int i = 1; i < 4; ++i)
    {
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
        g.drawVerticalLine   ((int) (r.getX() + r.getWidth() * i / 4.0f), r.getY(), r.getBottom());
    }
    g.drawRoundedRectangle (r, 10.0f, 1.2f);

    // Labels (title, axes).
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("DEEP SPACE", getLocalBounds().removeFromTop (20).withTrimmedLeft (18), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("SWIRL  >", juce::Rectangle<float> (r.getX(), r.getBottom() + 2.0f, r.getWidth(), 16.0f), juce::Justification::centredRight);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getX() - 6.0f, r.getY()));
        g.drawText ("SPACE  >", juce::Rectangle<float> (r.getX() - 6.0f - r.getHeight(), r.getY() - 14.0f, r.getHeight(), 14.0f),
                    juce::Justification::centredRight);
    }

    // The point.
    const float x = xParam->getValue(), y = yParam->getValue();
    const juce::Point<float> p (r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight());
    g.setColour (colours::accent2.withAlpha (0.18f));
    g.drawHorizontalLine ((int) p.y, r.getX(), r.getRight());
    g.drawVerticalLine   ((int) p.x, r.getY(), r.getBottom());
    for (float glow : { 26.0f, 16.0f })
    {
        g.setColour (colours::accent2.withAlpha (glow > 20.0f ? 0.15f : 0.3f));
        g.fillEllipse (juce::Rectangle<float> (glow, glow).withCentre (p));
    }
    g.setColour (dragging ? juce::Colours::white : colours::accent2);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (p));

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("Swirl " + juce::String (juce::roundToInt (x * 100.0f)) + "%   Space " + juce::String (juce::roundToInt (y * 100.0f)) + "%",
                r.reduced (8.0f).removeFromTop (16.0f), juce::Justification::centredRight);
}

void XYPad::setFromMouse (juce::Point<float> pos)
{
    const auto r = padArea();
    xParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (pos.x - r.getX()) / r.getWidth()));
    yParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (r.getBottom() - pos.y) / r.getHeight()));
    repaint();
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    xParam->beginChangeGesture();
    yParam->beginChangeGesture();
    setFromMouse (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void XYPad::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    xParam->endChangeGesture();
    yParam->endChangeGesture();
    repaint();
}

void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* p : { xParam, yParam })
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

// ============================================================================
// StepEditor
// ============================================================================
StepEditor::StepEditor (APVTS& s)
{
    pattern = s.getParameter (dt::pid::gatePattern);
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i] = s.getParameter (dt::gateStepId (i));
    setTooltip ("Draw the gate pattern. Drawing over a factory pattern turns it into an editable Custom copy.");
    startTimerHz (30); // reflects automation and pattern changes from the host
}

StepEditor::~StepEditor() { stopTimer(); }

int StepEditor::activePattern() const
{
    return (int) std::lround (pattern->convertFrom0to1 (pattern->getValue()));
}

float StepEditor::displayedLevel (int step) const
{
    const int p = activePattern();
    return p > 0 ? dt::TranceGate::patterns()[(size_t) p - 1].steps[(size_t) step]
                 : steps[(size_t) step]->getValue();
}

void StepEditor::setStep (size_t i, float v)
{
    if (! inGesture[i]) { steps[i]->beginChangeGesture(); inGesture[i] = true; }
    steps[i]->setValueNotifyingHost (v);
}

void StepEditor::convertToCustom()
{
    // Copy the playing factory pattern into the custom steps, then select Custom.
    const auto& src = dt::TranceGate::patterns()[(size_t) activePattern() - 1].steps;
    for (size_t i = 0; i < 16; ++i) setStep (i, src[i]);
    pattern->beginChangeGesture();
    pattern->setValueNotifyingHost (pattern->convertTo0to1 (0.0f));
    pattern->endChangeGesture();
}

void StepEditor::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 6.0f);

    const float w = r.getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float v = displayedLevel (i);
        auto bar = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f, 4.0f);
        g.setColour (colours::outline);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour ((i % 4 == 0) ? colours::accent2 : colours::accent); // beat starts in cyan
        g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * v), 3.0f);
    }
}

void StepEditor::setFromMouse (juce::Point<float> pos)
{
    const auto r = getLocalBounds().toFloat();
    const int i = juce::jlimit (0, 15, (int) ((pos.x - r.getX()) / (r.getWidth() / 16.0f)));
    const float v = juce::jlimit (0.0f, 1.0f, 1.0f - (pos.y - r.getY()) / r.getHeight());

    if (activePattern() > 0)
        convertToCustom();

    setStep ((size_t) i, v);
    repaint();
}

void StepEditor::mouseDown (const juce::MouseEvent& e) { setFromMouse (e.position); }
void StepEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void StepEditor::mouseUp (const juce::MouseEvent&)
{
    for (size_t i = 0; i < 16; ++i)
        if (inGesture[i]) { steps[i]->endChangeGesture(); inGesture[i] = false; }
}

// ============================================================================
// ShapeEditor
// ============================================================================
ShapeEditor::ShapeEditor (DeepTranceProcessor& p, int lfoIndex)
    : processor (p), lfo (lfoIndex)
{
    shapeParam = processor.getState().getParameter (dt::lfoId (lfo, "shape"));
    setTooltip ("Draw a custom LFO shape. Drawing switches this LFO to the Custom shape.");
    startTimerHz (30);
}

ShapeEditor::~ShapeEditor() { stopTimer(); }

bool ShapeEditor::isCustom() const
{
    return (int) std::lround (shapeParam->convertFrom0to1 (shapeParam->getValue())) == (int) dt::LfoShape::Custom;
}

void ShapeEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    // The drawn shape (dimmed when another shape is active).
    juce::Path path;
    const int n = dt::kCustomPoints;
    for (int i = 0; i <= n; ++i)
    {
        const float v = processor.getCustomPoint (lfo, i % n);
        const juce::Point<float> pt (r.getX() + r.getWidth() * (float) i / (float) n, r.getBottom() - 4.0f - v * (r.getHeight() - 8.0f));
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    const auto col = isCustom() ? colours::accent2 : colours::textDim.withAlpha (0.45f);
    g.setColour (col);
    g.strokePath (path, juce::PathStrokeType (2.0f));
    for (int i = 0; i < n; ++i)
    {
        const float v = processor.getCustomPoint (lfo, i);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ r.getX() + r.getWidth() * (float) i / (float) n,
                                                                         r.getBottom() - 4.0f - v * (r.getHeight() - 8.0f) }));
    }

    // Live output meter on the right edge.
    const float live = processor.getEngine().getLfoValue (lfo); // -1..1
    auto meter = r.removeFromRight (6.0f).reduced (1.0f, 4.0f);
    g.setColour (colours::warm);
    const float y = meter.getCentreY() - live * meter.getHeight() * 0.5f;
    g.fillRect (meter.withY (juce::jmin (y, meter.getCentreY())).withHeight (std::abs (y - meter.getCentreY()) + 1.0f));
}

void ShapeEditor::setFromMouse (juce::Point<float> pos)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const int i = juce::jlimit (0, dt::kCustomPoints - 1, (int) std::lround ((pos.x - r.getX()) / r.getWidth() * dt::kCustomPoints));
    const float v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - 4.0f - pos.y) / (r.getHeight() - 8.0f));

    // Fill any points skipped by a fast drag, so lines are continuous.
    const int from = lastPoint < 0 ? i : lastPoint;
    const float vFrom = lastPoint < 0 ? v : processor.getCustomPoint (lfo, lastPoint);
    const int dir = i >= from ? 1 : -1;
    for (int k = from; k != i + dir; k += dir)
        processor.setCustomPoint (lfo, k, from == i ? v : juce::jmap ((float) (k - from), 0.0f, (float) (i - from), vFrom, v));
    lastPoint = i;

    if (! isCustom())
    {
        shapeParam->beginChangeGesture();
        shapeParam->setValueNotifyingHost (shapeParam->convertTo0to1 ((float) dt::LfoShape::Custom));
        shapeParam->endChangeGesture();
    }
    repaint();
}

void ShapeEditor::mouseDown (const juce::MouseEvent& e) { lastPoint = -1; setFromMouse (e.position); }
void ShapeEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }
} // namespace dt::ui
