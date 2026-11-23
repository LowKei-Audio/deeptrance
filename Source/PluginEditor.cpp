#include "PluginEditor.h"

using namespace dt::ui;

// ============================================================================
// PresetBar
// ============================================================================
PresetBar::PresetBar (DeepTranceProcessor& p) : processor (p)
{
    for (auto* b : { &prev, &next, &name, &save, &slotA, &slotB, &copy, &dice }) addAndMakeVisible (*b);
    for (auto* b : { &slotA, &slotB }) { b->setClickingTogglesState (false); }
    slotA.setTooltip ("Sound A. Edit freely, then click B to compare; each keeps its own edits.");
    slotB.setTooltip ("Sound B. Starts as a copy of A the first time you switch to it.");
    copy.setTooltip ("Copy the sound you hear into the other slot (A to B, or B to A)");
    dice.setTooltip ("A new random sound from safe, musical settings. Levels and the mix are never touched.");
    slotA.onClick = [this] { processor.selectAB (0); timerCallback(); };
    slotB.onClick = [this] { processor.selectAB (1); timerCallback(); };
    copy.onClick  = [this] { processor.copyToOtherSlot(); };
    dice.onClick  = [this] { processor.randomize(); timerCallback(); };
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    name.setTooltip ("Browse presets");
    save.setTooltip ("Save the current sound as a user preset");
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (+1); };
    name.onClick = [this] { showMenu(); };
    save.onClick = [this] { saveAs(); };
    timerCallback();
    startTimerHz (5); // picks up preset changes made by the host
}

PresetBar::~PresetBar() { stopTimer(); }

void PresetBar::timerCallback()
{
    const auto n = processor.getPresets().currentPresetName();
    if (name.getButtonText() != n) name.setButtonText (n);
    slotA.setToggleState (processor.getABSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (processor.getABSlot() == 1, juce::dontSendNotification);
}

void PresetBar::paint (juce::Graphics&) {}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (58));
    r.removeFromRight (6);
    dice.setBounds (r.removeFromRight (72));
    r.removeFromRight (10);
    copy.setBounds (r.removeFromRight (52));
    r.removeFromRight (3);
    slotB.setBounds (r.removeFromRight (30));
    r.removeFromRight (3);
    slotA.setBounds (r.removeFromRight (30));
    r.removeFromRight (10);
    prev.setBounds (r.removeFromLeft (32));
    next.setBounds (r.removeFromRight (32));
    name.setBounds (r.reduced (4, 0));
}

void PresetBar::step (int delta)
{
    // One combined list: factory presets, then user presets.
    auto& pm = processor.getPresets();
    const auto users = pm.userPresets();
    const int nFactory = pm.numFactoryPresets(), total = nFactory + users.size();

    int current = pm.currentFactoryIndex();
    if (current < 0)
    {
        current = nFactory - 1; // unknown user preset: continue from the end of factory
        for (int i = 0; i < users.size(); ++i)
            if (users[i].getFileNameWithoutExtension() == pm.currentPresetName()) current = nFactory + i;
    }

    const int target = (current + delta + total) % total;
    if (target < nFactory) processor.setCurrentProgram (target);
    else                   processor.loadUserPreset (users[target - nFactory]);
    timerCallback();
}

