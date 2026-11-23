#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params/ParameterLayout.h"

namespace
{
    const juce::Identifier kUiMode ("uiMode"); // "easy" | "pro"
    const juce::Identifier kCustomShape[2] = { "lfo1Custom", "lfo2Custom" };

    // Helpers for the binding table (plain function pointers: no allocation, no captures).
    float pct (float v) noexcept { return v * 0.01f; }
    bool  on  (float v) noexcept { return v >= 0.5f; }
    int   idx (float v) noexcept { return (int) std::lround (v); }
}

DeepTranceProcessor::DeepTranceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DeepTrance", dt::createParameterLayout()),
      presets (apvts)
{
    using E = dt::EngineParams;
    auto bind = [this] (const juce::String& id, void (*apply) (E&, float, int), int index = 0)
    {
        auto* v = apvts.getRawParameterValue (id);
        jassert (v != nullptr); // every ID used here must exist in the layout
        bindings.push_back ({ v, apply, index });
    };
    namespace P = dt::pid;

    bind (P::inputGain,    [] (E& e, float v, int) { e.inputGainDb = v; });
    bind (P::outputGain,   [] (E& e, float v, int) { e.outputGainDb = v; });
    bind (P::mix,          [] (E& e, float v, int) { e.mix = pct (v); });

    bind (P::macroGate,    [] (E& e, float v, int) { e.macros.gate = pct (v); });
    bind (P::macroSwirl,   [] (E& e, float v, int) { e.macros.swirl = pct (v); });
    bind (P::macroSpace,   [] (E& e, float v, int) { e.macros.space = pct (v); });
    bind (P::macroDrift,   [] (E& e, float v, int) { e.macros.drift = pct (v); });
    bind (P::buildUp,      [] (E& e, float v, int) { e.buildUp = pct (v); });
    bind (P::buildUpNoise, [] (E& e, float v, int) { e.buildUpNoise = pct (v); });
    bind (P::combo,        [] (E& e, float v, int) { e.combo = (dt::Combo) juce::jlimit (0, 2, idx (v)); });
    bind (P::comboAmount,  [] (E& e, float v, int) { e.comboAmount = pct (v); });
    bind (P::padThrow,     [] (E& e, float v, int) { e.pads.throwOn = on (v); });
    bind (P::padWash,      [] (E& e, float v, int) { e.pads.washOn = on (v); });
    bind (P::padDrop,      [] (E& e, float v, int) { e.pads.dropOn = on (v); });
    bind (P::padRiser,     [] (E& e, float v, int) { e.pads.riserOn = on (v); });
    bind (P::riserBars,    [] (E& e, float v, int) { e.pads.riserBars = idx (v); });

    bind (P::gateOn,      [] (E& e, float v, int) { e.gateOn = on (v); });
    bind (P::gateRate,    [] (E& e, float v, int) { e.gate.rateIndex = idx (v); });
    bind (P::gatePattern, [] (E& e, float v, int) { e.gate.pattern = idx (v); });
    bind (P::gateDepth,   [] (E& e, float v, int) { e.gate.depth = pct (v); });
    bind (P::gateLength,  [] (E& e, float v, int) { e.gate.length = pct (v); });
    bind (P::gateAttack,  [] (E& e, float v, int) { e.gate.attackMs = v; });
    bind (P::gateRelease, [] (E& e, float v, int) { e.gate.releaseMs = v; });
    bind (P::gateFilter,  [] (E& e, float v, int) { e.gate.filterAmount = pct (v); });
    bind (P::gateSwing,   [] (E& e, float v, int) { e.gate.swing = pct (v); });
    bind (P::gateStereo,  [] (E& e, float v, int) { e.gate.stereo = pct (v); });
    bind (P::gatePatternExt, [] (E& e, float v, int) { e.gate.patternExt = idx (v); });
    for (int i = 0; i < dt::TranceGate::kNumSteps; ++i)
        bind (dt::gateStepId (i), [] (E& e, float v, int k) { e.gate.steps[(size_t) k] = pct (v); }, i);

    bind (P::vowelOn,        [] (E& e, float v, int) { e.vowelOn = on (v); });
    bind (P::vowelMorph,     [] (E& e, float v, int) { e.vowel.morph = v; });
    bind (P::vowelLfoRate,   [] (E& e, float v, int) { e.vowel.lfoRate = idx (v); });
    bind (P::vowelLfoDepth,  [] (E& e, float v, int) { e.vowel.lfoDepth = pct (v); });
    bind (P::vowelResonance, [] (E& e, float v, int) { e.vowel.resonance = pct (v); });
    bind (P::vowelMix,       [] (E& e, float v, int) { e.vowel.mix = pct (v); });

    bind (P::swirlOn,       [] (E& e, float v, int) { e.swirlOn = on (v); });
    bind (P::swirlMode,     [] (E& e, float v, int) { e.swirl.mode = (dt::SwirlMode) juce::jlimit (0, 2, idx (v)); });
    bind (P::swirlRate,     [] (E& e, float v, int) { e.swirl.rateIndex = idx (v); });
    bind (P::swirlDepth,    [] (E& e, float v, int) { e.swirl.depth = pct (v); });
    bind (P::swirlFeedback, [] (E& e, float v, int) { e.swirl.feedback = pct (v); });
    bind (P::swirlStereo,   [] (E& e, float v, int) { e.swirl.stereoDeg = v; });
    bind (P::swirlXover,    [] (E& e, float v, int) { e.swirl.xoverHz = v; });
    bind (P::swirlLow,      [] (E& e, float v, int) { e.swirl.lowAmount = pct (v); });
    bind (P::swirlMix,      [] (E& e, float v, int) { e.swirl.mix = pct (v); });

    bind (P::driftOn,      [] (E& e, float v, int) { e.driftOn = on (v); });
    bind (P::driftAmount,  [] (E& e, float v, int) { e.drift.amount = pct (v); });
    bind (P::driftRate,    [] (E& e, float v, int) { e.drift.rateHz = v; });
    bind (P::driftFlutter, [] (E& e, float v, int) { e.drift.flutter = pct (v); });
    bind (P::driftStereo,  [] (E& e, float v, int) { e.drift.stereo = pct (v); });
    bind (P::driftMix,     [] (E& e, float v, int) { e.drift.mix = pct (v); });

    bind (P::acidOn,        [] (E& e, float v, int) { e.acidOn = on (v); });
    bind (P::acidCutoff,    [] (E& e, float v, int) { e.acid.cutoffHz = v; });
    bind (P::acidResonance, [] (E& e, float v, int) { e.acid.resonance = pct (v); });
    bind (P::acidEnvMod,    [] (E& e, float v, int) { e.acid.envMod = pct (v); });
    bind (P::acidDecay,     [] (E& e, float v, int) { e.acid.decayMs = v; });
    bind (P::acidDrive,     [] (E& e, float v, int) { e.acid.drive = pct (v); });
    bind (P::acidMix,       [] (E& e, float v, int) { e.acid.mix = pct (v); });

    bind (P::punchOn,      [] (E& e, float v, int) { e.punchOn = on (v); });
    bind (P::punchAttack,  [] (E& e, float v, int) { e.punch.attack = pct (v); });
    bind (P::punchSustain, [] (E& e, float v, int) { e.punch.sustain = pct (v); });
    bind (P::punchDuck,    [] (E& e, float v, int) { e.punch.duck = pct (v); });
    bind (P::punchDuckRel, [] (E& e, float v, int) { e.punch.duckRelMs = v; });

    bind (P::cloudOn,       [] (E& e, float v, int) { e.cloudOn = on (v); });
    bind (P::cloudSize,     [] (E& e, float v, int) { e.cloud.sizeMs = v; });
    bind (P::cloudDensity,  [] (E& e, float v, int) { e.cloud.density = v; });
    bind (P::cloudPosition, [] (E& e, float v, int) { e.cloud.positionMs = v; });
    bind (P::cloudSpray,    [] (E& e, float v, int) { e.cloud.spray = pct (v); });
    bind (P::cloudPitch,    [] (E& e, float v, int) { e.cloud.pitch = v; });
    bind (P::cloudJitter,   [] (E& e, float v, int) { e.cloud.jitter = pct (v); });
    bind (P::cloudShimmer,  [] (E& e, float v, int) { e.cloud.shimmer = pct (v); });
    bind (P::cloudStereo,   [] (E& e, float v, int) { e.cloud.stereo = pct (v); });
    bind (P::cloudMix,      [] (E& e, float v, int) { e.cloud.mix = pct (v); });
    bind (P::cloudFreeze,   [] (E& e, float v, int) { e.cloud.freeze = on (v); });

    bind (P::delayOn,       [] (E& e, float v, int) { e.delayOn = on (v); });
    bind (P::delaySync,     [] (E& e, float v, int) { e.delay.sync = on (v); });
    bind (P::delayTimeSync, [] (E& e, float v, int) { e.delay.syncIndex = idx (v); });
    bind (P::delayTimeMs,   [] (E& e, float v, int) { e.delay.timeMs = v; });
    bind (P::delayFeedback, [] (E& e, float v, int) { e.delay.feedback = pct (v); });
    bind (P::delayPitch,    [] (E& e, float v, int) { e.delay.pitch = v; });
    bind (P::delayTone,     [] (E& e, float v, int) { e.delay.toneHz = v; });
    bind (P::delayPingPong, [] (E& e, float v, int) { e.delay.pingPong = pct (v); });
    bind (P::delayMix,      [] (E& e, float v, int) { e.delay.mix = pct (v); });

    bind (P::abyssOn,       [] (E& e, float v, int) { e.abyssOn = on (v); });
    bind (P::abyssDecay,    [] (E& e, float v, int) { e.abyss.decaySec = v; });
    bind (P::abyssSize,     [] (E& e, float v, int) { e.abyss.size = pct (v); });
    bind (P::abyssDamping,  [] (E& e, float v, int) { e.abyss.damping = pct (v); });
    bind (P::abyssPreDelay, [] (E& e, float v, int) { e.abyss.preDelayMs = v; });
    bind (P::abyssMod,      [] (E& e, float v, int) { e.abyss.mod = pct (v); });
    bind (P::abyssShimmer,  [] (E& e, float v, int) { e.abyss.shimmer = pct (v); });
    bind (P::abyssWidth,    [] (E& e, float v, int) { e.abyss.width = pct (v); });
    bind (P::abyssMix,      [] (E& e, float v, int) { e.abyss.mix = pct (v); });
    bind (P::abyssFreeze,   [] (E& e, float v, int) { e.abyss.freeze = on (v); });

    bind (P::pumpOn,    [] (E& e, float v, int) { e.pumpOn = on (v); });
    bind (P::pumpRate,  [] (E& e, float v, int) { e.pump.rateIndex = idx (v); });
    bind (P::pumpDepth, [] (E& e, float v, int) { e.pump.depth = pct (v); });
    bind (P::pumpShape, [] (E& e, float v, int) { e.pump.shape = pct (v); });
    bind (P::pumpBand,  [] (E& e, float v, int) { e.pump.band = idx (v); });

    bind (P::outWidth,    [] (E& e, float v, int) { e.output.width = pct (v); });
    bind (P::outBassMono, [] (E& e, float v, int) { e.output.bassMonoHz = v < 40.0f ? 0.0f : v; });
    bind (P::outClip,     [] (E& e, float v, int) { e.output.clip = on (v); });

    for (int i = 0; i < 2; ++i)
    {
        bind (dt::lfoId (i, "shape"),     [] (E& e, float v, int k) { e.lfo[(size_t) k].shape = (dt::LfoShape) juce::jlimit (0, 7, idx (v)); }, i);
        bind (dt::lfoId (i, "sync"),      [] (E& e, float v, int k) { e.lfo[(size_t) k].sync = on (v); }, i);
        bind (dt::lfoId (i, "rate_sync"), [] (E& e, float v, int k) { e.lfo[(size_t) k].syncIndex = idx (v); }, i);
        bind (dt::lfoId (i, "rate_hz"),   [] (E& e, float v, int k) { e.lfo[(size_t) k].rateHz = v; }, i);
    }
    bind (P::envAttack,  [] (E& e, float v, int) { e.env.attackMs = v; });
    bind (P::envRelease, [] (E& e, float v, int) { e.env.releaseMs = v; });
    bind (P::envGain,    [] (E& e, float v, int) { e.env.gainDb = v; });

    for (int s = 0; s < dt::kModSlots; ++s)
    {
        bind (dt::modId (s, "source"), [] (E& e, float v, int k) { e.mod[(size_t) k].source = idx (v); }, s);
        bind (dt::modId (s, "dest"),   [] (E& e, float v, int k) { e.mod[(size_t) k].dest = idx (v); }, s);
        bind (dt::modId (s, "amount"), [] (E& e, float v, int k) { e.mod[(size_t) k].amount = pct (v); }, s);
    }

    // Every automatable parameter must be bound, or it would silently do nothing.
    jassert ((int) bindings.size() == getParameters().size());

    resetCustomShapes();
    apvts.state.setProperty (kUiMode, "easy", nullptr);
    // A new instance is current by definition: stamp the version (do not migrate it).
    apvts.state.setProperty (dt::kStateVersionId, dt::kStateVersion, nullptr);

    // After a host load: snap each parameter to its exact saved value (see PresetManager).
    stateGuard.onApplied = [this] { dt::PresetManager::syncParametersToState (apvts); };
}

