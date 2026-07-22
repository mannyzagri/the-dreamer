// test_fx_read_bounds.cpp -- TD-001 regression gate: the modfx fractional
// delay-read index math must NEVER produce a one-past-end index.
//
// Root cause (fixed 2026-07-22): in Ensemble/Dimension/Rotary read(),
//   pos = (float)w_ - d;  if (pos < 0) pos += (float)len_;
// when d is 1-2 ulp above w_, the wrap add rounds to EXACTLY (float)len_
// (half-ulp window), so i0 == len_ -> a one-past-end heap read whose 4 bytes
// were returned verbatim (fr == 0). In the deployed 2.5.1 this fired
// deterministically on preset "SOLINA FIELDS" (ENSEMBLE modfx) ~14 s in,
// filling the reverb with ~1e26 garbage that the limiter pinned at the
// -0.1 dBFS ceiling -> the reported "0 dBFS noise after ~20-30 s" (TD-001).
// Fix: re-normalize pos >= len_ to 0.0f (the bit-correct congruent value).
//
// Sections:
//   [trigger]   the PRE-FIX expression, exhaustively over every w_ and the
//               +/-1..4-ulp d neighborhood for all three geometries -> proves
//               the hazard was real and this test can detect it.
//   [fixedmath] the POST-FIX expression over the same exhaustive set -> zero
//               out-of-range indices, and the pos==len_ case lands on buf[0]
//               (the interpolation's continuous limit), fr == 0.
//   [ensemble]  real dreamer::Ensemble, 30 s at the SOLINA FIELDS production
//               params (the faulting trajectory) -> finite, bounded.
//   [dimension] [rotary] real classes, 30 s sweeps -> finite, bounded.
//
// Build (vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I.
//     tests\test_fx_read_bounds.cpp /Fo:tests\bin\ /Fe:tests\bin\test_fx_read_bounds.exe

#include <immintrin.h>
#include <cstdio>
#include <cmath>
#include <cstdint>

#include "dsp/glue/Ensemble.h"
#include "dsp/glue/Dimension.h"
#include "dsp/glue/Rotary.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static float ulpUp(float x, int n) {           // n representable steps above x
    for (int i = 0; i < n; ++i) x = std::nextafterf(x, 1.0e30f);
    return x;
}

