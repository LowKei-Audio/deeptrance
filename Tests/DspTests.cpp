// DeepTrance DSP tests. Plain C++ (no framework) so they build anywhere.
// Run: DeepTranceTests            -> runs all checks, exits non-zero on failure
//      DeepTranceTests --render D -> also writes demo WAVs into directory D

#include "DSP/DeepTranceEngine.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace dt;

namespace
{
int failures = 0;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } \
    } while (0)

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

// --- signal generators -----------------------------------------------------
Stereo sine (double sr, double hz, double seconds, float amp = 0.5f)
{
    Stereo s ((size_t) (sr * seconds));
    for (size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = amp * (float) std::sin (kTwoPiD * hz * (double) i / sr);
    return s;
}

Stereo noise (double sr, double seconds, float amp = 0.3f, uint32_t seed = 1)
{
    Stereo s ((size_t) (sr * seconds));
    Random a (seed), b (seed * 7 + 3);
    for (size_t i = 0; i < s.size(); ++i) { s.l[i] = amp * a.bipolar(); s.r[i] = amp * b.bipolar(); }
    return s;
}

/** Pink-ish noise (Kellet economy filter): closer to a real mix spectrum than white noise. */
Stereo pinkNoise (double sr, double seconds, uint32_t seed = 5)
{
    Stereo s ((size_t) (sr * seconds));
    Random rng (seed);
    double b0 = 0, b1 = 0, b2 = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double w = rng.bipolar() * 0.3;
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        s.l[i] = s.r[i] = (float) ((b0 + b1 + b2 + w * 0.1848) * 0.3);
    }
    return s;
}

/** Detuned saw chord (A minor) - a typical trance pad source for demo renders. */
Stereo sawChord (double sr, double seconds)
{
    const double notes[] = { 220.0, 261.63, 329.63, 440.0 };
    Stereo s ((size_t) (sr * seconds));
    for (double f : notes)
        for (double det : { -0.003, 0.003 })
        {
            double ph = 0.0;
            for (size_t i = 0; i < s.size(); ++i)
            {
                const float v = 0.06f * (float) (2.0 * ph - 1.0);
                s.l[i] += det < 0 ? v : 0.6f * v;
                s.r[i] += det > 0 ? v : 0.6f * v;
                ph = wrap01 (ph + f * (1.0 + det) / sr);
            }
        }
    return s;
}

// --- helpers -----------------------------------------------------------------
EngineParams allOff()
{
    EngineParams p;
    p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = false;
    return p;
}

double& srRef() { static double sr = 48000.0; return sr; } // sample rate of the current run

/** Feeds `s` through the engine in host-sized blocks, advancing host PPQ like a DAW would. */
void run (DeepTranceEngine& e, Stereo& s, int block, TransportInfo t)
{
    for (size_t off = 0; off < s.size(); off += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, s.size() - off);
        e.process (s.l.data() + off, s.r.data() + off, n, t);
        if (t.isPlaying) t.ppqAtBlockStart += n * t.bpm / 60.0 / srRef();
    }
}

Stereo process (EngineParams p, Stereo in, double sr, int block = 512, TransportInfo t = {})
{
    srRef() = sr;
    DeepTranceEngine e;
    e.setParams (p);
    e.prepare (sr);
    run (e, in, block, t);
    return in;
}

double rmsDb (const std::vector<float>& v, size_t from = 0)
{
    double acc = 0.0;
    for (size_t i = from; i < v.size(); ++i) acc += (double) v[i] * v[i];
    return 10.0 * std::log10 (acc / (double) std::max<size_t> (1, v.size() - from) + 1e-20);
}

float peak (const Stereo& s)
{
    float m = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) m = std::max ({ m, std::fabs (s.l[i]), std::fabs (s.r[i]) });
    return m;
}

bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

/** Single-bin DFT magnitude (Goertzel) at `hz`, from sample `from`. */
double toneLevel (const std::vector<float>& v, double sr, double hz, size_t from = 0)
{
    const double w = kTwoPiD * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (v.size() - from);
}

void writeWav (const std::string& path, const Stereo& s, double sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("  (could not write %s)\n", path.c_str()); return; }
    const uint32_t frames = (uint32_t) s.size(), dataBytes = frames * 2 * 2, rate = (uint32_t) sr;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 (rate); u32 (rate * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (size_t i = 0; i < s.size(); ++i)
        for (float v : { s.l[i], s.r[i] })
        {
            const int16_t q = (int16_t) std::lrint (clampf (v, -1.0f, 1.0f) * 32767.0f);
            std::fwrite (&q, 2, 1, f);
        }
    std::fclose (f);
}

