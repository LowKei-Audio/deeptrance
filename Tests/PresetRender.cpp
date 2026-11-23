// Renders every factory preset through the real plugin processor (parameter
// layout, bindings, preset loading, engine) and checks the result is sane.
// Usage: DeepTranceRender [outputDir]   (writes one WAV per preset when given)
//        DeepTranceRender --static [v]   the static check alone: loop wraps, restarts
//                                        and sound changes while playing (v: every event)
//        DeepTranceRender --compat     the 1.0.3 compatibility check alone
// The default run ends with the static and compatibility checks too.

#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstring>

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

    // ---- Static (Serato report, Nov 2026) ---------------------------------------
    // Clicks on loop wraps, transport restarts and sound changes. A held, harmonically
    // rich tone plays through the real processor with a scripted host playhead; a click
    // shows up as a spike in the fourth difference of the output (steps and kinks stand
    // out of it at any level, a smooth sound's own curve does not), measured against
    // the sharpest moment each sound makes on its own.
    struct StaticRun
    {
        static constexpr double fs = 48000.0;
        static constexpr int block = 512;
        struct ScriptedHead final : juce::AudioPlayHead
        {
            double ppq = 0.0, bpm = 138.0; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo p;
                p.setBpm (bpm);
                p.setPpqPosition (ppq);
                p.setIsPlaying (playing);
                return p;
            }
        };
        DeepTranceProcessor& p;
        ScriptedHead head;
        std::vector<float> l, r;
        double phase = 0.0;
        explicit StaticRun (DeepTranceProcessor& proc) : p (proc) { p.setPlayHead (&head); }
        ~StaticRun() { p.setPlayHead (nullptr); }

        void play (int blocks, bool advance = true)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < block; ++i)
                {
                    const double fadeIn = std::min (1.0, (double) (l.size() + (size_t) i) / 2400.0);
                    const float v = (float) (0.3 * fadeIn * (std::sin (phase) + 0.4 * std::sin (2 * phase) + 0.2 * std::sin (3 * phase)));
                    phase += 2.0 * juce::MathConstants<double>::pi * 233.08 / fs;
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                p.processBlock (buf, midi);
                l.insert (l.end(), buf.getReadPointer (0), buf.getReadPointer (0) + block);
                r.insert (r.end(), buf.getReadPointer (1), buf.getReadPointer (1) + block);
                if (advance && head.playing) head.ppq += block * head.bpm / (60.0 * fs);
            }
        }
        float d (int i) const
        {
            if (i < 4 || i >= (int) l.size()) return 0.0f;
            auto d4 = [i] (const std::vector<float>& x) { const auto k = (size_t) i; return std::abs (x[k] - 4 * x[k - 1] + 6 * x[k - 2] - 4 * x[k - 3] + x[k - 4]); };
            return std::max (d4 (l), d4 (r));
        }
        // The event at `at` (+ `after` samples) against the sound's sharpest moments
        // before and after it. Floor: a step under ~-75 dBFS under a loud tone is no click.
        float spike (int at, int after, int refBefore, int refAfter) const
        {
            float ref = 1.0e-3f, m = 0.0f;
            for (int i = at - refBefore; i < at - 2 * block; ++i) ref = std::max (ref, d (i));
            for (int i = at + after + block; i < at + after + refAfter; ++i) ref = std::max (ref, d (i));
            for (int i = at - 16; i < at + after; ++i) m = std::max (m, d (i));
            return m / ref;
        }
        bool finite() const
        {
            for (size_t i = 0; i < l.size(); ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) return false;
            return true;
        }
    };


    // ---- 1.0.3 compatibility ----------------------------------------------------
    // Tests/compat holds session states saved by DeepTrance 1.0.3 (every factory preset
    // plus hand-edited ones) and a fingerprint of each one's render. 1.1 must load each
    // state with every new feature neutral, and (where the fingerprints were made: GCC,
    // x86-64) render it bit for bit.
    uint64_t fnv1a (const juce::AudioBuffer<float>& b)
    {
        uint64_t h = 1469598103934665603ull;
        for (int i = 0; i < b.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                const float v = b.getSample (ch, i);
                uint32_t bits; std::memcpy (&bits, &v, 4);
                for (int k = 0; k < 4; ++k) { h ^= (bits >> (8 * k)) & 0xffu; h *= 1099511628211ull; }
            }
        return h;
    }

    uint64_t renderState (const juce::MemoryBlock& state)
    {
        const double sr = 48000.0;
        const int block = 512;
        DeepTranceProcessor proc;
        proc.setStateInformation (state.getData(), (int) state.getSize());
        proc.prepareToPlay (sr, block);
        Head head;
        proc.setPlayHead (&head);
        auto buffer = sourceMaterial (sr, 6.0);
        juce::MidiBuffer midi;
        for (int off = 0; off < buffer.getNumSamples(); off += block)
        {
            const int n = juce::jmin (block, buffer.getNumSamples() - off);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, off, n);
            proc.processBlock (view, midi);
            head.ppq += n * 138.0 / 60.0 / sr;
        }
        proc.setPlayHead (nullptr);
        return fnv1a (buffer);
    }

    /** Hand-edited states, made on top of a factory preset by setting parameters. */
    struct Edit { const char* name; int preset; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Edit>& compatEdits()
    {
        static const std::vector<Edit> e {
            { "edit-all-on", 0, { { "gate_on", 1 }, { "vowel_on", 1 }, { "acid_on", 1 }, { "punch_on", 1 }, { "cloud_on", 1 },
                                 { "swirl_mode", 2 }, { "mix", 85 }, { "output_gain", -2 } } },
            { "edit-mod-matrix", 3, { { "mod1_source", 1 }, { "mod1_dest", 10 }, { "mod1_amount", 60 },
                                     { "mod2_source", 3 }, { "mod2_dest", 23 }, { "mod2_amount", -40 }, { "acid_on", 1 },
                                     { "lfo1_shape", 6 }, { "lfo1_rate_sync", 4 } } },
            { "edit-gate-custom", 2, { { "gate_pattern", 0 }, { "gate_step_03", 20 }, { "gate_step_07", 0 }, { "gate_step_12", 55 },
                                      { "gate_filter", 60 }, { "buildup", 40 }, { "combo", 1 }, { "combo_amount", 50 } } },
            { "edit-macros", 10, { { "macro_gate", 80 }, { "macro_swirl", 10 }, { "macro_space", 95 }, { "macro_drift", 70 },
                                  { "input_gain", 4 } } },
        };
        return e;
    }

    juce::File compatDir() { return juce::File (DT_COMPAT_DIR); }

    /** Writes Tests/compat from the current build (run once, on 1.0.3). */
    int generateCompat()
    {
        auto dir = compatDir();
        dir.createDirectory();
        juce::StringArray lines;
        auto save = [&] (const juce::String& name, DeepTranceProcessor& p)
        {
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
                xml->writeTo (dir.getChildFile (name + ".xml"));
            lines.add (name + " " + juce::String::toHexString ((juce::int64) renderState (mb)));
        };
        const int n = DeepTranceProcessor().getNumPrograms();
        for (int k = 0; k < n; ++k)
        {
            DeepTranceProcessor p;
            p.setCurrentProgram (k);
            save ("preset-" + juce::String (k).paddedLeft ('0', 2), p);
        }
        for (const auto& e : compatEdits())
        {
            DeepTranceProcessor p;
            p.setCurrentProgram (e.preset);
            for (const auto& [id, v] : e.values)
                if (auto* rp = p.getState().getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            save (e.name, p);
        }
        dir.getChildFile ("fingerprints.txt").replaceWithText (lines.joinIntoString ("\n") + "\n");
        std::printf ("wrote %d states to %s\n", lines.size(), dir.getFullPathName().toRawUTF8());
        return 0;
    }

    /** Every 1.1 parameter at its neutral (1.0-equivalent) value? Names the first that is not. */
    juce::String firstNonNeutral (DeepTranceProcessor& p)
    {
        const std::pair<const char*, float> neutral[] = {
            { "gate_swing", 0 }, { "gate_stereo", 0 }, { "gate_pattern_ext", 0 },
            { "pump_on", 0 }, { "out_width", 100 }, { "out_bass_mono", 0 }, { "out_clip", 0 },
            { "pad_throw", 0 }, { "pad_wash", 0 }, { "pad_drop", 0 }, { "pad_riser", 0 } };
        for (const auto& [id, v] : neutral)
        {
            auto* rp = p.getState().getParameter (id);
            if (rp == nullptr) return juce::String (id) + " (missing)";
            if (std::abs (rp->convertFrom0to1 (rp->getValue()) - v) > 1.0e-4f) return id;
        }
        return {};
    }

    int runCompatCheck()
    {
        std::printf ("\n1.0.3 compatibility: saved sessions load with every new feature neutral\n");
        int problems = 0;
        {
            // A new instance has the safety clip on; a 1.1 session saved with it on keeps it.
            DeepTranceProcessor fresh;
            auto* clip = fresh.getState().getParameter ("out_clip");
            if (clip == nullptr || clip->getValue() < 0.5f) { ++problems; std::printf ("    new instance: Safety Clip should be on\n"); }
            juce::MemoryBlock mb;
            fresh.getStateInformation (mb);
            DeepTranceProcessor reloaded;
            reloaded.setStateInformation (mb.getData(), (int) mb.getSize());
            if (reloaded.getState().getParameter ("out_clip")->getValue() < 0.5f) { ++problems; std::printf ("    1.1 session: Safety Clip was switched off on reload\n"); }
        }
        const auto lines = juce::StringArray::fromLines (compatDir().getChildFile ("fingerprints.txt").loadFileAsString().trim());
        int bad = 0, checked = 0;
        for (const auto& line : lines)
        {
            const auto name = line.upToFirstOccurrenceOf (" ", false, false);
            const auto want = (uint64_t) line.fromFirstOccurrenceOf (" ", false, false).getHexValue64();
            const auto xml = juce::XmlDocument::parse (compatDir().getChildFile (name + ".xml"));
            if (xml == nullptr) { ++bad; std::printf ("    %s: missing\n", name.toRawUTF8()); continue; }
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*xml, mb);
            ++checked;
            {
                DeepTranceProcessor loaded;
                loaded.setStateInformation (mb.getData(), (int) mb.getSize());
                if (const auto id = firstNonNeutral (loaded); id.isNotEmpty())
                {
                    ++bad;
                    std::printf ("    %-24s loads with %s not neutral\n", name.toRawUTF8(), id.toRawUTF8());
                }
            }
            const auto got = renderState (mb);
#if defined(__GNUC__) && ! defined(__clang__) && defined(__x86_64__)
            if (got != want) { ++bad; std::printf ("    %-24s DIFFERS from 1.0.3\n", name.toRawUTF8()); }
#else
            juce::ignoreUnused (got, want);
#endif
        }
        if (checked == 0) { ++bad; std::printf ("    no compatibility states found\n"); }
        bad += problems;
        std::printf (bad == 0 ? "COMPAT OK (%d states)\n" : "COMPAT: %d PROBLEM(S)\n", bad == 0 ? checked : bad);
        return bad;
    }

    std::unique_ptr<DeepTranceProcessor> makeStaticProcessor()
    {
        auto p = std::make_unique<DeepTranceProcessor>();
        p->setPlayConfigDetails (2, 2, StaticRun::fs, StaticRun::block);
        p->prepareToPlay (StaticRun::fs, StaticRun::block);
        return p;
    }

    int runStaticCheck (bool verbose)
    {
        std::printf ("\nStatic: loop wraps, restarts and sound changes\n");
        constexpr float kChangeLimit = 1.25f, kWrapLimit = 2.5f;
        float worstWrap = 0, worstRestart = 0, worstChange = 0;
        int bad = 0;
        auto report = [&] (const juce::String& what, float ratio, float limit, float& worst)
        {
            worst = std::max (worst, ratio);
            const bool ok = ratio < limit;
            if (! ok) ++bad;
            if (! ok || verbose) std::printf ("    %-56s %8.2f %s\n", what.toRawUTF8(), (double) ratio, ok ? "ok" : "CLICK");
        };

        // 1. Loop wraps and restarts over sound, for every factory preset. The loop is
        //    4 beats; at 138 BPM that is 1.74 s.
        const int numPresets = makeStaticProcessor()->getNumPrograms();
        const int loopBlocks = (int) (4 * 60.0 / 138.0 * StaticRun::fs) / StaticRun::block;
        for (int k = 0; k < numPresets; ++k)
            for (int kind = 0; kind < 3; ++kind)   // 0 wrap, 1 wrap with a "stopped" block, 2 restart
            {
                auto p = makeStaticProcessor();
                p->setCurrentProgram (k);
                p->prepareToPlay (StaticRun::fs, StaticRun::block);
                StaticRun run (*p);
                run.play (loopBlocks);
                const int at = (int) run.l.size();
                if (kind == 0) run.head.ppq = 0.0;
                if (kind == 1) { run.head.playing = false; run.play (1, false); run.head.playing = true; run.head.ppq = 0.0; }
                if (kind == 2) { run.head.playing = false; run.play (8, false); run.head.playing = true; run.head.ppq = 1.0; }
                run.play (loopBlocks);
                if (! run.finite()) { ++bad; std::printf ("    %s: NaN/Inf\n", p->getProgramName (k).toRawUTF8()); continue; }
                const int window = kind == 2 ? 10 * StaticRun::block : 3 * StaticRun::block;
                const int ref = loopBlocks * StaticRun::block * 3 / 4;
                static const char* kinds[] = { "wrap", "wrap + stopped block", "restart" };
                report (p->getProgramName (k) + ": " + kinds[kind], run.spike (at, window, ref, ref), kWrapLimit,
                        kind <= 1 ? worstWrap : worstRestart);
            }

        // 2. Sound changes while playing: every factory preset (the host program list),
        //    and host state loads (Serato's own preset browser loads that way).
        {
            auto p = makeStaticProcessor();
            StaticRun run (*p);
            const int segment = 112;   // ~1.2 s per sound
            std::vector<std::pair<int, juce::String>> changes;
            auto change = [&] (const juce::String& name, const std::function<void()>& f)
            {
                changes.push_back ({ (int) run.l.size(), name });
                f();
                run.play (segment);
            };
            run.play (segment);
            for (int k = 1; k < numPresets; ++k)
                change ("preset " + p->getProgramName (k), [&] { p->setCurrentProgram (k); });
            change ("preset " + p->getProgramName (0), [&] { p->setCurrentProgram (0); });
            for (int k : { 3, numPresets / 2, numPresets - 1 })
            {
                auto other = makeStaticProcessor();
                other->setCurrentProgram (k);
                juce::MemoryBlock mb;
                other->getStateInformation (mb);
                change ("host state load (" + other->getProgramName (k) + ")", [&, mb] { p->setStateInformation (mb.getData(), (int) mb.getSize()); });
            }
            if (! run.finite()) { ++bad; std::printf ("    NaN/Inf in sound changes\n"); }
            const int seg = segment * StaticRun::block;
            for (const auto& [at, name] : changes)
                report (name, run.spike (at, 8 * StaticRun::block, seg - 8 * StaticRun::block, seg - 10 * StaticRun::block), kChangeLimit, worstChange);
        }

        std::printf ("    worst: wrap %.2f, restart %.2f, sound change %.2f\n", (double) worstWrap, (double) worstRestart, (double) worstChange);
        std::printf (bad == 0 ? "STATIC OK\n" : "STATIC: %d EVENT(S) OVER THE LIMIT\n", bad);
        return bad;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--compat-generate")   // run once, on 1.0.3
        return generateCompat();
    if (argc > 1 && juce::String (argv[1]) == "--compat")
        return runCompatCheck() == 0 ? 0 : 1;
    if (argc > 1 && juce::String (argv[1]) == "--static")   // the static check alone ("v": every event)
        return runStaticCheck (argc > 2 && juce::String (argv[2]) == "v") == 0 ? 0 : 1;
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
    proc.setPlayHead (nullptr);
    const int clicks = runStaticCheck (false);
    const int compat = runCompatCheck();
    return failures == 0 && clicks == 0 && compat == 0 ? 0 : 1;
}
