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
    gatePattern = std::make_unique<PatternBox> (s);
    for (auto* c : { (juce::Component*) combo.get(), (juce::Component*) comboAmount.get(), (juce::Component*) gatePattern.get() }) addAndMakeVisible (*c);

    // Performance pads. The Riser fills as it climbs toward the drop.
    const auto& engine = p.getEngine();
    addAndMakeVisible (*pads.emplace_back (std::make_unique<PadButton> (s, pid::padThrow, "THROW", colours::accent2)));
    addAndMakeVisible (*pads.emplace_back (std::make_unique<PadButton> (s, pid::padWash,  "WASH",  colours::accent)));
    addAndMakeVisible (*pads.emplace_back (std::make_unique<PadButton> (s, pid::padDrop,  "DROP",  colours::warm)));
    addAndMakeVisible (*pads.emplace_back (std::make_unique<PadButton> (s, pid::padRiser, "RISER", colours::warm,
                                                                        [&engine] { return engine.getRiser(); })));
    pads[0]->setTooltip ("Hold to throw the sound into the delay and reverb; let go and the tail rings out. Shift-click latches.");
    pads[1]->setTooltip ("Hold to wash everything into a huge, endless reverb. Shift-click latches.");
    pads[2]->setTooltip ("Hold to cut the lows (kick and bass vanish) just before a drop. Shift-click latches.");
    pads[3]->setTooltip ("Hold to build: Build-Up rises to 100% over the Riser length. Let go at the drop.");
    riserBars = std::make_unique<ParamControl> (s, *s.getParameter (pid::riserBars), ParamControl::Size::Normal, "Riser");
    pumpDepth = std::make_unique<ParamControl> (s, *s.getParameter (pid::pumpDepth), ParamControl::Size::Normal, "Pump Depth");
    gateStrip = std::make_unique<StepEditor> (s, [&engine] { return engine.getGateStep(); });
    for (auto* c : { (juce::Component*) riserBars.get(), (juce::Component*) pumpDepth.get(), (juce::Component*) gateStrip.get() })
        addAndMakeVisible (*c);

    const std::pair<const char*, const char*> moduleDefs[] = {
        { pid::gateOn, "GATE" }, { pid::vowelOn, "VOWEL" }, { pid::swirlOn, "SWIRL" }, { pid::driftOn, "DRIFT" },
        { pid::acidOn, "ACID" }, { pid::punchOn, "PUNCH" }, { pid::cloudOn, "CLOUD" }, { pid::delayOn, "DELAY" },
        { pid::abyssOn, "ABYSS" }, { pid::pumpOn, "PUMP" } };
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
    card (padArea);
    card (stripArea);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("MACROS  (50% = as set in Pro)", macroArea.reduced (14, 6).removeFromTop (16), juce::Justification::centredLeft);
    g.drawText ("MODULES", stripArea.reduced (14, 6).removeFromTop (16), juce::Justification::centredLeft);
    g.drawText ("PERFORM  (hold a pad; Shift-click latches)", padArea.reduced (14, 6).removeFromTop (16), juce::Justification::centredLeft);
    if (! gateStrip->getBounds().isEmpty())
        g.drawText ("GATE", gateStrip->getBounds().translated (0, -17).withHeight (16), juce::Justification::centredLeft);
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
        const int freezeW = 116, gap = 6;
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

    // Above it: the performance row (pads, Riser length, Pump, live gate strip).
    padArea = r.removeFromBottom (104);
    r.removeFromBottom (10);
    {
        auto a = padArea.reduced (12, 8);
        a.removeFromTop (18);
        const int padW = juce::jlimit (64, 110, a.getWidth() / 9), gap = 6;
        for (auto& pad : pads) { pad->setBounds (a.removeFromLeft (padW).reduced (0, 2)); a.removeFromLeft (gap); }
        a.removeFromLeft (4);
        // Riser and Pump use the full row height (their labels sit level with the row
        // caption, which only spans the pads), so the Pump knob stays a usable size.
        const int top = padArea.getY() + 4;
        riserBars->setBounds (a.removeFromLeft (100).withTop (top));
        pumpDepth->setBounds (a.removeFromLeft (96).withTop (top));
        a.removeFromLeft (10);
        gateStrip->setBounds (a.withTrimmedTop (2));
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