void section (const char* name) { std::printf ("[%s]\n", name); }

// --- tests -------------------------------------------------------------------
void testBypassIsBitExact()
{
    section ("bypass and mix=0 are bit-exact");
    const double sr = 48000.0;
    const Stereo in = noise (sr, 1.0);

    Stereo out = process (allOff(), in, sr);
    bool same = std::memcmp (out.l.data(), in.l.data(), in.size() * 4) == 0
             && std::memcmp (out.r.data(), in.r.data(), in.size() * 4) == 0;
    CHECK (same, "all modules off should pass audio through untouched");

    EngineParams p; // defaults have Swirl + Drift on
    p.mix = 0.0f;
    out = process (p, in, sr);
    same = std::memcmp (out.l.data(), in.l.data(), in.size() * 4) == 0;
    CHECK (same, "global mix 0 should return the exact dry signal");
}

void testSilenceStaysSilent()
{
    section ("silence in -> silence out");
    EngineParams p;
    p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = true;
    p.swirl.mode = SwirlMode::Both;
    const Stereo out = process (p, Stereo (48000), 48000.0);
    CHECK (peak (out) == 0.0f, "expected exact silence, got peak %g", (double) peak (out));
}

void testStabilityExtremes()
{
    section ("stability at extreme settings, all sample rates");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        EngineParams p;
        p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = true;
        p.inputGainDb = 12.0f;
        p.gate.filterAmount = 1.0f; p.gate.rateIndex = 2;
        p.vowel.resonance = 2.0f; p.vowel.lfoDepth = 1.0f; p.vowel.lfoRate = 0;
        p.swirl.mode = SwirlMode::Both; p.swirl.feedback = 0.95f; p.swirl.depth = 1.0f; p.swirl.lowAmount = 1.0f; p.swirl.rateIndex = 0;
        p.drift.amount = 1.0f; p.drift.flutter = 1.0f; p.drift.rateHz = 0.1f; p.drift.mix = 1.0f;

        Stereo in = noise (sr, 4.0, 0.9f);
        const Stereo s = sine (sr, 110.0, 4.0, 0.9f);
        for (size_t i = 0; i < in.size(); ++i) { in.l[i] += s.l[i]; in.r[i] += s.r[i]; }

        const Stereo out = process (p, in, sr, 333);
        CHECK (allFinite (out), "NaN/Inf at %g Hz", sr);
        CHECK (peak (out) < 16.0f, "runaway output at %g Hz: peak %g", sr, (double) peak (out));
        std::printf ("  %6.0f Hz: peak %.2f\n", sr, (double) peak (out));
    }
}

void testGateTiming()
{
    section ("gate follows host tempo and pattern");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.gateOn = true;
    p.gate.pattern = 2;      // Offbeat Pump: steps 3 & 4 of each beat open
    p.gate.rateIndex = 1;    // 1/16
    p.gate.length = 1.0f;
    p.gate.attackMs = 0.5f; p.gate.releaseMs = 1.0f;

    Stereo dc ((size_t) sr); std::fill (dc.l.begin(), dc.l.end(), 1.0f); dc.r = dc.l;
    TransportInfo t; t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true; t.ppqAtBlockStart = 0.0;
    const Stereo out = process (p, dc, sr, 512, t);

    const double stepSamples = sr * 60.0 / 120.0 / 4.0; // one 1/16 at 120 BPM = 6000 samples
    for (int step = 0; step < 8; ++step)
    {
        const float v = out.l[(size_t) ((step + 0.5) * stepSamples)];
        const bool open = (step % 4) >= 2;
        CHECK (open ? v > 0.95f : v < 0.05f, "step %d expected %s, got %.3f", step, open ? "open" : "closed", (double) v);
    }

    // Host-synced output must not depend on the host's block size.
    const Stereo a = process (p, dc, sr, 64, t), b = process (p, dc, sr, 1999, t);
    float maxDiff = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) maxDiff = std::max (maxDiff, std::fabs (a.l[i] - b.l[i]));
    CHECK (maxDiff < 1e-4f, "block size changed output by %g", (double) maxDiff);

    // Transport stopped: gate free-runs instead of freezing.
    t.isPlaying = false;
    const Stereo free = process (p, dc, sr, 512, t);
    float lo = 1.0f, hi = 0.0f;
    for (float v : free.l) { lo = std::min (lo, v); hi = std::max (hi, v); }
    CHECK (lo < 0.05f && hi > 0.95f, "free-running gate should still open and close");
}