bool DeepTranceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo in/out, or mono in/out (mono is processed as dual-mono and summed back).
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DeepTranceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock));
    updateEngineParams();
    engine.prepare (sampleRate);
    setLatencySamples (dt::DeepTranceEngine::getLatencySamples());
}

dt::TransportInfo DeepTranceProcessor::readTransport() const
{
    dt::TransportInfo t;
    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
        {
            if (const auto bpm = pos->getBpm())         t.bpm = *bpm;
            if (const auto ppq = pos->getPpqPosition()) { t.ppqAtBlockStart = *ppq; t.hasPpq = true; }
            t.isPlaying = pos->getIsPlaying();
        }
    return t;
}

void DeepTranceProcessor::updateEngineParams() noexcept
{
    dt::EngineParams p;
    for (const auto& b : bindings)
        b.apply (p, b.value->load (std::memory_order_relaxed), b.index);
    for (size_t l = 0; l < 2; ++l)
        for (size_t i = 0; i < (size_t) dt::kCustomPoints; ++i)
            p.lfo[l].custom[i] = customShapes[l][i].load (std::memory_order_relaxed);
    engine.setParams (p);
}

void DeepTranceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    updateEngineParams();
    const auto transport = readTransport();

    if (buffer.getNumChannels() >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, transport);
        return;
    }

    // Mono: process as dual-mono, return the average. Grow scratch only if a host
    // exceeds its announced block size (rare, but some do).
    if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, false, true);
    auto* l = buffer.getWritePointer (0);
    auto* r = monoScratch.getWritePointer (0);
    std::copy (l, l + n, r);
    engine.process (l, r, n, transport);
    for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
}

