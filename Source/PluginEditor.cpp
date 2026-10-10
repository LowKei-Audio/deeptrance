#include "PluginEditor.h"
#include "DSP/TranceGate.h"
#include "Params/ParameterLayout.h"

using namespace dt::ui;

// ============================================================================
// Look and feel
// ============================================================================
DeepTranceLookAndFeel::DeepTranceLookAndFeel()
{
    setColourScheme ({ colours::bg, colours::panel, colours::panel, colours::outline, colours::text,
                       colours::accent, colours::text, colours::accent, colours::text });

    setColour (juce::Slider::textBoxTextColourId,      colours::text);
    setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Label::textColourId,              colours::textDim);
    setColour (juce::ToggleButton::textColourId,       colours::text);
    setColour (juce::ToggleButton::tickColourId,       colours::accent2);
    setColour (juce::ComboBox::backgroundColourId,     colours::bg);
    setColour (juce::ComboBox::outlineColourId,        colours::outline);
    setColour (juce::ComboBox::textColourId,           colours::text);
    setColour (juce::PopupMenu::backgroundColourId,    colours::panel);
    setColour (juce::PopupMenu::textColourId,          colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::TextButton::buttonColourId,       colours::panel);
    setColour (juce::TextButton::buttonOnColourId,     colours::accent);
    setColour (juce::TextButton::textColourOffId,      colours::textDim);
    setColour (juce::TextButton::textColourOnId,       juce::Colours::white);
}

void DeepTranceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider&)
{
    const auto  bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float lineW  = 3.5f;

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, angle, true);

    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (colours::accent, centre.x - radius, centre.y,
                                             colours::accent2, centre.x + radius, centre.y, false));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer
    const auto tip = centre.getPointOnCircumference (radius - lineW * 3.0f, angle);
    g.setColour (colours::text);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.25f, angle), tip }, 2.0f);
}

// ============================================================================
// ParamControl
// ============================================================================
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& state, juce::RangedAudioParameter& p)
{
    const auto id = p.getParameterID();
    label.setText (p.getName (32), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (label);

    if (dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr)
    {
        toggle = std::make_unique<juce::ToggleButton> ("On");
        addAndMakeVisible (*toggle);
        toggleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *toggle);
    }
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
    {
        combo = std::make_unique<juce::ComboBox>();
        combo->addItemList (choice->choices, 1);
        addAndMakeVisible (*combo);
        comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *combo);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        slider->setColour (juce::Slider::textBoxOutlineColourId, colours::outline); // subtle, not white
        slider->setScrollWheelEnabled (false); // the wheel scrolls the page; it must never nudge a knob by accident
        addAndMakeVisible (*slider);
        sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *slider);
        slider->setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue())); // double-click = default
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds().reduced (4);
    label.setBounds (r.removeFromTop (16));
    if (slider) slider->setBounds (r);
    if (toggle) toggle->setBounds (r.withSizeKeepingCentre (64, 28));
    if (combo)  combo->setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
}

// ============================================================================
// StepEditor
// ============================================================================
StepEditor::StepEditor (juce::AudioProcessorValueTreeState& s) : state (s)
{
    pattern = state.getParameter (dt::pid::gatePattern);
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i] = state.getParameter (dt::gateStepId (i));
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
// ModuleSection
// ============================================================================
ModuleSection::ModuleSection (const juce::String& t, juce::AudioProcessorValueTreeState& state,
                              const juce::Array<juce::RangedAudioParameter*>& params, bool withStepEditor)
    : title (t)
{
    for (auto* p : params)
        addAndMakeVisible (*controls.emplace_back (std::make_unique<ParamControl> (state, *p)));

    if (withStepEditor)
        addAndMakeVisible (*(stepEditor = std::make_unique<StepEditor> (state)));
}

int ModuleSection::heightForWidth (int width) const
{
    const int cols = columnsFor (width);
    const int rows = ((int) controls.size() + cols - 1) / cols;
    return kHeader + rows * kCellH + (stepEditor ? kStepH : 0) + kPad;
}

void ModuleSection::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (kHeader).reduced (kPad, 0),
                juce::Justification::centredLeft);

    if (stepEditor)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Draw to edit the pattern (switches Gate Pattern to Custom)",
                    stepEditor->getBounds().translated (0, -16).withHeight (16), juce::Justification::centredLeft);
    }
}

void ModuleSection::resized()
{
    auto r = getLocalBounds().reduced (kPad, 0);
    r.removeFromTop (kHeader);
    const int cols = columnsFor (getWidth());

    for (size_t i = 0; i < controls.size(); ++i)
        controls[i]->setBounds (r.getX() + (int) (i % (size_t) cols) * kCellW,
                                r.getY() + (int) (i / (size_t) cols) * kCellH, kCellW, kCellH);

    if (stepEditor)
        stepEditor->setBounds (r.withTop (getHeight() - kStepH - kPad + 18).withHeight (kStepH - 22));
}

// ============================================================================
// Editor
// ============================================================================
DeepTranceEditor::DeepTranceEditor (DeepTranceProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);

    titleLabel.setText ("DEEPTRANCE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (titleLabel);

    phaseLabel.setText ("v0.1 - Phase 1", juce::dontSendNotification);
    phaseLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (phaseLabel);

    // One click switches views; the choice is saved with the session.
    for (auto* b : { &easyButton, &proButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        addAndMakeVisible (*b);
    }
    proButton.setToggleState (processor.isProMode(), juce::dontSendNotification);
    easyButton.setToggleState (! processor.isProMode(), juce::dontSendNotification);
    easyButton.onClick = [this] { if (easyButton.getToggleState()) { processor.setProMode (false); rebuild(); } };
    proButton.onClick  = [this] { if (proButton.getToggleState())  { processor.setProMode (true);  rebuild(); } };

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (560, 420, 1600, 1200);
    setSize (900, 640);
    rebuild();
}

DeepTranceEditor::~DeepTranceEditor()
{
    sections.clear();
    setLookAndFeel (nullptr);
}

void DeepTranceEditor::rebuild()
{
    sections.clear();
    const bool pro = processor.isProMode();
    const auto& easy = dt::easyParameterIds();
    auto& state = processor.getState();

    for (const auto* group : processor.getParameterTree().getSubgroups (false))
    {
        juce::Array<juce::RangedAudioParameter*> params;
        bool hasSteps = false;

        for (auto* raw : group->getParameters (false))
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw))
            {
                const auto id = p->getParameterID();
                if (id.startsWith (dt::pid::gateStepPrefix)) { hasSteps = true; continue; } // drawn by StepEditor
                if (pro || easy.contains (id)) params.add (p);
            }

        if (! params.isEmpty())
        {
            auto& s = sections.emplace_back (std::make_unique<dt::ui::ModuleSection> (
                group->getName(), state, params, pro && hasSteps));
            content.addAndMakeVisible (*s);
        }
    }
    layoutContent();
}

void DeepTranceEditor::layoutContent()
{
    const int width = viewport.getMaximumVisibleWidth();
    int y = 0;
    for (auto& s : sections)
    {
        const int h = s->heightForWidth (width);
        s->setBounds (0, y, width, h);
        y += h + 10;
    }
    content.setSize (width, y);
}

void DeepTranceEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
}

void DeepTranceEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (40);
    proButton.setBounds (header.removeFromRight (70).reduced (0, 6));
    easyButton.setBounds (header.removeFromRight (70).reduced (0, 6));
    titleLabel.setBounds (header.removeFromLeft (200));
    phaseLabel.setBounds (header);
    r.removeFromTop (8);
    viewport.setBounds (r);
    layoutContent();
}