void testVowel()
{
    section ("vowel filter: formants and loudness");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.vowelOn = true;

    // Loudness stays within +/-6 dB on musical material (pink noise and a saw chord).
    // Formants are resonances, so some per-vowel variation is expected and intended.
    const Stereo sources[] = { pinkNoise (sr, 2.0), sawChord (sr, 2.0) };
    const char*  srcNames[] = { "pink", "saw" };
    for (int s = 0; s < 2; ++s)
    {
        const double inDb = rmsDb (sources[s].l, 4800);
        std::printf ("  %-5s", srcNames[s]);
        for (float m : { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f })
        {
            p.vowel.morph = m;
            const double d = rmsDb (process (p, sources[s], sr).l, 4800) - inDb;
            std::printf (" %s %+.1f", VowelFilter::kVowelNames[(size_t) m], d);
            CHECK (std::fabs (d) < 6.0, "%s: vowel %.0f loudness off by %.1f dB", srcNames[s], (double) m, d);
        }
        std::printf (" dB\n");
    }

    // "A" passes 650 Hz far more than 1870 Hz; "I" favours 1870 Hz relative to "A".
    auto level = [&] (float morph, double hz)
    {
        p.vowel.morph = morph;
        return toneLevel (process (p, sine (sr, hz, 0.5), sr).l, sr, hz, 4800);
    };
    CHECK (level (0.0f, 650.0) > 4.0 * level (0.0f, 1870.0), "A should emphasise its 1st formant");
    CHECK (level (2.0f, 1870.0) > 2.0 * level (0.0f, 1870.0), "I should boost 1870 Hz more than A does");
}

void testSwirl()
{
    section ("swirl: flat at mix 0, moving at mix 1");
    const double sr = 48000.0;
    const Stereo in = noise (sr, 2.0);
    EngineParams p = allOff();
    p.swirlOn = true;

    p.swirl.mix = 0.0f;
    const double d0 = rmsDb (process (p, in, sr).l) - rmsDb (in.l);
    CHECK (std::fabs (d0) < 0.5, "crossover recombination should be flat, got %+.2f dB", d0);

    // Phaser must create time-varying notches: 1/4 note rate, compare a tone's level over time.
    p.swirl.mix = 1.0f; p.swirl.depth = 1.0f; p.swirl.rateIndex = 2;
    TransportInfo t; t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true;
    const Stereo out = process (p, sine (sr, 3000.0, 1.0), sr, 512, t);
    double lo = 1e9, hi = 0.0;
    for (size_t w = 4800; w + 2400 <= out.size(); w += 1200)
    {
        const std::vector<float> win (out.l.begin() + (long) w, out.l.begin() + (long) (w + 2400));
        const double lv = toneLevel (win, sr, 3000.0);
        lo = std::min (lo, lv); hi = std::max (hi, lv);
    }
    const double swingDb = 20.0 * std::log10 (hi / std::max (lo, 1e-9));
    std::printf ("  phaser tone swing: %.1f dB\n", swingDb);
    CHECK (swingDb > 6.0, "phaser should sweep notches through the tone");

    // Low band stays solid with lowAmount = 0: a 60 Hz tone is barely touched.
    const Stereo bass = process (p, sine (sr, 60.0, 1.0), sr);
    const double bassDb = 20.0 * std::log10 (toneLevel (bass.l, sr, 60.0, 9600) / toneLevel (sine (sr, 60.0, 1.0).l, sr, 60.0, 9600));
    std::printf ("  60 Hz through full swirl: %+.2f dB\n", bassDb);
    CHECK (std::fabs (bassDb) < 1.0, "low band should be protected");
}

void testDriftDetune()
{
    section ("drift: measured detune matches the cents spec");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.driftOn = true;
    p.drift.amount = 1.0f; p.drift.flutter = 0.0f; p.drift.rateHz = 1.0f; p.drift.mix = 1.0f; p.drift.stereo = 0.0f;

    const Stereo out = process (p, sine (sr, 1000.0, 4.0), sr);

    // Instantaneous frequency from interpolated zero crossings, averaged over 10 cycles.
    std::vector<double> crossings;
    for (size_t i = 1; i < out.size(); ++i)
        if (out.l[i - 1] < 0.0f && out.l[i] >= 0.0f)
            crossings.push_back ((double) (i - 1) + out.l[i - 1] / (out.l[i - 1] - out.l[i]));

    double maxCents = 0.0;
    for (size_t k = 20; k + 10 < crossings.size(); ++k)
    {
        const double hz = 10.0 * sr / (crossings[k + 10] - crossings[k]);
        maxCents = std::max (maxCents, std::fabs (1200.0 * std::log2 (hz / 1000.0)));
    }
    std::printf ("  peak detune: %.1f cents (spec: 25 max)\n", maxCents);
    CHECK (maxCents > 12.0 && maxCents < 30.0, "detune %.1f cents outside expected range", maxCents);
}