juce::AudioProcessorEditor* DeepTranceProcessor::createEditor()
{
    return new DeepTranceEditor (*this);
}

// --- Programs --------------------------------------------------------------------
int DeepTranceProcessor::getNumPrograms()                  { return presets.numFactoryPresets(); }
int DeepTranceProcessor::getCurrentProgram()               { return juce::jmax (0, presets.currentFactoryIndex()); }
const juce::String DeepTranceProcessor::getProgramName (int i) { return presets.factoryName (i); }

void DeepTranceProcessor::setCurrentProgram (int index)
{
    // A whole new sound: the engine fades to dry, swaps every setting at once and fades
    // back in, so a preset never clicks or plays half of each sound.
    lowkei::SoundChangeGate::Scope change (engine.soundGate);
    presets.loadFactory (index);
    resetCustomShapes();
    for (int l = 0; l < 2; ++l)
        for (int i = 0; i < dt::kCustomPoints; ++i)
            if (const auto v = presets.factoryCustomPoint (index, l, i)) setCustomPoint (l, i, *v);
}

bool DeepTranceProcessor::loadUserPreset (const juce::File& file)
{
    lowkei::SoundChangeGate::Scope change (engine.soundGate);
    if (! presets.loadUser (file)) return false;
    loadCustomShapesFromState();
    return true;
}

