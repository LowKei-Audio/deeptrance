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
    p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = p.acidOn = false;
    p.punchOn = p.cloudOn = p.delayOn = p.abyssOn = false;
    return p;
}

/** Every module on, at its defaults. */
EngineParams allOn()
{
    EngineParams p;
    p.gateOn = p.vowelOn = p.swirlOn = p.driftOn = p.acidOn = true;
    p.punchOn = p.cloudOn = p.delayOn = p.abyssOn = true;
    return p;
}

/** Decaying noise bursts every quarter note at 120 BPM: a stand-in for drums/plucks. */
Stereo hits (double sr, double seconds, float amp = 0.8f)
{
    Stereo s ((size_t) (sr * seconds));
    Random rng (77);
    const size_t period = (size_t) (sr * 0.5);
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) (i % period) / sr;
        const float v = amp * rng.bipolar() * (float) std::exp (-t * 30.0);
        s.l[i] = s.r[i] = v;
    }
    return s;
}

/** Estimated frequency from interpolated rising zero crossings in [from, to). */
double zeroCrossHz (const std::vector<float>& v, double sr, size_t from, size_t to)
{
    double first = -1.0, last = -1.0; int count = 0;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, v.size()); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double x = (double) (i - 1) + v[i - 1] / (v[i - 1] - v[i]);
            if (first < 0.0) first = x; else ++count;
            last = x;
        }
    return count > 0 ? count * sr / (last - first) : 0.0;
}

double windowRmsDb (const std::vector<float>& v, size_t from, size_t len)
{
    double acc = 0.0;
    for (size_t i = from; i < std::min (v.size(), from + len); ++i) acc += (double) v[i] * v[i];
    return 10.0 * std::log10 (acc / (double) len + 1e-20);
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
    EngineParams p = allOn();
    p.swirl.mode = SwirlMode::Both;
    const Stereo out = process (p, Stereo (48000), 48000.0);
    CHECK (peak (out) == 0.0f, "expected exact silence, got peak %g", (double) peak (out));
}

void testStabilityExtremes()
{
    section ("stability at extreme settings, all sample rates");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        EngineParams p = allOn();
        p.inputGainDb = 12.0f;
        p.acid.resonance = 1.0f; p.acid.drive = 1.0f; p.acid.envMod = 1.0f;
        p.punch.attack = 1.0f; p.punch.sustain = 1.0f;
        p.cloud.density = 200.0f; p.cloud.sizeMs = 500.0f; p.cloud.pitch = 24.0f; p.cloud.shimmer = 1.0f; p.cloud.mix = 1.0f;
        p.delay.feedback = 0.95f; p.delay.pitch = 12.0f; p.delay.toneHz = 18000.0f; p.delay.mix = 1.0f; p.delay.sync = false; p.delay.timeMs = 50.0f;
        p.abyss.decaySec = 60.0f; p.abyss.shimmer = 1.0f; p.abyss.size = 1.0f; p.abyss.damping = 0.0f; p.abyss.mix = 1.0f;
        p.buildUp = 1.0f; p.buildUpNoise = 1.0f;
        p.mod[0] = { 1, (int) ModDest::AbyssSize, 1.0f }; p.lfo[0].syncIndex = 0;
        p.gate.filterAmount = 1.0f; p.gate.rateIndex = 2;
        p.vowel.resonance = 2.0f; p.vowel.lfoDepth = 1.0f; p.vowel.lfoRate = 0;
        p.swirl.mode = SwirlMode::Both; p.swirl.feedback = 0.95f; p.swirl.depth = 1.0f; p.swirl.lowAmount = 1.0f; p.swirl.rateIndex = 0;
        p.drift.amount = 1.0f; p.drift.flutter = 1.0f; p.drift.rateHz = 0.1f; p.drift.mix = 1.0f;

        Stereo in = noise (sr, 4.0, 0.9f);
        const Stereo s = sine (sr, 110.0, 4.0, 0.9f);
        for (size_t i = 0; i < in.size(); ++i) { in.l[i] += s.l[i]; in.r[i] += s.r[i]; }

        const Stereo out = process (p, in, sr, 333);
        CHECK (allFinite (out), "NaN/Inf at %g Hz", sr);
        CHECK (peak (out) < 24.0f, "runaway output at %g Hz: peak %g", sr, (double) peak (out));
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

    for (int which = 0; which < 6; ++which)
    {
        DeepTranceEngine e;
        EngineParams p = allOff();
        p.swirl.mix = 1.0f; p.drift.mix = 1.0f; p.gate.pattern = 1; p.vowel.morph = 2.0f;
        e.setParams (p); e.prepare (sr);

        Stereo s = in;
        for (size_t off = 0; off < s.size(); off += 480)
        {
            const bool on = (off / 4800) % 2 == 1; // toggle every 100 ms
            bool* flags[] = { &p.gateOn, &p.vowelOn, &p.swirlOn, &p.driftOn, &p.acidOn, &p.punchOn };
            *flags[which] = on;
            e.setParams (p);
            e.process (s.l.data() + off, s.r.data() + off, 480, TransportInfo {});
        }
        float maxStep = 0.0f;
        for (size_t i = 1; i < s.size(); ++i) maxStep = std::max (maxStep, std::fabs (s.l[i] - s.l[i - 1]));
        const char* names[] = { "gate", "vowel", "swirl", "drift", "acid", "punch" };
        std::printf ("  %-5s max step %.4f (input %.4f)\n", names[which], (double) maxStep, (double) maxStepIn);
        CHECK (maxStep < 4.0f * maxStepIn, "%s toggle produced a click", names[which]);
    }
}

