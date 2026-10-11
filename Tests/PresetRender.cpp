// Renders every factory preset through the real plugin processor (parameter
// layout, bindings, preset loading, engine) and checks the result is sane.
// Usage: DeepTranceRender [outputDir]   (writes one WAV per preset when given)

#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    /** 8 bars of a detuned saw pad with a plucky 1/8 rhythm on top, 138 BPM. */
    juce::AudioBuffer<float> sourceMaterial (double sr, double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (sr * seconds));
        b.clear();
        const double notes[] = { 220.0, 261.63, 329.63, 440.0 };
        const double eighth = 60.0 / 138.0 / 2.0;
        for (double f : notes)
        {
            double ph = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr, local = std::fmod (t, eighth);
                const float env = (float) (0.4 + 0.6 * std::exp (-local * 18.0));
                const float v = 0.07f * env * (float) (2.0 * ph - 1.0);
                b.addSample (0, i, v);
                b.addSample (1, i, v);
                ph += f / sr; ph -= std::floor (ph);
            }
        }
        return b;
    }

    struct Head final : juce::AudioPlayHead
    {
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (138.0);
            p.setPpqPosition (ppq);
            p.setIsPlaying (true);
            return p;
        }
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const int block = 512;
    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File();
    if (outDir != juce::File()) outDir.createDirectory();

    DeepTranceProcessor proc;
    Head head;
    proc.setPlayHead (&head);
    int failures = 0;

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        proc.prepareToPlay (sr, block);
        head.ppq = 0.0;

        auto buffer = sourceMaterial (sr, 14.0);
        const float inRms = buffer.getRMSLevel (0, 0, buffer.getNumSamples());
        juce::MidiBuffer midi;
        for (int off = 0; off < buffer.getNumSamples(); off += block)
        {
            const int n = juce::jmin (block, buffer.getNumSamples() - off);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, off, n);
            proc.processBlock (view, midi);
            head.ppq += n * 138.0 / 60.0 / sr;
        }

        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
        const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
        const float rms  = buffer.getRMSLevel (0, 0, buffer.getNumSamples());
        const double gainDb = juce::Decibels::gainToDecibels (rms / inRms);

        // A preset must make sound, stay finite, and land within a sensible loudness window.
        const bool ok = finite && peak < 4.0f && gainDb > -18.0 && gainDb < 12.0;
        std::printf ("%-2s %-42s peak %5.2f  level %+5.1f dB\n", ok ? "ok" : "!!",
                     proc.getProgramName (p).toRawUTF8(), (double) peak, gainDb);
        if (! ok) ++failures;

        if (outDir != juce::File())
        {
            const auto name = juce::File::createLegalFileName (juce::String (p).paddedLeft ('0', 2) + " " + proc.getProgramName (p).replace (" / ", " - "));
            juce::WavAudioFormat wav;
            if (auto stream = outDir.getChildFile (name + ".wav").createOutputStream())
            {
                stream->setPosition (0); stream->truncate();
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
                {
                    stream.release(); // writer owns it now
                    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
                }
            }
        }
    }

    std::printf (failures == 0 ? "\nALL %d PRESETS OK\n" : "\n%d PRESET(S) FAILED\n", failures == 0 ? proc.getNumPrograms() : failures);
    return failures == 0 ? 0 : 1;
}
