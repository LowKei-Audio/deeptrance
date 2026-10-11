#include "ProView.h"
#include "DSP/DeepTranceEngine.h"
#include "Params/ParameterLayout.h"
#include "PluginProcessor.h"

namespace dt::ui
{
namespace
{
    void paintCard (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, int headerH)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), bounds.removeFromTop (headerH).reduced (12, 0), juce::Justification::centredLeft);
    }
}

// ============================================================================
// ModuleSection
// ============================================================================
ModuleSection::ModuleSection (const juce::String& t, APVTS& state, const juce::Array<juce::RangedAudioParameter*>& params,
                              juce::RangedAudioParameter* onParam, bool withStepEditor)
    : title (t)
{
    for (auto* p : params)
        addAndMakeVisible (*controls.emplace_back (std::make_unique<ParamControl> (state, *p)));

    if (onParam != nullptr)
        addAndMakeVisible (*(onSwitch = std::make_unique<PillToggle> (state, onParam->getParameterID(), "ON", colours::accent2)));

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
    paintCard (g, getLocalBounds(), title, kHeader);

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
    if (onSwitch)
        onSwitch->setBounds (getLocalBounds().removeFromTop (kHeader).removeFromRight (84).reduced (10, 6));

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
// ModulationSection
// ============================================================================
ModulationSection::ModulationSection (DeepTranceProcessor& p)
{
    auto& s = p.getState();
    for (int l = 0; l < 2; ++l)
    {
        for (const char* suffix : { "shape", "sync", "rate_sync", "rate_hz" })
        {
            auto* param = s.getParameter (lfoId (l, suffix));
            const juce::String label = juce::String (suffix) == "shape" ? "Shape" : juce::String (suffix) == "sync" ? "Sync"
                                     : juce::String (suffix) == "rate_sync" ? "Rate" : "Rate (free)";
            addAndMakeVisible (*lfoControls[l].emplace_back (std::make_unique<ParamControl> (s, *param, ParamControl::Size::Normal, label)));
        }
        addAndMakeVisible (*(shapeEditors[l] = std::make_unique<ShapeEditor> (p, l)));
    }

    for (const char* id : { pid::envAttack, pid::envRelease, pid::envGain })
        addAndMakeVisible (*envControls.emplace_back (std::make_unique<ParamControl> (s, *s.getParameter (id))));

    for (int i = 0; i < kModSlots; ++i)
    {
        auto row = std::make_unique<MatrixRow>();
        row->number.setText (juce::String (i + 1), juce::dontSendNotification);
        row->number.setColour (juce::Label::textColourId, colours::textDim);
        row->number.setJustificationType (juce::Justification::centred);

        auto fill = [] (juce::ComboBox& box, juce::RangedAudioParameter* param)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param)) box.addItemList (c->choices, 1);
        };
        fill (row->source, s.getParameter (modId (i, "source")));
        fill (row->dest,   s.getParameter (modId (i, "dest")));
        row->amount.setSliderStyle (juce::Slider::LinearHorizontal);
        row->amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        row->amount.setScrollWheelEnabled (false);
        row->amount.setColour (juce::Slider::textBoxOutlineColourId, colours::outline);
        row->amount.setColour (juce::Slider::textBoxBackgroundColourId, colours::bg);
        row->amount.setColour (juce::Slider::textBoxTextColourId, colours::text);
        row->amount.setTooltip ("Modulation amount. Double-click to reset.");
        row->amount.setDoubleClickReturnValue (true, 0.0);

        row->sourceAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "source"), row->source);
        row->destAtt   = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "dest"),   row->dest);
        row->amountAtt = std::make_unique<APVTS::SliderAttachment>   (s, modId (i, "amount"), row->amount);

        for (juce::Component* c : { (juce::Component*) &row->number, (juce::Component*) &row->source,
                                    (juce::Component*) &row->dest, (juce::Component*) &row->amount })
            addAndMakeVisible (c);
        rows[(size_t) i] = std::move (row);
    }
}

int ModulationSection::heightForWidth (int width) const
{
    const bool wide = width >= 760; // LFOs side by side
    const int lfoBlock = wide ? kLfoH : 2 * kLfoH + kPad;
    return kHeader + lfoBlock + kPad + kEnvH + kPad + 24 + kModSlots * kRowH + 2 * kPad;
}