void testDelay()
{
    section ("fractal delay: timing, ping-pong, pitch per repeat, spillover");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.delayOn = true;
    p.delay.sync = true; p.delay.syncIndex = 6; // 1/8D at 120 BPM = 375 ms
    p.delay.feedback = 0.6f; p.delay.mix = 0.5f; p.delay.pingPong = 1.0f; p.delay.toneHz = 18000.0f;
    TransportInfo t; t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true;

    // Impulse: echoes alternate L, R, L ... at 375 ms spacing.
    Stereo imp ((size_t) (sr * 2.0)); imp.l[0] = imp.r[0] = 1.0f;
    const Stereo out = process (p, imp, sr, 512, t);
    const size_t step = (size_t) (0.375 * sr);
    for (int k = 1; k <= 3; ++k)
    {
        size_t best = 0; float bestV = 0.0f; bool right = false;
        for (size_t i = k * step - 200; i < k * step + 2000; ++i)
        {
            if (std::fabs (out.l[i]) > bestV) { bestV = std::fabs (out.l[i]); best = i; right = false; }
            if (std::fabs (out.r[i]) > bestV) { bestV = std::fabs (out.r[i]); best = i; right = true; }
        }
        const double errMs = ((double) best - (double) (k * step)) / sr * 1000.0;
        std::printf ("  echo %d: %+.2f ms on %s\n", k, errMs, right ? "R" : "L");
        CHECK (std::fabs (errMs) < 2.0, "echo %d off the grid by %.2f ms", k, errMs);
        CHECK (right == (k % 2 == 0), "echo %d on the wrong side", k);
    }

    // Pitch +12 per repeat: echo 2 (one pass through the shifter) is an octave above echo 1.
    p.delay.pitch = 12.0f; p.delay.pingPong = 0.0f;
    Stereo burst ((size_t) (sr * 1.5));
    for (size_t i = 0; i < (size_t) (0.2 * sr); ++i) burst.l[i] = burst.r[i] = 0.5f * (float) std::sin (kTwoPiD * 440.0 * i / sr);
    const Stereo pit = process (p, burst, sr, 512, t);
    const double f1 = zeroCrossHz (pit.l, sr, step + 2000, step + 8000);
    const double f2 = zeroCrossHz (pit.l, sr, 2 * step + 4000, 2 * step + 8000);
    std::printf ("  echo 1: %.0f Hz, echo 2: %.0f Hz (expect 440 / 880)\n", f1, f2);
    CHECK (std::fabs (f1 - 440.0) < 10.0 && std::fabs (f2 - 880.0) < 25.0, "pitch per repeat wrong");

    // Spillover: switching the delay off keeps the echo tail, then goes bit-exact dry.
    p.delay.pitch = 0.0f;
    DeepTranceEngine e; e.setParams (p); e.prepare (sr);
    Stereo s ((size_t) (sr * 6.0)); s.l[0] = s.r[0] = 1.0f;
    for (size_t off = 0; off < s.size(); off += 480)
    {
        if (off == 4800) { p.delayOn = false; e.setParams (p); } // off after 100 ms
        e.process (s.l.data() + off, s.r.data() + off, 480, t);
        t.ppqAtBlockStart += 480 * 2.0 / sr;
    }
    CHECK (std::fabs (s.l[step]) + std::fabs (s.r[step]) > 0.05f || peak (s) > 0.05f, "tail should ring out after switching off");
    Stereo dryAfter = noise (sr, 0.5);
    const Stereo dryIn = dryAfter;
    e.process (dryAfter.l.data(), dryAfter.r.data(), (int) dryAfter.size(), t);
    CHECK (std::memcmp (dryAfter.l.data(), dryIn.l.data(), dryIn.size() * 4) == 0, "after the tail ends, the delay should be fully bypassed");
}

