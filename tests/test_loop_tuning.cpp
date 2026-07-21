// test_loop_tuning.cpp -- v3 loop tuning (LoopRoots.h).
//
// Build (after vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_loop_tuning.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_loop_tuning.exe
//
// The v3 library is SYNTHESIZED at a fixed 220 Hz nominal (bake_final.py:
// root=220.0, "no rootHz correction, no pitch-lock warp"), so every loop's root
// is 220.0 and the engine stretch noteHz/rootHz plays each loop IN TUNE.
//   [roots]  every Bank3 Loop entry's rootHz == kLoopRoots[i] (LoopRoots.h),
//            1:1 in manifest order, and every one == 220.0.
//   [ratio]  PcmOsc3 stretch: driving a loop at 220 Hz (MIDI A3) gives inc ==
//            1.0 sample/sample (renders the raw buffer = plays in tune); 2*220
//            gives inc 2.0. Uniform across all loops (root is 220 for all).
//   [intune] worst-case tonal detune from 220 is 0 cents (all loops at nominal).
//
// Regression fixed in 2.5.1: the header previously held stale v2-MEASURED roots
// (off +-up to ~45 c); playing the v3 audio through them detuned every tonal
// loop ~+13 cents sharp. This test now guards the v3 all-220 truth.

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

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ/DAZ

    // ---- [roots] --------------------------------------------------------
    std::printf("[roots]\n");
    int nAt220 = 0, notAt220 = 0, mismatch = 0;
    for (int i = 0; i < bank3::kNumLoops; ++i) {
        const auto& e = bank3::kWaveforms[(size_t)(bank3::kNumCycles + i)];
        if (e.rootHz != bank3::kLoopRoots[i]) ++mismatch;   // 1:1 with the header
        if (e.rootHz == 220.0f) ++nAt220; else ++notAt220;
    }
    std::printf("  loops=%d at220=%d notAt220=%d\n", bank3::kNumLoops, nAt220, notAt220);
    CHECK(mismatch == 0, "every loop rootHz == kLoopRoots[i] (manifest order)");
    CHECK(nAt220 == bank3::kNumLoops && notAt220 == 0,
          "v3: all 130 loops at 220 Hz nominal (synthesized, no per-loop detune)");

    // ---- [ratio] --------------------------------------------------------
    std::printf("[ratio]\n");
    {
        // Sample of loops across families (indices into the loop block).
        const int idx[] = { 0, 4, 46, 78, 92, 104 };   // PAD_01,PAD_05,VOX_01,FM_01,WIND_01,METAL_01
        for (int li : idx) {
            const int w = bank3::kNumCycles + li;
            const auto& e = bank3::kWaveforms[(size_t)w];
            const double root = (double)e.rootHz;
            CHECK(root == 220.0, "v3 loop root is 220 (generated at nominal)");

            dreamer::PcmOsc3 o;
            o.setSampleRate(44100.0);
            o.setWaveform(w);
            o.setInterp(dreamer::PcmOsc3::Interp::Linear);

            // driven at MIDI A3 (220 Hz) == root -> ratio 1.0, renders raw buffer
            // (the loop plays at its true pitch: IN TUNE, no correction needed).
            o.setFrequency(220.0);
            o.reset(0.0);
            CHECK(approx(o.incSamplesForTest(), 1.0, 1e-9),
                  "v3 loop at A3 plays in tune (root 220 -> inc 1.0)");
            bool raw = true;
            for (uint32_t i = 0; i < 4000; ++i) {
                const float expect = (float)e.samples[i] * (1.0f / 32768.0f);
                if (o.process() != expect) { raw = false; break; }
            }
            CHECK(raw, "at A3 the loop renders its raw buffer");

            // one octave up (2*220) -> inc exactly 2.0 (skips every other sample)
            o.setFrequency(440.0);
            o.reset(0.0);
            CHECK(approx(o.incSamplesForTest(), 2.0, 1e-9),
                  "A4 (2x root) -> inc 2.0");
        }
    }

    // ---- [intune] -------------------------------------------------------
    std::printf("[intune]\n");
    {
        double worst = 0.0; const char* worstName = "";
        for (int i = 0; i < bank3::kNumLoops; ++i) {
            const auto& e = bank3::kWaveforms[(size_t)(bank3::kNumCycles + i)];
            const double c = cents((double)e.rootHz, 220.0);   // 0 for all v3 loops
            if (std::fabs(c) > std::fabs(worst)) { worst = c; worstName = e.name; }
        }
        std::printf("  worst root-vs-220 detune: %+.1f cents (%s)\n", worst, worstName);
        CHECK(std::fabs(worst) < 1e-6,
              "v3: no per-loop detune -- every loop plays in tune at its note");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