// --- A/B compare and Randomize -------------------------------------------------------
void DeepTranceProcessor::applySound (const juce::ValueTree& source)
{
    lowkei::SoundChangeGate::Scope change (engine.soundGate);
    auto tree = source.createCopy();
    tree.setProperty (kUiMode, apvts.state.getProperty (kUiMode), nullptr);   // the view is not part of a sound
    stateGuard.setState (tree);
    loadCustomShapesFrom (tree);
}

void DeepTranceProcessor::selectAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abActive) return;
    abSlots[(size_t) abActive] = stateGuard.getState();
    if (! abSlots[(size_t) slot].isValid())
        abSlots[(size_t) slot] = abSlots[(size_t) abActive].createCopy();   // first visit: same sound
    abActive = slot;
    applySound (abSlots[(size_t) slot]);
}

void DeepTranceProcessor::copyToOtherSlot()
{
    abSlots[(size_t) (1 - abActive)] = stateGuard.getState();
}

void DeepTranceProcessor::randomize()
{
    lowkei::SoundChangeGate::Scope change (engine.soundGate);
    juce::Random rng;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
    };
    auto range  = [&rng] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto chance = [&rng] (float p) { return rng.nextFloat() < p ? 1.0f : 0.0f; };
    auto pick   = [&rng] (std::initializer_list<float> v) { return *(v.begin() + rng.nextInt ((int) v.size())); };

    // Start from Init, so nothing from the last sound (mod matrix, combos, pads) lingers.
    presets.loadFactory (0);
    resetCustomShapes();
    namespace P = dt::pid;

    set (P::swirlOn, chance (0.7f));   set (P::swirlMode, pick ({ 0, 0, 1, 2 }));
    set (P::swirlRate, pick ({ 3, 4, 5, 6, 7 }));   set (P::swirlDepth, range (20, 80));
    set (P::swirlFeedback, range (10, 70));          set (P::swirlMix, range (15, 55));

    set (P::driftOn, chance (0.6f));   set (P::driftAmount, range (10, 60));   set (P::driftMix, range (20, 50));

    if (chance (0.45f) > 0.0f)
    {
        set (P::gateOn, 1);   set (P::gateRate, pick ({ 0, 1, 1, 3 }));
        if (chance (0.5f) > 0.0f) set (P::gatePatternExt, (float) rng.nextInt ({ 1, 9 }));
        else                      set (P::gatePattern, (float) rng.nextInt ({ 1, 9 }));
        set (P::gateLength, range (40, 90));   set (P::gateFilter, chance (0.5f) * range (0, 50));
        set (P::gateSwing, chance (0.3f) * range (10, 40));   set (P::gateStereo, chance (0.25f) * range (50, 100));
    }

    if (chance (0.2f) > 0.0f) { set (P::vowelOn, 1); set (P::vowelMorph, range (0, 4)); set (P::vowelLfoDepth, range (0, 80)); set (P::vowelMix, range (50, 90)); }
    if (chance (0.15f) > 0.0f) { set (P::acidOn, 1); set (P::acidCutoff, range (300, 2500)); set (P::acidResonance, range (40, 85)); set (P::acidDrive, range (20, 60)); }
    if (chance (0.2f) > 0.0f) { set (P::cloudOn, 1); set (P::cloudSize, range (40, 200)); set (P::cloudDensity, range (10, 50)); set (P::cloudMix, range (25, 60)); set (P::cloudShimmer, chance (0.4f) * range (0, 40)); }

    set (P::delayOn, chance (0.75f));   set (P::delayTimeSync, pick ({ 3, 5, 6, 6, 8 }));
    set (P::delayFeedback, range (20, 65));   set (P::delayMix, range (8, 30));
    set (P::delayPitch, pick ({ 0, 0, 0, 0, 12, 7, -12, 5 }));

    set (P::abyssOn, chance (0.85f));   set (P::abyssDecay, range (2, 14));   set (P::abyssSize, range (40, 95));
    set (P::abyssMix, range (15, 45));   set (P::abyssDamping, range (25, 65));   set (P::abyssShimmer, chance (0.3f) * range (0, 40));

    if (chance (0.35f) > 0.0f)
    {
        set (P::pumpOn, 1);   set (P::pumpRate, pick ({ 1, 1, 1, 2 }));   set (P::pumpDepth, range (30, 75));
        set (P::pumpShape, range (30, 70));   set (P::pumpBand, pick ({ 0, 1 }));
    }
    presets.setCustomName ("Random");
}