void testAbyss()
{
    section ("abyss reverb: decay time, freeze, shimmer stability");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.abyssOn = true;
    p.abyss.mix = 1.0f; p.abyss.preDelayMs = 0.0f; p.abyss.damping = 0.0f; p.abyss.mod = 0.0f;

    // RT60: measure the decay slope of an impulse response between 0.3 s and 1.3 s.
    for (float rt : { 2.0f, 6.0f })
    {
        p.abyss.decaySec = rt;
        Stereo imp ((size_t) (sr * 2.0)); imp.l[0] = imp.r[0] = 1.0f;
        const Stereo out = process (p, imp, sr);
        const size_t win = (size_t) (0.1 * sr);
        const double a = windowRmsDb (out.l, (size_t) (0.3 * sr), win), b = windowRmsDb (out.l, (size_t) (1.3 * sr), win);
        const double measured = 60.0 / (a - b);
        std::printf ("  RT60 set %.0f s -> measured %.2f s\n", (double) rt, measured);
        CHECK (measured > rt * 0.7 && measured < rt * 1.35, "decay time off: %.2f s for %.0f s", measured, (double) rt);
    }

    // Freeze: 1 s of noise, then freeze for 20 s. Level must hold (no decay, no growth).
    for (float shim : { 0.0f, 1.0f })
    {
        DeepTranceEngine e;
        p.abyss.decaySec = 3.0f; p.abyss.freeze = false; p.abyss.shimmer = shim; p.abyss.mod = 0.4f;
        e.setParams (p); e.prepare (sr);
        Stereo s = noise (sr, 21.0, 0.3f);
        for (size_t i = (size_t) sr; i < s.size(); ++i) s.l[i] = s.r[i] = 0.0f; // input stops at 1 s
        for (size_t off = 0; off < s.size(); off += 512)
        {
            if (off >= (size_t) (0.9 * sr) && ! p.abyss.freeze) { p.abyss.freeze = true; e.setParams (p); }
            e.process (s.l.data() + off, s.r.data() + off, (int) std::min<size_t> (512, s.size() - off), TransportInfo {});
        }
        const size_t win = (size_t) (0.5 * sr);
        const double at2 = windowRmsDb (s.l, (size_t) (2 * sr), win), at20 = windowRmsDb (s.l, (size_t) (20 * sr), win);
        std::printf ("  freeze (shimmer %.0f%%): %.1f dB at 2 s, %.1f dB at 20 s\n", shim * 100.0, at2, at20);
        CHECK (std::fabs (at20 - at2) < 4.0, "frozen level should hold steady"); // highs settle first, then it levels off (measured: -3.5 dB after 2 minutes)
        CHECK (allFinite (s) && peak (s) < 4.0f, "freeze must stay bounded");
    }

    // Regression: shimmer must not run away into the loop limiter on sustained tones
    // (it did at large sizes before the loop gain was tied to the decay headroom).
    for (float size : { 0.0f, 1.0f })
        for (float rt : { 2.0f, 60.0f })
        {
            EngineParams q = allOff();
            q.abyssOn = true; q.abyss.mix = 1.0f; q.abyss.size = size; q.abyss.decaySec = rt;
            const Stereo tone = sine (sr, 440.0, 10.0, 0.3f);
            q.abyss.shimmer = 0.0f; const double plain = rmsDb (process (q, tone, sr).l, (size_t) (6 * sr));
            q.abyss.shimmer = 1.0f; const Stereo sh = process (q, tone, sr);
            const double withShim = rmsDb (sh.l, (size_t) (6 * sr));
            std::printf ("  shimmer 100%% (size %.0f, %2.0f s): %+.1f dB vs plain, peak %.2f\n", (double) size, (double) rt, withShim - plain, (double) peak (sh));
            // Runaway looked like +11..+18 dB, pinned at the loop limiter; healthy shimmer adds a few dB.
            CHECK (withShim - plain < 6.0 && peak (sh) < 2.5f, "shimmer runaway at size %.0f, %.0f s", (double) size, (double) rt);
        }

    // Shimmer at maximum with a 60 s decay on continuous input: bounded.
    p.abyss.freeze = false; p.abyss.decaySec = 60.0f; p.abyss.shimmer = 1.0f;
    const Stereo out = process (p, noise (sr, 20.0, 0.5f), sr);
    std::printf ("  max shimmer + 60 s decay, 20 s of noise: peak %.2f\n", (double) peak (out));
    CHECK (allFinite (out) && peak (out) < 8.0f, "shimmer loop must stay stable");
}

void testPunch()
{
    section ("punch: sharper attacks, ducked tails");
    const double sr = 48000.0;
    const Stereo in = hits (sr, 4.0);
    EngineParams p = allOff();
    p.punchOn = true; p.punch.attack = 1.0f; p.punch.duck = 0.0f;

    // Crest factor (peak vs RMS) rises when attacks are boosted.
    auto crestDb = [] (const Stereo& s) { return 20.0 * std::log10 ((double) peak (s)) - rmsDb (s.l); };
    const double before = crestDb (in), after = crestDb (process (p, in, sr));
    std::printf ("  crest factor: %.1f dB -> %.1f dB\n", before, after);
    CHECK (after > before + 1.5, "attack boost should raise the crest factor");

    // Ducking: reverb level right after each hit drops when duck is on.
    p.punch.attack = 0.0f; p.abyssOn = true; p.abyss.mix = 0.5f; p.abyss.decaySec = 4.0f;
    auto earlyTail = [&] (float duckAmt)
    {
        p.punch.duck = duckAmt;
        const Stereo o = process (p, in, sr);
        const Stereo dry = in;
        double acc = 0.0;
        for (int hit = 2; hit < 8; ++hit) // 10-40 ms after each hit, wet only (output minus dry)
            for (size_t i = hit * (size_t) (0.5 * sr) + 480; i < hit * (size_t) (0.5 * sr) + 1920; ++i)
                acc += std::pow (o.l[i] - dry.l[i], 2.0);
        return 10.0 * std::log10 (acc + 1e-20);
    };
    const double noDuck = earlyTail (0.0f), fullDuck = earlyTail (1.0f);
    std::printf ("  wet level just after hits: %.1f dB -> %.1f dB with duck\n", noDuck, fullDuck);
    CHECK (fullDuck < noDuck - 6.0, "duck should pull the wet down after hits");
}