void testToggleIsClickFree()
{
    section ("module toggles are click-free");
    const double sr = 48000.0;
    const Stereo in = sine (sr, 220.0, 1.0);
    float maxStepIn = 0.0f;
    for (size_t i = 1; i < in.size(); ++i) maxStepIn = std::max (maxStepIn, std::fabs (in.l[i] - in.l[i - 1]));

    for (int which = 0; which < 4; ++which)
    {
        DeepTranceEngine e;
        EngineParams p = allOff();
        p.swirl.mix = 1.0f; p.drift.mix = 1.0f; p.gate.pattern = 1; p.vowel.morph = 2.0f;
        e.setParams (p); e.prepare (sr);

        Stereo s = in;
        for (size_t off = 0; off < s.size(); off += 480)
        {
            const bool on = (off / 4800) % 2 == 1; // toggle every 100 ms
            bool* flags[] = { &p.gateOn, &p.vowelOn, &p.swirlOn, &p.driftOn };
            *flags[which] = on;
            e.setParams (p);
            e.process (s.l.data() + off, s.r.data() + off, 480, TransportInfo {});
        }
        float maxStep = 0.0f;
        for (size_t i = 1; i < s.size(); ++i) maxStep = std::max (maxStep, std::fabs (s.l[i] - s.l[i - 1]));
        const char* names[] = { "gate", "vowel", "swirl", "drift" };
        std::printf ("  %-5s max step %.4f (input %.4f)\n", names[which], (double) maxStep, (double) maxStepIn);
        CHECK (maxStep < 4.0f * maxStepIn, "%s toggle produced a click", names[which]);
    }
}

void testCpu()
{
    section ("CPU load");
    const double sr = 48000.0, seconds = 10.0;
    EngineParams p;
    p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = true;
    p.gate.filterAmount = 0.5f; p.vowel.lfoDepth = 0.5f; p.swirl.mode = SwirlMode::Both;
    Stereo s = noise (sr, seconds);

    srRef() = sr;
    DeepTranceEngine e; e.setParams (p); e.prepare (sr);
    const auto t0 = std::chrono::steady_clock::now();
    run (e, s, 256, TransportInfo {});
    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * elapsed / seconds;
    std::printf ("  all Phase 1 modules: %.2f%% of one core in real time\n", pct);
    CHECK (pct < 10.0, "Phase 1 chain too heavy: %.2f%%", pct);
}

void renderDemos (const std::string& dir)
{
    section ("rendering demo WAVs");
    const double sr = 48000.0;
    TransportInfo t; t.bpm = 138.0; t.isPlaying = true; t.hasPpq = true;
    const Stereo pad = sawChord (sr, 8.0);

    struct Demo { const char* file; std::function<void (EngineParams&)> setup; };
    const Demo demos[] = {
        { "00_dry.wav",          [] (EngineParams& p) { p = allOff(); } },
        { "01_defaults.wav",     [] (EngineParams&) {} },
        { "02_gate_pump.wav",    [] (EngineParams& p) { p.gateOn = true; p.gate.pattern = 3; p.gate.filterAmount = 0.35f; } },
        { "03_vowel_talk.wav",   [] (EngineParams& p) { p.vowelOn = true; p.vowel.morph = 2.0f; p.vowel.lfoDepth = 1.0f; p.vowel.lfoRate = 3; p.vowel.mix = 0.8f; } },
        { "04_swirl_flange.wav", [] (EngineParams& p) { p.swirl.mode = SwirlMode::Both; p.swirl.depth = 0.8f; p.swirl.feedback = 0.7f; p.swirl.mix = 0.6f; p.swirl.rateIndex = 4; } },
        { "05_drift_wide.wav",   [] (EngineParams& p) { p.drift.amount = 0.8f; p.drift.flutter = 0.4f; p.drift.mix = 0.5f; } },
    };

    for (const auto& d : demos)
    {
        EngineParams p;
        d.setup (p);
        const Stereo out = process (p, pad, sr, 512, t);
        writeWav (dir + "/" + d.file, out, sr);
        std::printf ("  wrote %s\n", d.file);
    }
}
} // namespace

int main (int argc, char** argv)
{
    testBypassIsBitExact();
    testSilenceStaysSilent();
    testStabilityExtremes();
    testGateTiming();
    testVowel();
    testSwirl();
    testDriftDetune();
    testToggleIsClickFree();
    testCpu();

    if (argc == 3 && std::string (argv[1]) == "--render")
        renderDemos (argv[2]);

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