// --- View mode and custom LFO shapes ------------------------------------------------
bool DeepTranceProcessor::isProMode() const { return apvts.state.getProperty (kUiMode).toString() == "pro"; }
void DeepTranceProcessor::setProMode (bool pro) { apvts.state.setProperty (kUiMode, pro ? "pro" : "easy", nullptr); }

float DeepTranceProcessor::getCustomPoint (int lfo, int point) const noexcept
{
    return customShapes[(size_t) juce::jlimit (0, 1, lfo)][(size_t) juce::jlimit (0, dt::kCustomPoints - 1, point)].load();
}

void DeepTranceProcessor::setCustomPoint (int lfo, int point, float value)
{
    customShapes[(size_t) juce::jlimit (0, 1, lfo)][(size_t) juce::jlimit (0, dt::kCustomPoints - 1, point)]
        .store (juce::jlimit (0.0f, 1.0f, value));
    syncCustomShapesToState();
}

void DeepTranceProcessor::resetCustomShapes()
{
    const dt::LfoParams defaults;
    for (auto& shape : customShapes)
        for (size_t i = 0; i < (size_t) dt::kCustomPoints; ++i)
            shape[i].store (defaults.custom[i]);
    syncCustomShapesToState();
}

void DeepTranceProcessor::syncCustomShapesToState()
{
    for (int l = 0; l < 2; ++l)
    {
        juce::StringArray pts;
        for (const auto& p : customShapes[(size_t) l]) pts.add (juce::String (p.load(), 4));
        apvts.state.setProperty (kCustomShape[l], pts.joinIntoString (","), nullptr);
    }
}