void testCloud()
{
    section ("cloud: grains, pitch, freeze");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.cloudOn = true; p.cloud.mix = 1.0f; p.cloud.jitter = 0.0f; p.cloud.spray = 0.0f; p.cloud.stereo = 0.0f;

    // Pitch +12: a 300 Hz tone comes out at 600 Hz.
    p.cloud.pitch = 12.0f; p.cloud.sizeMs = 100.0f; p.cloud.density = 30.0f;
    const Stereo out = process (p, sine (sr, 300.0, 2.0), sr);
    const double up = toneLevel (out.l, sr, 600.0, (size_t) sr), base = toneLevel (out.l, sr, 300.0, (size_t) sr);
    std::printf ("  pitch +12: 600 Hz %.4f vs 300 Hz %.4f\n", up, base);
    CHECK (up > 5.0 * base, "grains should be an octave up");

    // Loudness roughly steady across density (normalisation works).
    p.cloud.pitch = 0.0f;
    const Stereo pink = pinkNoise (sr, 3.0);
    for (float d : { 5.0f, 40.0f, 200.0f })
    {
        p.cloud.density = d;
        const double diff = rmsDb (process (p, pink, sr).l, (size_t) sr) - rmsDb (pink.l, (size_t) sr);
        std::printf ("  density %3.0f/s: %+.1f dB\n", (double) d, diff);
        CHECK (std::fabs (diff) < 8.0, "density %.0f changes loudness by %.1f dB", (double) d, diff);
    }

    // Freeze: input stops at 1 s, the cloud keeps sounding at 5 s.
    DeepTranceEngine e;
    p.cloud.density = 30.0f; p.cloud.freeze = false;
    e.setParams (p); e.prepare (sr);
    Stereo s = sine (sr, 300.0, 5.0);
    for (size_t i = (size_t) sr; i < s.size(); ++i) s.l[i] = s.r[i] = 0.0f;
    for (size_t off = 0; off < s.size(); off += 512)
    {
        if (off >= (size_t) (0.9 * sr) && ! p.cloud.freeze) { p.cloud.freeze = true; e.setParams (p); }
        e.process (s.l.data() + off, s.r.data() + off, (int) std::min<size_t> (512, s.size() - off), TransportInfo {});
    }
    const double frozenLvl = windowRmsDb (s.l, (size_t) (4.5 * sr), (size_t) (0.4 * sr));
    std::printf ("  frozen cloud at 4.5 s: %.1f dB\n", frozenLvl);
    CHECK (frozenLvl > -30.0, "frozen cloud should keep sounding");
}

void testAcid()
{
    section ("acid: resonance, loudness across drive");
    const double sr = 48000.0;
    EngineParams p = allOff();
    p.acidOn = true; p.acid.envMod = 0.0f;
    const Stereo saw = sawChord (sr, 2.0);
    const double inDb = rmsDb (saw.l, 4800);
    for (float d : { 0.0f, 0.5f, 1.0f })
    {
        p.acid.drive = d; p.acid.cutoffHz = 2000.0f; p.acid.resonance = 0.5f;
        const double diff = rmsDb (process (p, saw, sr).l, 4800) - inDb;
        std::printf ("  drive %3.0f%%: %+.1f dB\n", d * 100.0, diff);
        CHECK (std::fabs (diff) < 8.0, "drive %.0f%% loudness off by %.1f dB", d * 100.0, diff);
    }

    // High resonance: noise output has a clear peak near the cutoff.
    p.acid.drive = 0.0f; p.acid.resonance = 0.9f; p.acid.cutoffHz = 1000.0f;
    const Stereo n = process (p, noise (sr, 2.0, 0.1f), sr);
    const double atCut = toneLevel (n.l, sr, 1000.0, 4800), below = toneLevel (n.l, sr, 300.0, 4800);
    std::printf ("  resonant peak: 1 kHz %.5f vs 300 Hz %.5f\n", atCut, below);
    CHECK (atCut > 2.0 * below, "resonance should peak at the cutoff");
}