void PresetBar::showMenu()
{
    auto& pm = processor.getPresets();
    juce::PopupMenu menu;

    // Factory, grouped by category ("Category / Name"); Init sits at the top.
    std::map<juce::String, juce::PopupMenu> categories;
    juce::StringArray order;
    for (int i = 0; i < pm.numFactoryPresets(); ++i)
    {
        const auto full = pm.factoryName (i);
        const auto cat = full.upToFirstOccurrenceOf (" / ", false, false);
        if (! full.contains (" / ")) { menu.addItem (i + 1, full, true, pm.currentFactoryIndex() == i); continue; }
        if (! order.contains (cat)) order.add (cat);
        categories[cat].addItem (i + 1, full.fromFirstOccurrenceOf (" / ", false, false), true, pm.currentFactoryIndex() == i);
    }
    for (const auto& c : order) menu.addSubMenu (c, categories[c]);

    menu.addSeparator();
    const auto users = pm.userPresets();
    juce::PopupMenu userMenu;
    for (int i = 0; i < users.size(); ++i)
        userMenu.addItem (10000 + i, users[i].getFileNameWithoutExtension(), true,
                          pm.currentFactoryIndex() < 0 && users[i].getFileNameWithoutExtension() == pm.currentPresetName());
    if (users.isEmpty()) userMenu.addItem (-1, "(none saved yet)", false);
    menu.addSubMenu ("User", userMenu);
    menu.addSeparator();
    menu.addItem (20000, "Save As...");
    menu.addItem (20001, "Open Presets Folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name),
                        [this, users] (int result)
                        {
                            if (result <= 0) return;
                            if (result == 20000) { saveAs(); return; }
                            if (result == 20001)
                            {
                                const auto dir = dt::PresetManager::userPresetDirectory();
                                dir.createDirectory();
                                dir.startAsProcess();
                                return;
                            }
                            if (result >= 10000) processor.loadUserPreset (users[result - 10000]);
                            else                 processor.setCurrentProgram (result - 1);
                            timerCallback();
                        });
}

void PresetBar::saveAs()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", processor.getPresets().currentPresetName().fromLastOccurrenceOf (" / ", false, false));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->setLookAndFeel (&getLookAndFeel());

    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto n = w->getTextEditorContents ("name");
        if (! safe->processor.getPresets().saveUser (n))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved",
                                                    "Could not save \"" + n + "\". Please use a different name.");
        safe->timerCallback();
    }), true);
}

// ============================================================================
// Editor
// ============================================================================
DeepTranceEditor::DeepTranceEditor (DeepTranceProcessor& p)
    : AudioProcessorEditor (p), processor (p), presetBar (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (presetBar);

    // One click switches views; the choice is saved with the session.
    for (auto* b : { &easyButton, &proButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        addAndMakeVisible (*b);
    }
    easyButton.setTooltip ("Performance view: XY pad, macros, one-click switches");
    proButton.setTooltip ("Every parameter, the gate editor and modulation");
    easyButton.onClick = [this] { if (easyButton.getToggleState()) showView (false); };
    proButton.onClick  = [this] { if (proButton.getToggleState())  showView (true); };

    setResizable (true, true);
    setResizeLimits (980, 720, 1800, 1400);   // 980: ten module switches stay readable
    setSize (1040, 780);   // 1.1: room for the performance row
    showView (processor.isProMode());
}

DeepTranceEditor::~DeepTranceEditor()
{
    view.reset();
    setLookAndFeel (nullptr);
}

void DeepTranceEditor::showView (bool pro)
{
    processor.setProMode (pro);
    easyButton.setToggleState (! pro, juce::dontSendNotification);
    proButton.setToggleState (pro, juce::dontSendNotification);

    view.reset();
    if (pro) view = std::make_unique<ProView> (processor);
    else     view = std::make_unique<EasyView> (processor);
    addAndMakeVisible (*view);
    resized();
}

void DeepTranceEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // Logo: wordmark with the accent gradient.
    const auto logo = juce::Rectangle<float> (16.0f, 6.0f, 220.0f, 30.0f);
    g.setGradientFill (juce::ColourGradient (colours::accent, logo.getX(), 0.0f, colours::accent2, logo.getRight(), 0.0f, false));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("DEEPTRANCE", logo, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("by LowKei  v" JucePlugin_VersionString, juce::Rectangle<float> (17.0f, 36.0f, 220.0f, 14.0f), juce::Justification::centredLeft);

    g.setColour (colours::outline);
    g.drawHorizontalLine (60, 12.0f, (float) getWidth() - 12.0f);
}

void DeepTranceEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 0);
    auto header = r.removeFromTop (60).reduced (0, 12);
    proButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (4);
    easyButton.setBounds (header.removeFromRight (70));
    header.removeFromLeft (240);
    header.removeFromRight (24);
    presetBar.setBounds (header.withSizeKeepingCentre (juce::jmin (700, header.getWidth()), header.getHeight()));

    r.removeFromTop (10);
    r.removeFromBottom (12);
    if (view) view->setBounds (r);
}