void DeepTranceProcessor::loadCustomShapesFrom (const juce::ValueTree& root)
{
    for (int l = 0; l < 2; ++l)
    {
        const auto pts = juce::StringArray::fromTokens (root.getProperty (kCustomShape[l]).toString(), ",", "");
        if (pts.size() != dt::kCustomPoints) continue; // missing/old state: keep current shape
        for (int i = 0; i < dt::kCustomPoints; ++i)
            customShapes[(size_t) l][(size_t) i].store (juce::jlimit (0.0f, 1.0f, pts[i].getFloatValue()));
    }
}

// --- State -------------------------------------------------------------------------
void DeepTranceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateGuard.getState().createXml())   // safe on any thread (see StateGuard.h)
        copyXmlToBinary (*xml, destData);
}

void DeepTranceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // A host loading a project or a preset (Serato's own browser loads this way while
    // playing) is a sound change like any preset.
    lowkei::SoundChangeGate::Scope change (engine.soundGate);
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (stateGuard.stateType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            dt::migrateState (tree);   // 1.0.x sessions: every 1.1 feature neutral
            // Parameters at once; apvts.state itself on the message thread (onApplied).
            stateGuard.setState (tree);
            loadCustomShapesFrom (tree);   // atomics: safe on any thread
            // Tell the host every parameter may have changed, so it re-reads
            // them (CLAP: params rescan; VST3: restart with param values changed).
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeepTranceProcessor();
}
