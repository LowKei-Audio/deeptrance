#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params/ParameterLayout.h"

namespace
{
    const juce::Identifier kUiMode ("uiMode"); // "easy" | "pro"
}

DeepTranceProcessor::DeepTranceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DeepTrance", dt::createParameterLayout())
{
    auto bind = [this] (Raw& r, const juce::String& id)
    {
        r.p = apvts.getRawParameterValue (id);
        jassert (r.p != nullptr); // every ID used here must exist in the layout
    };

    namespace P = dt::pid;
    bind (inputGain, P::inputGain);  bind (outputGain, P::outputGain);  bind (mix, P::mix);

    bind (gateOn, P::gateOn);         bind (gateRate, P::gateRate);       bind (gatePattern, P::gatePattern);
    bind (gateDepth, P::gateDepth);   bind (gateLength, P::gateLength);   bind (gateAttack, P::gateAttack);
    bind (gateRelease, P::gateRelease); bind (gateFilter, P::gateFilter);
    for (int i = 0; i < 16; ++i) bind (gateSteps[(size_t) i], dt::gateStepId (i));

    bind (vowelOn, P::vowelOn);       bind (vowelMorph, P::vowelMorph);   bind (vowelLfoRate, P::vowelLfoRate);
    bind (vowelLfoDepth, P::vowelLfoDepth); bind (vowelResonance, P::vowelResonance); bind (vowelMix, P::vowelMix);

    bind (swirlOn, P::swirlOn);       bind (swirlMode, P::swirlMode);     bind (swirlRate, P::swirlRate);
    bind (swirlDepth, P::swirlDepth); bind (swirlFeedback, P::swirlFeedback); bind (swirlStereo, P::swirlStereo);
    bind (swirlXover, P::swirlXover); bind (swirlLow, P::swirlLow);       bind (swirlMix, P::swirlMix);

    bind (driftOn, P::driftOn);       bind (driftAmount, P::driftAmount); bind (driftRate, P::driftRate);
    bind (driftFlutter, P::driftFlutter); bind (driftStereo, P::driftStereo); bind (driftMix, P::driftMix);

    apvts.state.setProperty (kUiMode, "easy", nullptr);
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
    auto pct = [] (const Raw& r) { return r.get() * 0.01f; };
    auto on  = [] (const Raw& r) { return r.get() >= 0.5f; };
    auto idx = [] (const Raw& r) { return (int) std::lround (r.get()); };

    p.inputGainDb  = inputGain.get();
    p.outputGainDb = outputGain.get();
    p.mix          = pct (mix);

    p.gateOn = on (gateOn);
    p.gate.rateIndex    = idx (gateRate);
    p.gate.pattern      = idx (gatePattern);
    p.gate.depth        = pct (gateDepth);
    p.gate.length       = pct (gateLength);
    p.gate.attackMs     = gateAttack.get();
    p.gate.releaseMs    = gateRelease.get();
    p.gate.filterAmount = pct (gateFilter);
    for (size_t i = 0; i < 16; ++i) p.gate.steps[i] = pct (gateSteps[i]);

    p.vowelOn = on (vowelOn);
    p.vowel.morph     = vowelMorph.get();
    p.vowel.lfoRate   = idx (vowelLfoRate);
    p.vowel.lfoDepth  = pct (vowelLfoDepth);
    p.vowel.resonance = pct (vowelResonance);
    p.vowel.mix       = pct (vowelMix);

    p.swirlOn = on (swirlOn);
    p.swirl.mode      = (dt::SwirlMode) juce::jlimit (0, 2, idx (swirlMode));
    p.swirl.rateIndex = idx (swirlRate);
    p.swirl.depth     = pct (swirlDepth);
    p.swirl.feedback  = pct (swirlFeedback);
    p.swirl.stereoDeg = swirlStereo.get();
    p.swirl.xoverHz   = swirlXover.get();
    p.swirl.lowAmount = pct (swirlLow);
    p.swirl.mix       = pct (swirlMix);

    p.driftOn = on (driftOn);
    p.drift.amount  = pct (driftAmount);
    p.drift.rateHz  = driftRate.get();
    p.drift.flutter = pct (driftFlutter);
    p.drift.stereo  = pct (driftStereo);
    p.drift.mix     = pct (driftMix);

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

bool DeepTranceProcessor::isProMode() const
{
    return apvts.state.getProperty (kUiMode).toString() == "pro";
}

void DeepTranceProcessor::setProMode (bool pro)
{
    apvts.state.setProperty (kUiMode, pro ? "pro" : "easy", nullptr);
}

void DeepTranceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DeepTranceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeepTranceProcessor();
}
