// test_loop_tuning.cpp -- DSP_BUILD.md section 1b: per-loop measured rootHz.
//
// Build (after vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_loop_tuning.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_loop_tuning.exe
//
// Proves the loop-detune fix:
//   [roots]  every Bank3 Loop entry's rootHz == kLoopRoots[i] (LoopRoots.h),
//            1:1 in manifest order; 15 inharmonic loops (METAL_* + MORPH
//            ETHMETAL/METALAIR/VOXMETAL) pinned to 220.0, the other 115 carry
//            a measured, non-220 root.
//   [ratio]  PcmOsc3 stretch uses the PER-LOOP root: driving a tonal loop at
//            its own rootHz gives inc == 1.0 sample/sample (renders the raw
//            buffer); driving at MIDI A3 (220 Hz) gives inc == 220/rootHz
//            (!= 1.0 for tonal, == 1.0 for METAL) -- i.e. the engine no longer
//            assumes 220 for everyone.
//   [detune] worst-case tonal detune from 220 is tens of cents (the fix is
//            audible), and every tonal root sits within a sane +-60-cent band.

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "dsp/bank/Bank3.h"
#include "dsp/bank/LoopRoots.h"
#include "dsp/glue/PcmOsc3.h"

using namespace rompler;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static bool approx(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
static double cents(double hz, double ref) { return 1200.0 * std::log2(hz / ref); }

// The inharmonic loops that must keep 220.0 nominal (spec s1b).
static bool isInharmonicName(const char* n) {
    return std::strstr(n, "METAL") != nullptr;   // METAL_* and MORPH_*METAL*/METALAIR
}

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ/DAZ

    // ---- [roots] --------------------------------------------------------
    std::printf("[roots]\n");
    int nTonal = 0, nInharmonic = 0, mismatch = 0, wrongPin = 0;
    for (int i = 0; i < bank3::kNumLoops; ++i) {
        const auto& e = bank3::kWaveforms[(size_t)(bank3::kNumCycles + i)];
        // 1:1 with the generated header, manifest order.
        if (e.rootHz != bank3::kLoopRoots[i]) ++mismatch;
        const bool pinned = (e.rootHz == 220.0f);
        if (pinned) ++nInharmonic; else ++nTonal;
        // The pinned set must be exactly the inharmonic (METAL) loops.
        if (pinned != isInharmonicName(e.name)) ++wrongPin;
    }
    std::printf("  loops=%d tonal=%d inharmonic(220)=%d\n",
                bank3::kNumLoops, nTonal, nInharmonic);
    CHECK(mismatch == 0, "every loop rootHz == kLoopRoots[i] (manifest order)");
    CHECK(nInharmonic == 15 && nTonal == 115, "15 inharmonic / 115 tonal");
    CHECK(wrongPin == 0, "only METAL/inharmonic loops are pinned to 220");

    // ---- [ratio] --------------------------------------------------------
    std::printf("[ratio]\n");
    {
        // Sample of tonal loops across families (indices into the loop block).
        const int tonalIdx[] = { 0, 4, 46, 78, 92 };   // PAD_01,PAD_05,VOX_01,FM_01,WIND_01
        for (int li : tonalIdx) {
            const int w = bank3::kNumCycles + li;
            const auto& e = bank3::kWaveforms[(size_t)w];
            const double root = (double)e.rootHz;
            CHECK(root != 220.0, "tonal loop root is not 220");

            dreamer::PcmOsc3 o;
            o.setSampleRate(44100.0);
            o.setWaveform(w);
            o.setInterp(dreamer::PcmOsc3::Interp::Linear);

            // driven at its OWN measured root -> ratio 1.0 (renders raw buffer)
            o.setFrequency(root);
            o.reset(0.0);
            CHECK(approx(o.incSamplesForTest(), 1.0, 1e-4),
                  "freq == measured root -> inc 1.0 sample/sample");
            bool raw = true;
            for (uint32_t i = 0; i < 4000; ++i) {
                const float expect = (float)e.samples[i] * (1.0f / 32768.0f);
                if (o.process() != expect) { raw = false; break; }
            }
            CHECK(raw, "at measured root the loop renders its raw buffer");

            // driven at MIDI A3 (220 Hz) -> engine stretches by 220/root, which is
            // NOT 1.0 for a tonal loop (the whole point: 220 is not the true root).
            o.setFrequency(220.0);
            o.reset(0.0);
            CHECK(approx(o.incSamplesForTest(), 220.0 / root, 1e-4),
                  "at A3 the stretch ratio uses the per-loop root (220/root)");
            CHECK(!approx(o.incSamplesForTest(), 1.0, 1e-4),
                  "tonal loop at A3 is NOT played as raw 220 (detune corrected)");
        }

        // METAL (inharmonic) loop: rootHz == 220 -> A3 gives inc 1.0 exactly.
        const int mw = bank3::kNumCycles + 104;   // METAL_01
        const auto& me = bank3::kWaveforms[(size_t)mw];
        CHECK(me.rootHz == 220.0f, "METAL_01 pinned to 220");
        dreamer::PcmOsc3 mo;
        mo.setSampleRate(44100.0);
        mo.setWaveform(mw);
        mo.setFrequency(220.0);
        mo.reset(0.0);
        CHECK(approx(mo.incSamplesForTest(), 1.0, 1e-9),
              "METAL loop at A3 -> inc 1.0 (nominal root unchanged)");
    }

    // ---- [detune] -------------------------------------------------------
    std::printf("[detune]\n");
    {
        double worst = 0.0; const char* worstName = "";
        int outOfBand = 0;
        for (int i = 0; i < bank3::kNumLoops; ++i) {
            const auto& e = bank3::kWaveforms[(size_t)(bank3::kNumCycles + i)];
            if (e.rootHz == 220.0f) continue;   // inharmonic, no meaningful pitch
            const double c = cents((double)e.rootHz, 220.0);
            if (std::fabs(c) > std::fabs(worst)) { worst = c; worstName = e.name; }
            if (std::fabs(c) > 60.0) ++outOfBand;
        }
        std::printf("  worst tonal detune: %+.1f cents (%s)\n", worst, worstName);
        CHECK(std::fabs(worst) > 20.0, "worst tonal detune is tens of cents (fix matters)");
        CHECK(outOfBand == 0, "all tonal roots within a sane +-60 cent band");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