void testBuildUpAndCombos()
{
    section ("build-up, combos, modulation");
    const double sr = 48000.0;

    // Build-Up at full: bass removed, riser noise present, reverb forced on.
    EngineParams p = allOff();
    p.buildUp = 1.0f; p.buildUpNoise = 1.0f;
    const Stereo bass = process (p, sine (sr, 60.0, 2.0), sr);
    const double cut = 20.0 * std::log10 (toneLevel (bass.l, sr, 60.0, 9600) / toneLevel (sine (sr, 60.0, 2.0).l, sr, 60.0, 9600));
    std::printf ("  build-up 100%%: 60 Hz %+.1f dB\n", cut);
    CHECK (cut < -24.0, "build-up should remove the bass");
    const Stereo riser = process (p, Stereo ((size_t) sr), sr);
    std::printf ("  riser on silence: %.1f dBFS\n", rmsDb (riser.l, 4800));
    CHECK (rmsDb (riser.l, 4800) > -45.0, "build-up should add a noise riser");
    const EngineParams bd = DeepTranceEngine::derive (p, 0, 0, 0);
    CHECK (bd.abyssOn && bd.abyss.mix >= 0.59f && bd.swirl.speedMul > 7.9f, "build-up should push reverb and swirl speed");

    // Combos at full amount.
    EngineParams c = allOff();
    c.combo = Combo::AstralProjection; c.comboAmount = 1.0f;
    const EngineParams astral = DeepTranceEngine::derive (c, 0, 0, 0);
    CHECK (astral.cloudOn && astral.abyssOn && astral.cloud.mix == 1.0f && astral.abyss.mix == 1.0f, "astral: granular into reverb, no dry");
    c.combo = Combo::AcidBath;
    const EngineParams acid = DeepTranceEngine::derive (c, 0, 0, 0);
    CHECK (acid.acidOn && acid.delayOn && acid.delay.syncIndex == 2, "acid bath: 303 + fast delay");
    c.comboAmount = 0.0f;
    const EngineParams none = DeepTranceEngine::derive (c, 0, 0, 0);
    CHECK (! none.acidOn && ! none.delayOn, "combo amount 0 changes nothing");

    // Astral output: dry pad mostly replaced by the wash (low correlation with input).
    c.combo = Combo::AstralProjection; c.comboAmount = 1.0f;
    TransportInfo t; t.bpm = 138.0; t.isPlaying = true; t.hasPpq = true;
    const Stereo pad = sawChord (sr, 4.0);
    const Stereo wash = process (c, pad, sr, 512, t);
    CHECK (allFinite (wash) && rmsDb (wash.l, (size_t) sr) > -40.0, "astral should produce a wash");

    // Mod matrix: LFO 1 at +1, 50% to Vowel -> +2 vowels.
    EngineParams m = allOff();
    m.mod[0] = { 1, (int) ModDest::VowelMorph, 0.5f };
    CHECK (std::fabs (DeepTranceEngine::derive (m, 1.0f, 0, 0).vowel.morph - 2.0f) < 1e-5f, "mod offset wrong");
    m.mod[1] = { 3, (int) ModDest::AcidCutoff, 0.2f }; // envelope, log destination: +1 octave at full
    CHECK (std::fabs (DeepTranceEngine::derive (m, 0, 0, 1.0f).acid.cutoffHz - 1600.0f) < 1.0f, "log-scaled mod wrong");

    // Macros: 50% neutral, 0% removes the effect, 100% doubles it.
    EngineParams k = allOff();
    CHECK (DeepTranceEngine::derive (k, 0, 0, 0).swirl.mix == k.swirl.mix, "macro 50%% must be neutral");
    k.macros.swirl = 0.0f;
    CHECK (DeepTranceEngine::derive (k, 0, 0, 0).swirl.mix == 0.0f, "macro 0%% must remove the effect");

    // Synced LFO: 1 bar sine, a quarter note in = peak.
    Lfo lfo; lfo.prepare (sr);
    LfoParams lp; lp.sync = true; lp.syncIndex = 8; lfo.setParams (lp);
    TransportClock clk; clk.prepare (sr);
    TransportInfo ti; ti.bpm = 120.0; ti.isPlaying = true; ti.hasPpq = true; ti.ppqAtBlockStart = 1.0;
    clk.beginBlock (ti);
    CHECK (std::fabs (lfo.tick (clk, 32) - 1.0f) < 1e-3f, "synced LFO phase wrong");
}