// the index computation, pre-fix (as shipped in <= 2.5.1)
static int buggyIndex(int w, float d, int len) {
    float pos = (float)w - d;
    if (pos < 0.0f) pos += (float)len;
    return (int)pos;
}
// the index computation, post-fix (must match the shipped read() helpers)
static int fixedIndex(int w, float d, int len, float* frOut = nullptr) {
    float pos = (float)w - d;
    if (pos < 0.0f) pos += (float)len;
    if (pos >= (float)len) pos = 0.0f;         // TD-001 re-normalization
    const int i0 = (int)pos;
    if (frOut) *frOut = pos - (float)i0;
    return i0;
}

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ/DAZ

    const int lens[3] = { (int)(44100.0 * 0.030) + 4,     // Ensemble  1327
                          (int)(44100.0 * 0.020) + 4,     // Dimension  886
                          (int)(44100.0 * 0.006) + 4 };   // Rotary     268

    // ---- [trigger] pre-fix expression must exhibit the OOB ----------------
    std::printf("[trigger]\n");
    {
        long oob = 0, cases = 0;
        for (int g = 0; g < 3; ++g) {
            const int len = lens[g];
            for (int w = 0; w < len; ++w)
                for (int k = 1; k <= 4; ++k) {           // d = w + k ulps (danger side)
                    const float d = ulpUp((float)w, k);
                    if (d > (float)(len - 2)) continue;  // read() clamps these away
                    ++cases;
                    if (buggyIndex(w, d, len) >= len) ++oob;
                }
        }
        std::printf("  pre-fix: %ld one-past-end indices in %ld boundary cases\n", oob, cases);
        CHECK(oob > 0, "pre-fix math exhibits the OOB (test can detect the hazard)");
    }

    // ---- [fixedmath] post-fix expression: exhaustive, always in bounds ----
    std::printf("[fixedmath]\n");
    {
        long bad = 0, renorm = 0; bool frOk = true;
        for (int g = 0; g < 3; ++g) {
            const int len = lens[g];
            for (int w = 0; w < len; ++w)
                for (int k = 1; k <= 4; ++k) {
                    const float d = ulpUp((float)w, k);
                    if (d > (float)(len - 2)) continue;
                    float fr = 0.0f;
                    const int i0 = fixedIndex(w, d, len, &fr);
                    if (i0 < 0 || i0 >= len) ++bad;
                    if (buggyIndex(w, d, len) >= len) {  // the re-normalized cases
                        ++renorm;
                        if (i0 != 0 || fr != 0.0f) frOk = false;   // limit = buf[0], weight 1
                    }
                }
            // plus a dense fractional sweep (non-boundary sanity)
            for (int w = 0; w < len; ++w)
                for (float d = 1.0f; d <= (float)(len - 2); d += 97.03125f) {
                    const int i0 = fixedIndex(w, d, len);
                    if (i0 < 0 || i0 >= len) ++bad;
                }
        }
        std::printf("  post-fix: %ld out-of-range of ~all cases; %ld renormalized (fr=0 -> buf[0])\n",
                    bad, renorm);
        CHECK(bad == 0, "post-fix index always in [0, len)");
        CHECK(renorm > 0, "the danger cases were exercised");
        CHECK(frOk, "renormalized reads land on buf[0] with weight 1 (continuous limit)");
    }

    // ---- [ensemble] real class, the faulting production trajectory --------
    std::printf("[ensemble]\n");
    {
        dreamer::Ensemble e; e.prepare(44100.0);
        const float rateHz = 0.2f + 0.3f * 5.8f;         // SOLINA FIELDS: rate01 0.3
        bool fin = true; float peak = 0.0f;
        for (long n = 0; n < 44100L * 30; ++n) {
            float l = std::sin((float)n * 0.0312f) * 0.5f, r = l;
            e.process(l, r, rateHz, 0.6f, 0.6f, 0.5f, 0.5f);
            if (!std::isfinite(l) || !std::isfinite(r)) fin = false;
            const float a = std::fmax(std::fabs(l), std::fabs(r));
            if (a > peak) peak = a;
        }
        std::printf("  30s SOLINA params: finite=%d peak=%.4f\n", (int)fin, peak);
        CHECK(fin, "ensemble finite over the faulting trajectory");
        CHECK(peak < 2.0f, "ensemble bounded (no heap garbage in the output)");
    }

    // ---- [dimension] [rotary] real classes, 30 s -------------------------
    std::printf("[dimension]\n");
    {
        dreamer::Dimension d; d.prepare(44100.0);
        bool fin = true; float peak = 0.0f;
        for (long n = 0; n < 44100L * 30; ++n) {
            float l = std::sin((float)n * 0.05f) * 0.5f, r = l;
            d.process(l, r, (float)((n / 44100) % 4) * 0.25f + 0.1f, 0.3f, 1.0f);
            if (!std::isfinite(l)) fin = false;
            peak = std::fmax(peak, std::fabs(l));
        }
        std::printf("  30s mode-stepping: finite=%d peak=%.4f\n", (int)fin, peak);
        CHECK(fin, "dimension finite"); CHECK(peak < 2.0f, "dimension bounded");
    }
    std::printf("[rotary]\n");
    {
        dreamer::Rotary ro; ro.prepare(44100.0);
        bool fin = true; float peak = 0.0f;
        for (long n = 0; n < 44100L * 30; ++n) {
            float l = std::sin((float)n * 0.05f) * 0.5f, r = l;
            const float speed = ((n / 44100) % 2) ? 0.9f : 0.1f;   // chorale<->tremolo
            ro.process(l, r, speed, 0.5f, 0.5f, 0.8f, 1.0f);
            if (!std::isfinite(l)) fin = false;
            peak = std::fmax(peak, std::fabs(l));
        }
        std::printf("  30s speed-toggling: finite=%d peak=%.4f\n", (int)fin, peak);
        CHECK(fin, "rotary finite"); CHECK(peak < 2.0f, "rotary bounded");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
