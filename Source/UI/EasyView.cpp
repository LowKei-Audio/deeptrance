#include "EasyView.h"
#include "Params/ParameterLayout.h"
#include "PluginProcessor.h"

namespace dt::ui
{
EasyView::EasyView (DeepTranceProcessor& p)
    : pad (p.getState(), pid::macroSwirl, pid::macroSpace)
{
    auto& s = p.getState();
    addAndMakeVisible (pad);

    const std::pair<const char*, const char*> macroDefs[] = {
        { pid::macroGate, "GATE" }, { pid::macroSwirl, "SWIRL" }, { pid::macroSpace, "SPACE" },
        { pid::macroDrift, "DRIFT" }, { pid::buildUp, "BUILD-UP" }, { pid::mix, "MIX" } };
    for (const auto& [id, name] : macroDefs)
        addAndMakeVisible (*macros.emplace_back (std::make_unique<ParamControl> (s, *s.getParameter (id), ParamControl::Size::Large, name)));

    combo       = std::make_unique<ParamControl> (s, *s.getParameter (pid::combo), ParamControl::Size::Normal, "COMBO");
    comboAmount = std::make_unique<ParamControl> (s, *s.getParameter (pid::comboAmount), ParamControl::Size::Normal, "Amount");
    gatePattern = std::make_unique<ParamControl> (s, *s.getParameter (pid::gatePattern), ParamControl::Size::Normal, "Gate Pattern");
    for (auto* c : { combo.get(), comboAmount.get(), gatePattern.get() }) addAndMakeVisible (*c);

    const std::pair<const char*, const char*> moduleDefs[] = {
        { pid::gateOn, "GATE" }, { pid::vowelOn, "VOWEL" }, { pid::swirlOn, "SWIRL" }, { pid::driftOn, "DRIFT" },
        { pid::acidOn, "ACID" }, { pid::punchOn, "PUNCH" }, { pid::cloudOn, "CLOUD" }, { pid::delayOn, "DELAY" },
        { pid::abyssOn, "ABYSS" } };
    for (const auto& [id, name] : moduleDefs)
        addAndMakeVisible (*modules.emplace_back (std::make_unique<PillToggle> (s, id, name, colours::accent2)));

    addAndMakeVisible (*freezes.emplace_back (std::make_unique<PillToggle> (s, pid::cloudFreeze, "FREEZE CLOUD", colours::warm)));
    addAndMakeVisible (*freezes.emplace_back (std::make_unique<PillToggle> (s, pid::abyssFreeze, "FREEZE ABYSS", colours::warm)));
}

void EasyView::paint (juce::Graphics& g)
{
    auto card = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    };
    card (macroArea);
    card (comboArea);
    card (stripArea);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("MACROS  (50% = as set in Pro)", macroArea.reduced (14, 6).removeFromTop (16), juce::Justification::centredLeft);
    g.drawText ("MODULES", stripArea.reduced (14, 6).removeFromTop (16), juce::Justification::centredLeft);
}

void EasyView::resized()
{
    auto r = getLocalBounds();

    // Bottom strip: module switches + freezes.
    stripArea = r.removeFromBottom (86);
    r.removeFromBottom (10);
    {
        auto s = stripArea.reduced (12, 8);
        s.removeFromTop (18);
        const int freezeW = 132, gap = 6;
        auto freezeArea = s.removeFromRight (freezeW * 2 + gap);
        for (auto& f : freezes)
        {
            f->setBounds (freezeArea.removeFromLeft (freezeW).withSizeKeepingCentre (freezeW, 34));
            freezeArea.removeFromLeft (gap);
        }
        s.removeFromRight (14);
        const int w = (s.getWidth() - gap * ((int) modules.size() - 1)) / (int) modules.size();
        for (auto& m : modules) { m->setBounds (s.removeFromLeft (w).withSizeKeepingCentre (w, 34)); s.removeFromLeft (gap); }
    }

    // Left: the XY pad as a square.
    const int padSize = juce::jmin (r.getHeight(), (int) (r.getWidth() * 0.47f));
    pad.setBounds (r.removeFromLeft (padSize).withSizeKeepingCentre (padSize, padSize));
    r.removeFromLeft (12);

    // Right: macros (2 x 3) above the combo row.
    comboArea = r.removeFromBottom (110);
    r.removeFromBottom (10);
    macroArea = r;
    {
        auto m = macroArea.reduced (10, 6);
        m.removeFromTop (18);
        const int cellW = m.getWidth() / 3, cellH = m.getHeight() / 2;
        for (size_t i = 0; i < macros.size(); ++i)
            macros[i]->setBounds (m.getX() + (int) (i % 3) * cellW, m.getY() + (int) (i / 3) * cellH, cellW, cellH);
    }
    {
        auto c = comboArea.reduced (12, 8);
        combo->setBounds (c.removeFromLeft ((int) (c.getWidth() * 0.42f)).withSizeKeepingCentre ((int) (c.getWidth() * 0.42f), 64));
        comboAmount->setBounds (c.removeFromLeft (100));
        gatePattern->setBounds (c.withSizeKeepingCentre (c.getWidth(), 64));
    }
}
} // namespace dt::ui