void ModulationSection::paint (juce::Graphics& g)
{
    paintCard (g, getLocalBounds(), "Modulation", kHeader);

    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    auto sub = [&g] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (colours::bg.withAlpha (0.5f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (colours::accent2);
        g.drawText (t, r.removeFromTop (20).reduced (8, 0), juce::Justification::centredLeft);
    };
    sub (lfoAreas[0], "LFO 1");
    sub (lfoAreas[1], "LFO 2");
    sub (envArea, "ENVELOPE FOLLOWER  (input level)");
    sub (matrixArea, "MOD MATRIX  (source  >  destination,  amount)");
}

void ModulationSection::resized()
{
    auto r = getLocalBounds().reduced (kPad, 0);
    r.removeFromTop (kHeader);

    const bool wide = getWidth() >= 760;
    if (wide)
    {
        auto lfoRow = r.removeFromTop (kLfoH);
        lfoAreas[0] = lfoRow.removeFromLeft (lfoRow.getWidth() / 2 - kPad / 2);
        lfoRow.removeFromLeft (kPad);
        lfoAreas[1] = lfoRow;
    }
    else
    {
        lfoAreas[0] = r.removeFromTop (kLfoH);
        r.removeFromTop (kPad);
        lfoAreas[1] = r.removeFromTop (kLfoH);
    }
    r.removeFromTop (kPad);

    for (int l = 0; l < 2; ++l)
    {
        auto a = lfoAreas[l].reduced (6);
        a.removeFromTop (18);
        auto controlsRow = a.removeFromTop (100);
        const int w = controlsRow.getWidth() / 4;
        for (auto& c : lfoControls[l]) c->setBounds (controlsRow.removeFromLeft (w));
        a.removeFromTop (4);
        shapeEditors[l]->setBounds (a);
    }

    envArea = r.removeFromTop (kEnvH);
    {
        auto a = envArea.reduced (6);
        a.removeFromTop (18);
        for (auto& c : envControls) c->setBounds (a.removeFromLeft (116));
    }
    r.removeFromTop (kPad);

    matrixArea = r.withHeight (24 + kModSlots * kRowH + kPad);
    auto m = matrixArea.reduced (8, 4);
    m.removeFromTop (22);
    for (auto& row : rows)
    {
        auto line = m.removeFromTop (kRowH).reduced (0, 3);
        row->number.setBounds (line.removeFromLeft (24));
        const int comboW = juce::jmin (190, line.getWidth() / 3);
        row->source.setBounds (line.removeFromLeft (comboW - 40));
        line.removeFromLeft (8);
        row->dest.setBounds (line.removeFromLeft (comboW));
        line.removeFromLeft (12);
        row->amount.setBounds (line);
    }
}

// ============================================================================
// ProView
// ============================================================================
ProView::ProView (DeepTranceProcessor& p)
{
    auto& state = p.getState();
    const juce::StringArray skip { group::lfo1, group::lfo2, group::env, group::mod }; // drawn by ModulationSection

    for (const auto* grp : p.getParameterTree().getSubgroups (false))
    {
        if (skip.contains (grp->getID())) continue;

        juce::Array<juce::RangedAudioParameter*> params;
        juce::RangedAudioParameter* onParam = nullptr;
        bool hasSteps = false;

        for (auto* raw : grp->getParameters (false))
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (raw))
            {
                const auto id = rp->getParameterID();
                if (id.startsWith (pid::gateStepPrefix)) { hasSteps = true; continue; } // drawn by StepEditor
                if (id.endsWith ("_on")) { onParam = rp; continue; }                     // header switch
                params.add (rp);
            }

        auto& s = sections.emplace_back (std::make_unique<ModuleSection> (grp->getName(), state, params, onParam, hasSteps));
        content.addAndMakeVisible (*s);
    }

    modulation = std::make_unique<ModulationSection> (p);
    content.addAndMakeVisible (*modulation);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
}

void ProView::layoutContent()
{
    // Always reserve the scrollbar: the content is taller than the view, and measuring
    // before the scrollbar appears would let it cover the right edge of every card.
    const int width = viewport.getWidth() - viewport.getScrollBarThickness() - 4;
    int y = 0;
    for (auto& s : sections)
    {
        const int h = s->heightForWidth (width);
        s->setBounds (0, y, width, h);
        y += h + 10;
    }
    const int mh = modulation->heightForWidth (width);
    modulation->setBounds (0, y, width, mh);
    content.setSize (width, y + mh);
}

void ProView::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutContent();
}
} // namespace dt::ui
