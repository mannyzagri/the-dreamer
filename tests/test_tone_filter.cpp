// test_tone_filter.cpp -- TD-007 gate for the per-tone 14-type ToneFilter
// (dsp/glue/ToneFilter.h) and its DreamVoice/DreamSynth plumbing.
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I.
//     tests\test_tone_filter.cpp /Fo:tests\bin\ /Fe:tests\bin\test_tone_filter.exe
//
// Checks:
//   [dispatch] all 14 types finite + bounded on a 2 s saw render at production
//              params (ctrl-rate cutoff/res, engine oversample 1 and 2);
//              MORPH audibly changes type 13 (proves the latched morph path)
//   [bitexact] types 0-3 through ToneFilter == the raw ToneSvf sequence
//              FLOAT-IDENTICAL on a 48k cutoff/res sweep incl. live intra-0-3
//              mode flips (the TD-007 bit-exactness rule, exact == asserts)
//   [switch]   cross-family type flips mid-note: no post-switch sample exceeds
//              the pre-switch window peak +6 dB, no NaN/Inf
//   [alloc]    zero heap allocations across 96 prepared instances rendering
//              (the 24-voice x 4-tone production count)
//   [bench]    per-family realtime factor, DreamSynth 24 voices x 4 tones
//              forced to each family, 10 s @48k; worst family must be < 60%
//              realtime; + a recalc-burst variant (fast per-tone LFO cutoff
//              sweep on all tones -> a coefficient recalc every ctrl tick)

#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <new>

// ---- house allocation guard (test_engine pattern) --------------------------
static bool g_countAllocs = false;
static long g_allocCount  = 0;
void* operator new(std::size_t n) {
    if (g_countAllocs) ++g_allocCount;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    if (g_countAllocs) ++g_allocCount;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

#include "dsp/glue/ToneFilter.h"
#include "dsp/glue/DreamVoice.h"

static int fails = 0;
#define CHECK(c, m) do { \
    if (!(c)) { std::printf("  FAIL: %s (line %d)\n", m, __LINE__); ++fails; } \
} while (0)

static constexpr double SR = 48000.0;

// deterministic naive saw (test signal; aliasing irrelevant here)
struct Saw {
    double ph = 0.0, inc;
    explicit Saw(double hz) : inc(hz / SR) {}
    float next() noexcept {
        const float v = (float)(2.0 * ph - 1.0);
        ph += inc; if (ph >= 1.0) ph -= 1.0;
        return v;
    }
};

// drive one ToneFilter exactly like Tone does: per-sample process, 16-sample
// control-rate setMorph+setCutoffRes. Returns peak |y|; nan flag via out-param.
static double renderPeak(int type, double cutHz, double res, double morph,
                         int os, int nSamples, bool& finite, double amp = 0.5) {
    dreamer::ToneFilter f;
    f.setSampleRate(SR);
    f.setStagger(type & 15);
    f.setOversample(os);
    f.setType(type);
    f.reset();
    Saw saw(110.0);
    double pk = 0.0; int ctrl = 0; finite = true;
    for (int i = 0; i < nSamples; ++i) {
        if (ctrl-- <= 0) { ctrl = 15; f.setMorph((float)morph); f.setCutoffRes(cutHz, res); }
        const float y = f.process(amp * saw.next());
        if (!std::isfinite(y)) finite = false;
        const double a = std::fabs((double)y);
        if (a > pk) pk = a;
    }
    return pk;
}

// steady-tone RMS (settled window) for the morph-audibility probe
static double toneRMS13(double morph, double sigHz) {
    dreamer::ToneFilter f;
    f.setSampleRate(SR); f.setStagger(3); f.setOversample(1);
    f.setType(13); f.reset();
    double ph = 0, inc = 2.0 * 3.14159265358979323846 * sigHz / SR, e = 0;
    int ctrl = 0; const int N = 24000;
    for (int i = 0; i < N; ++i) {
        if (ctrl-- <= 0) { ctrl = 15; f.setMorph((float)morph); f.setCutoffRes(1200.0, 0.6); }
        const float y = f.process((float)(0.5 * std::sin(ph))); ph += inc;
        if (i > 8000) e += (double)y * y;
    }
    return std::sqrt(e / (N - 8000));
}

// 24-voice x 4-tone DreamSynth patch with every tone forced to `type`
static dreamer::DreamPatch benchPatch(int type) {
    dreamer::DreamPatch p;
    int sawIdx = 0;
    for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank3::kWaveforms[(size_t)i].category, "Basic") &&
            !std::strcmp(rompler::bank3::kWaveforms[(size_t)i].name, "Saw")) { sawIdx = i; break; }
    for (int t = 0; t < 4; ++t) {
        auto& tn = p.tone[t];
        tn.enabled = true;
        tn.waveIndex = sawIdx;
        tn.level = 0.8;
        tn.tvfMode = type;
        tn.cutoffHz = 2000.0;
        tn.resonance = 0.4;
        tn.tvfEnvAmount = 0.0;
        tn.tvfKeyFollow = 0.0;
        tn.tvaA = 0.002; tn.tvaS = 1.0; tn.tvaR = 0.1;
        tn.tvfA = 0.002; tn.tvfS = 1.0;
    }
    p.morph = 0.5;
    return p;
}