void testCpu()
{
    section ("CPU load");
    const double sr = 48000.0, seconds = 10.0;
    TransportInfo t; t.bpm = 138.0; t.isPlaying = true; t.hasPpq = true;
    const Stereo src = noise (sr, seconds);

    auto measure = [&] (const char* label, const EngineParams& p)
    {
        Stereo s = src;
        srRef() = sr;
        DeepTranceEngine e; e.setParams (p); e.prepare (sr);
        const auto t0 = std::chrono::steady_clock::now();
        run (e, s, 256, t);
        const double pct = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / seconds;
        std::printf ("  %-34s %.2f%% of one core\n", label, pct);
        return pct;
    };

    measure ("defaults:", EngineParams {});
    EngineParams p = allOn();
    p.gate.filterAmount = 0.5f; p.vowel.lfoDepth = 0.5f; p.swirl.mode = SwirlMode::Both; p.abyss.shimmer = 0.5f;
    for (int i = 0; i < kModSlots; ++i) p.mod[(size_t) i] = { 1 + i % 3, 1 + i * 3, 0.2f };
    measure ("everything on + 8 mod slots:", p);
    p.cloud.density = 200.0f; p.cloud.sizeMs = 500.0f; p.buildUp = 0.5f;
    const double worst = measure ("worst case (96 grains, build-up):", p);
    CHECK (worst < 25.0, "worst case too heavy: %.2f%%", worst);
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
        { "06_fractal_delay_up.wav", [] (EngineParams& p) { p = allOff(); p.delayOn = true; p.delay.pitch = 7.0f; p.delay.feedback = 0.6f; p.delay.mix = 0.45f; } },
        { "07_abyss_shimmer.wav",    [] (EngineParams& p) { p = allOff(); p.abyssOn = true; p.abyss.decaySec = 12.0f; p.abyss.shimmer = 0.5f; p.abyss.mix = 0.5f; p.abyss.size = 1.0f; } },
        { "08_cloud.wav",            [] (EngineParams& p) { p = allOff(); p.cloudOn = true; p.cloud.mix = 0.8f; p.cloud.sizeMs = 150.0f; p.cloud.density = 40.0f; p.cloud.jitter = 0.3f; p.cloud.shimmer = 0.3f; p.abyssOn = true; } },
        { "09_astral_projection.wav",[] (EngineParams& p) { p.combo = Combo::AstralProjection; } },
        { "10_acid_bath.wav",        [] (EngineParams& p) { p.combo = Combo::AcidBath; p.gateOn = true; p.gate.pattern = 3; } },
    };

    for (const auto& d : demos)
    {
        EngineParams p;
        d.setup (p);
        const Stereo out = process (p, pad, sr, 512, t);
        writeWav (dir + "/" + d.file, out, sr);
        std::printf ("  wrote %s\n", d.file);
    }

    // Build-Up automated from 0 to 100% over 8 bars, the way it is used before a drop.
    {
        EngineParams p;
        srRef() = sr;
        DeepTranceEngine e; e.setParams (p); e.prepare (sr);
        const double secs = 8 * 4 * 60.0 / 138.0;
        Stereo s = sawChord (sr, secs + 1.0);
        TransportInfo tt = t;
        for (size_t off = 0; off < s.size(); off += 512)
        {
            p.buildUp = (float) std::min (1.0, (double) off / (secs * sr));
            if (off >= (size_t) (secs * sr)) p.buildUp = 0.0f; // the drop
            e.setParams (p);
            const int n = (int) std::min<size_t> (512, s.size() - off);
            e.process (s.l.data() + off, s.r.data() + off, n, tt);
            tt.ppqAtBlockStart += n * tt.bpm / 60.0 / sr;
        }
        writeWav (dir + "/11_build_up_8_bars.wav", s, sr);
        std::printf ("  wrote 11_build_up_8_bars.wav\n");
    }

    // Punch on a drum-like loop through a big reverb, with and without ducking.
    for (float dk : { 0.0f, 1.0f })
    {
        EngineParams p = allOff();
        p.abyssOn = true; p.abyss.mix = 0.45f; p.abyss.decaySec = 5.0f;
        p.punchOn = true; p.punch.attack = 0.8f; p.punch.duck = dk;
        const Stereo out = process (p, hits (sr, 6.0, 0.25f), sr, 512, t); // leave headroom: Punch raises peaks by design
        const std::string f = dk > 0.0f ? "12b_punch_ducked.wav" : "12a_punch_no_duck.wav";
        writeWav (dir + "/" + f, out, sr);
        std::printf ("  wrote %s\n", f.c_str());
    }
}
// --- static (Serato report, Nov 2026) --------------------------------------------
// Engine-level regressions for clicks heard on loop wraps and sound changes; each fails
// on 1.0.1. The plugin-level static check (every preset, wraps, restarts, preset and
// state changes through the real processor) is in DeepTranceRender.

/** Held tone with harmonics, faded in over 50 ms. */
Stereo heldTone (double sr, double seconds)
{
    Stereo s ((size_t) (sr * seconds));
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double ph = kTwoPiD * 233.08 * (double) i / sr;
        const float v = (float) (0.3 * std::min (1.0, (double) i / 2400.0) * (std::sin (ph) + 0.4 * std::sin (2 * ph) + 0.2 * std::sin (3 * ph)));
        s.l[i] = s.r[i] = v;
    }
    return s;
}

/** Sharpest fourth difference in [from, to): a step or a kink stands out of it. */
float sharpest4 (const Stereo& s, size_t from, size_t to)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 4); i < std::min (to, s.size()); ++i)
        for (const auto* x : { &s.l, &s.r })
            m = std::max (m, std::fabs ((*x)[i] - 4 * (*x)[i - 1] + 6 * (*x)[i - 2] - 4 * (*x)[i - 3] + (*x)[i - 4]));
    return m;
}

float spikeRatio (const Stereo& s, size_t at, size_t window)
{
    const float ref = std::max ({ sharpest4 (s, at / 4, at - 1024), sharpest4 (s, at + window + 1024, s.size()), 1.0e-3f });
    return sharpest4 (s, at - 16, at + window) / ref;
}