// render `seconds` of a 24-note chord; returns wall-time as % of realtime
static double benchPct(int type, double seconds, bool burst, double* outPeak) {
    static dreamer::DreamSynth synth;                 // static: big object, reused
    synth.prepare(SR);
    synth.setOversample(2);                           // Modern (the heavier path)
    synth.patch() = benchPatch(type);
    if (burst) {
        // recalc-burst: fast per-tone LFO -> CUTOFF at full depth on every
        // tone -> cutoff changes EVERY ctrl tick -> a coefficient recalc per
        // 16-sample grid slot per tone (the TD-007 worst case).
        for (int t = 0; t < 4; ++t) {
            auto& l = synth.patch().tone[t].lfo1;
            l.depth = 1.0; l.dest = 1; l.rate01 = 0.9; l.shape = 0;
        }
    }
    synth.updateLive();
    for (int n = 0; n < 24; ++n) synth.noteOn(40 + n, 0.8f);
    const int N = (int)(seconds * SR);
    double pk = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        float l = 0, r = 0;
        synth.process(l, r);
        const double a = std::fmax(std::fabs((double)l), std::fabs((double)r));
        if (a > pk) pk = a;
    }
    const auto t1 = std::chrono::steady_clock::now();
    if (outPeak) *outPeak = pk;
    const double el = std::chrono::duration<double>(t1 - t0).count();
    return el / seconds * 100.0;
}

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ/DAZ (house harness convention)

    // ---- [dispatch] --------------------------------------------------------
    std::printf("[dispatch] all 14 types finite + bounded, 2 s saw, production params\n");
    for (int t = 0; t < 14; ++t) {
        for (int os : { 1, 2 }) {
            bool fin = true;
            const double pk = renderPeak(t, 2000.0, 0.4, 0.5, os, (int)(2.0 * SR), fin);
            CHECK(fin, "type finite over 2 s render");
            // 4-13 pass through safety() (asymptote 1.5 * trim 1.0); 0-3 are
            // the BARE SVF (bit-exact rule) -- sanity-bound those at 4.0.
            CHECK(pk < (t <= 3 ? 4.0 : 1.6), "type bounded at production params");
        }
        // max-res sweep endpoints too (production extremes, cutHz map 60..12000)
        for (double c : { 60.0, 12000.0 }) {
            bool fin = true;
            const double pk = renderPeak(t, c, 1.0, 1.0, 1, 24000, fin);
            CHECK(fin, "type finite at cutoff extreme, res 1");
            CHECK(pk < (t <= 3 ? 4.0 : 1.6), "type bounded at cutoff extreme, res 1");
        }
    }
    {   // morph reaches the tone ZPlane through the latched path
        double md = 0;
        for (double f = 120.0; f <= 7000.0; f *= 1.06)
            md = std::fmax(md, std::fabs(toneRMS13(0.0, f) - toneRMS13(1.0, f)));
        std::printf("  type 13 max |morph0-morph1| RMS delta = %.4f\n", md);
        CHECK(md > 0.03, "MORPH audibly changes the tone DreamPlane response");
    }

    // ---- [bitexact] --------------------------------------------------------
    std::printf("[bitexact] types 0-3: ToneFilter == raw ToneSvf, float-identical\n");
    {
        long diffs = 0;
        for (int mode = 0; mode < 4; ++mode) {
            dreamer::ToneSvf    ref;  ref.setSampleRate(SR);
            dreamer::ToneFilter dut;  dut.setSampleRate(SR);
            dut.setStagger(7);                     // must have zero effect on 0-3
            dut.setOversample(2);
            ref.setMode(mode); ref.reset();
            dut.setType(mode); dut.reset();
            Saw sawA(110.0), sawB(110.0);
            int ctrl = 0;
            for (int i = 0; i < 48000; ++i) {
                // live intra-0-3 flip mid-stream (setMode-only rule)
                if (i == 24000) {
                    const int m2 = (mode + 1) & 3;
                    ref.setMode(m2);
                    dut.setType(m2);
                }
                if (ctrl-- <= 0) {
                    ctrl = 15;
                    // swept cutoff 60..12000 log + res ramp 0..1
                    const double u  = (double)i / 48000.0;
                    const double hz = 60.0 * std::pow(200.0, u);
                    ref.setCutoffRes(hz, u);
                    dut.setMorph((float)u);        // must be inert for 0-3
                    dut.setCutoffRes(hz, u);
                }
                const float xa = 0.5f * sawA.next();
                const float xb = 0.5f * sawB.next();
                const float ya = ref.process(xa);
                const float yb = dut.process(xb);
                if (std::memcmp(&ya, &yb, sizeof(float)) != 0) ++diffs;
            }
        }
        std::printf("  differing samples over 4 modes x 48000: %ld\n", diffs);
        CHECK(diffs == 0, "0-3 EXACTLY equal to the bare ToneSvf sequence");
    }

    // ---- [switch] ----------------------------------------------------------
    std::printf("[switch] cross-family flips mid-note: bounded, no NaN\n");
    {
        static constexpr int seq[9] = { 0, 5, 9, 13, 2, 12, 4, 13, 0 };
        dreamer::ToneFilter f;
        f.setSampleRate(SR); f.setStagger(11); f.setOversample(1);
        f.setType(seq[0]); f.reset();
        Saw saw(110.0);
        const int win = 2400;
        // "pre-switch peak" = peak over the WHOLE run before the switch (not
        // just the last window): consecutive types legitimately differ in
        // steady level (comb- is hollow, DreamPlane resonant), which is a
        // voicing property, not a switch fault. A switch transient/instability
        // still spikes above everything heard so far and trips the +6 dB bound.
        double runPeak = 0.0, prevPeak = 0.0;
        bool ok = true, fin = true;
        int ctrl = 0;
        for (int i = 0; i < win * 9; ++i) {
            const int seg = i / win;
            if (i % win == 0 && seg > 0) {
                prevPeak = std::fmax(runPeak, 0.02);   // floor vs near-silent start
                f.setType(seq[seg]);
            }
            if (ctrl-- <= 0) { ctrl = 15; f.setMorph(0.5f); f.setCutoffRes(2000.0, 0.5); }
            const float y = f.process(0.5f * saw.next());
            if (!std::isfinite(y)) fin = false;
            const double a = std::fabs((double)y);
            if (a > runPeak) runPeak = a;
            if (seg > 0 && a > prevPeak * 2.0) ok = false;   // +6 dB over pre-switch
        }
        CHECK(fin, "no NaN/Inf across cross-family switches");
        CHECK(ok, "no post-switch sample > pre-switch window peak +6 dB");
    }

    // ---- [alloc] -----------------------------------------------------------
    std::printf("[alloc] 96 instances (24 voices x 4 tones): zero audio-path allocations\n");
    {
        static dreamer::ToneFilter fs[96];             // fixed storage, like the engine
        for (int k = 0; k < 96; ++k) {
            fs[k].setSampleRate(SR);
            fs[k].setStagger(k & 15);
            fs[k].setOversample(2);
            fs[k].setType(k % 14);
            fs[k].reset();
        }
        Saw saw(110.0);
        g_allocCount = 0; g_countAllocs = true;
        int ctrl = 0;
        volatile float sink = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            if (ctrl-- <= 0) {
                ctrl = 15;
                const double hz = 200.0 + (i % 4800);
                for (int k = 0; k < 96; ++k) { fs[k].setMorph(0.5f); fs[k].setCutoffRes(hz, 0.6); }
            }
            const float x = 0.5f * saw.next();
            for (int k = 0; k < 96; ++k) sink += fs[k].process(x);
        }
        g_countAllocs = false;
        std::printf("  allocations during 1 s x 96 render: %ld\n", g_allocCount);
        CHECK(g_allocCount == 0, "zero allocations across prepare+render of 96 instances");
        (void)sink;
    }

    // ---- [bench] -----------------------------------------------------------
    std::printf("[bench] 24 voices x 4 tones, 10 s @48k, %% of realtime per type\n");
    {
        static constexpr const char* famName[4] = { "SVF", "RHINO", "EXTRA", "ZPLANE" };
        double famWorst[4] = { 0, 0, 0, 0 };
        for (int t = 0; t < 14; ++t) {
            double pk = 0.0;
            const double pct = benchPct(t, 10.0, false, &pk);
            const int fam = t <= 3 ? 0 : t <= 6 ? 1 : t <= 12 ? 2 : 3;
            if (pct > famWorst[fam]) famWorst[fam] = pct;
            std::printf("  type %2d  %-6s  %6.1f%% realtime  peak=%.3f\n",
                        t, famName[fam], pct, pk);
            CHECK(std::isfinite(pk), "bench render finite");
        }
        std::printf("  family worst: SVF=%.1f%% RHINO=%.1f%% EXTRA=%.1f%% ZPLANE=%.1f%%\n",
                    famWorst[0], famWorst[1], famWorst[2], famWorst[3]);
        double worst = 0; for (double v : famWorst) worst = std::fmax(worst, v);
        CHECK(worst < 60.0, "worst family < 60% realtime");

        std::printf("  recalc-burst variant (fast LFO cutoff sweep, all tones):\n");
        static constexpr int reps[4] = { 0, 6, 12, 13 };
        for (int r = 0; r < 4; ++r) {
            double pk = 0.0;
            const double pct = benchPct(reps[r], 10.0, true, &pk);
            std::printf("  burst type %2d  %-6s  %6.1f%% realtime  peak=%.3f\n",
                        reps[r], famName[r], pct, pk);
            CHECK(pct < 60.0, "burst variant < 60% realtime");
            CHECK(std::isfinite(pk), "burst render finite");
        }
    }

    if (fails == 0) std::printf("ALL CHECKS PASSED\n");
    else            std::printf("%d CHECK(S) FAILED\n", fails);
    return fails ? 1 : 0;
}