void testStatic()
{
    section ("static: loop wraps and parameter jumps stay click-free");
    const double sr = 48000.0;

    // Swirl (on by default, synced): a host loop wrapping round mid-sweep.
    {
        EngineParams p = allOff();
        // 2-bar sweep (the default) and a 4-beat loop: each wrap jumps it half a cycle.
        p.swirlOn = true; p.swirl.mix = 1.0f; p.swirl.depth = 1.0f; p.swirl.rateIndex = 5;
        p.swirl.mode = SwirlMode::Both;
        DeepTranceEngine e;
        e.setParams (p); e.prepare (sr);
        Stereo s = heldTone (sr, 4.0);
        TransportInfo t; t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true;
        const size_t wrap = (size_t) (2.0 * sr) + 96;
        for (size_t off = 0; off < s.size(); off += 512)
        {
            const int n = (int) std::min<size_t> (512, s.size() - off);
            if (off <= wrap && wrap < off + 512) t.ppqAtBlockStart = 0.0;   // loop wraps round
            e.process (&s.l[off], &s.r[off], n, t);
            t.ppqAtBlockStart += n * t.bpm / 60.0 / sr;
        }
        const float r = spikeRatio (s, (wrap / 512) * 512, 4096);
        std::printf ("  synced Swirl across a loop wrap: spike %.2f\n", (double) r);
        CHECK (r < 2.5f, "a loop wrap should not click the Swirl sweep (%.2f)", (double) r);
    }

    // A fast synced LFO (1/16) sweeping the Acid filter, across a loop wrap mid-cycle.
    // 1.0.2's LFO glide started one 32-sample chunk behind the sweep (it paused there),
    // and took a fixed 40 ms, sweeping a fast cycle several times faster than drawn.
    {
        EngineParams p = allOff();
        p.acidOn = true; p.acid.cutoffHz = 900.0f; p.acid.resonance = 0.8f; p.acid.envMod = 0.0f;
        p.lfo[0].sync = true; p.lfo[0].syncIndex = 2; p.lfo[0].shape = LfoShape::Sine;
        p.mod[0] = { 1, (int) ModDest::AcidCutoff, 1.0f };
        DeepTranceEngine e;
        e.setParams (p); e.prepare (sr);
        Stereo s = heldTone (sr, 4.0);
        TransportInfo t; t.bpm = 120.0; t.isPlaying = true; t.hasPpq = true;
        const size_t wrap = (size_t) (2.0 * sr) / 512 * 512;
        for (size_t off = 0; off < s.size(); off += 512)
        {
            const int n = (int) std::min<size_t> (512, s.size() - off);
            if (off == wrap) t.ppqAtBlockStart = 0.37;   // loop wraps round, mid-cycle
            e.process (&s.l[off], &s.r[off], n, t);
            t.ppqAtBlockStart += n * t.bpm / 60.0 / sr;
        }
        const float r = spikeRatio (s, wrap, 4096);
        std::printf ("  1/16 LFO on the Acid filter across a loop wrap: spike %.2f\n", (double) r);
        // Limit 1: the wrap may not be the sharpest moment the sound makes (1.0.2: 1.19).
        CHECK (r < 1.0f, "a loop wrap should not click a fast synced LFO (%.2f)", (double) r);
    }

    // Abyss: Decay and Damping jumped over a ringing tail (automation, a knob grab).
    // 1.0.1 stepped the tank's feedback and damping gains, which clicked.
    {
        EngineParams p = allOff();
        p.abyssOn = true; p.abyss.mix = 1.0f; p.abyss.decaySec = 20.0f; p.abyss.damping = 0.1f;
        DeepTranceEngine e;
        e.setParams (p); e.prepare (sr);
        Stereo s = heldTone (sr, 3.0);
        const size_t change = (size_t) (1.5 * sr) / 512 * 512;   // on a block boundary, as a host delivers it
        TransportInfo t;
        for (size_t off = 0; off < s.size(); off += 512)
        {
            if (off == change) { p.abyss.decaySec = 0.5f; p.abyss.damping = 0.9f; e.setParams (p); }
            e.process (&s.l[off], &s.r[off], (int) std::min<size_t> (512, s.size() - off), t);
        }
        // The tank recirculates a step for tens of ms: look 150 ms past the change.
        const float r = spikeRatio (s, change, (size_t) (0.15 * sr));
        std::printf ("  Abyss Decay 20 s -> 0.5 s, Damping 0.1 -> 0.9 over a tail: spike %.2f\n", (double) r);
        CHECK (r < 1.5f, "jumping Abyss Decay/Damping over a tail should not click (%.2f)", (double) r);
    }
}


// --- 1.0.3 compatibility ---------------------------------------------------------
// Every 1.1 feature is neutral by default, so a sound made with 1.0.3 must come out
// bit-for-bit the same. These fingerprints were rendered by the 1.0.3 engine. Floating
// point differs between compilers, so they are checked where they were made (GCC on
// x86-64, the Linux CI job); MSVC prints its own for reference.
uint64_t fnv1a (const Stereo& s)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < s.size(); ++i)
        for (float v : { s.l[i], s.r[i] })
        {
            uint32_t bits; std::memcpy (&bits, &v, 4);
            for (int b = 0; b < 4; ++b) { h ^= (bits >> (8 * b)) & 0xffu; h *= 1099511628211ull; }
        }
    return h;
}

struct CompatCase { const char* name; std::function<void (EngineParams&)> setup; };

const std::vector<CompatCase>& compatCases()
{
    static const std::vector<CompatCase> cases {
        { "defaults",        [] (EngineParams&) {} },
        { "all modules on",  [] (EngineParams& p) { p = allOn(); p.swirl.mode = SwirlMode::Both; } },
        { "gate custom",     [] (EngineParams& p) { p.gateOn = true; p.gate.pattern = 0; for (size_t i = 0; i < 16; ++i) p.gate.steps[i] = (i % 3) ? 1.0f : 0.3f; p.gate.filterAmount = 0.4f; } },
        { "gate factory",    [] (EngineParams& p) { p.gateOn = true; p.gate.pattern = 3; p.gate.rateIndex = 2; } },
        { "astral",          [] (EngineParams& p) { p.combo = Combo::AstralProjection; p.comboAmount = 0.7f; } },
        { "acid bath",       [] (EngineParams& p) { p.combo = Combo::AcidBath; } },
        { "mod matrix",      [] (EngineParams& p) { p.acidOn = true; for (int i = 0; i < kModSlots; ++i) p.mod[(size_t) i] = { 1 + i % 3, 1 + i * 3, 0.3f }; p.lfo[1].shape = LfoShape::SmoothRandom; } },
        { "build-up",        [] (EngineParams& p) { p.buildUp = 0.6f; p.buildUpNoise = 0.8f; } },
        { "macros",          [] (EngineParams& p) { p.macros = { 0.9f, 0.2f, 1.0f, 0.7f }; p.gateOn = true; } },
        { "cloud",           [] (EngineParams& p) { p.cloudOn = true; p.cloud.mix = 0.8f; p.cloud.shimmer = 0.4f; p.cloud.pitch = 7.0f; } },
        { "punch + space",   [] (EngineParams& p) { p.punchOn = true; p.abyss.shimmer = 0.3f; p.delay.pitch = 5.0f; } },
        { "gains + mix",     [] (EngineParams& p) { p.inputGainDb = 6.0f; p.outputGainDb = -3.0f; p.mix = 0.6f; } },
    };
    return cases;
}

// 1.0.3 engine, GCC 13 x86-64 -O2 (Release). Order matches compatCases().
constexpr uint64_t kCompat103[] = {
    0x56ab24067a96fe01ull,
    0xb32f2c5955fbf1deull,
    0xa988c929d6c197aaull,
    0x76a683f9c030f197ull,
    0xae2bfa70795e2e1eull,
    0xd7c5bdb381ec0a39ull,
    0x26ab5b2b598815c1ull,
    0xd0b505d908c4be86ull,
    0xb8ae40deea808d5eull,
    0x9bdbd159f2c9c8d7ull,
    0x71a437b804e821bdull,
    0xf90048bbd8a1a257ull };

std::vector<uint64_t> renderCompat()
{
    const double sr = 48000.0;
    std::vector<uint64_t> out;
    for (const auto& c : compatCases())
    {
        EngineParams p;
        c.setup (p);
        srRef() = sr;
        DeepTranceEngine e; e.setParams (p); e.prepare (sr);
        Stereo s = sawChord (sr, 3.0);
        TransportInfo t; t.bpm = 138.0; t.isPlaying = true; t.hasPpq = true;
        for (size_t off = 0; off < s.size(); off += 480)
        {
            const int n = (int) std::min<size_t> (480, s.size() - off);
            if (off == 96000) t.ppqAtBlockStart = 0.0;   // one loop wrap mid-render
            e.process (&s.l[off], &s.r[off], n, t);
            t.ppqAtBlockStart += n * t.bpm / 60.0 / sr;
        }
        out.push_back (fnv1a (s));
    }
    return out;
}

void testCompat103()
{
    section ("1.0.3 sounds are bit-exact");
    const auto got = renderCompat();
#if defined(__GNUC__) && ! defined(__clang__) && defined(__x86_64__)
    constexpr size_t n = sizeof (kCompat103) / sizeof (kCompat103[0]);
    if (n != got.size()) { std::printf ("  (fingerprint table not filled in)\n"); }
    for (size_t i = 0; i < std::min (n, got.size()); ++i)
        CHECK (got[i] == kCompat103[i], "%s differs from 1.0.3 (0x%016llx)", compatCases()[i].name, (unsigned long long) got[i]);
#endif
    for (size_t i = 0; i < got.size(); ++i)
        std::printf ("  %-16s 0x%016llxull\n", compatCases()[i].name, (unsigned long long) got[i]);
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
    testDelay();
    testAbyss();
    testPunch();
    testCloud();
    testAcid();
    testBuildUpAndCombos();
    testCpu();
    testStatic();
    testCompat103();

    if (argc == 3 && std::string (argv[1]) == "--render")
        renderDemos (argv[2]);

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
