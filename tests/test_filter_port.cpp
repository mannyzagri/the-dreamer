// test_filter_port.cpp -- phase-2 gate: RubberFilter port parity + adapter
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_filter_port.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_filter_port.exe
//   tests\bin\test_filter_port.exe
//
// Checks:
//   [parity]  donor rhino::RubberFilter vs ported dreamer::RubberFilter:
//             7 types x oversample {1,2} x res {0, 0.5, 0.985}, fixed-seed
//             noise, identical call cadence -> bitwise identical output.
//             (-120 dB RMS is the documented minimum bar; the port is a
//             namespace rename, so exact equality is expected and enforced.)
//   [adapter] RhinoFilterSlot vs direct setParams(hz, res, Character{}, type)
//             -> bitwise identical output for every type.
//   [sweep]   res = max, cutoff swept 60 -> 12000 Hz at control rate:
//             finite, bounded output for every type at both oversample rates.

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include "C:/rubber-rhino/Source/dsp/RubberFilter.h"   // donor, rhino::
#include "dsp/ported/RhinoFilter.h"                    // port, dreamer::
#include "dsp/glue/RhinoFilterSlot.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

struct Noise {           // xorshift32, deterministic across TUs
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed) {}
    float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (static_cast<int32_t>(s) / 2147483648.0f) * 0.5f;
    }
};

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ + DAZ
    const double SR = 48000.0;
    const int    N  = 48000;             // 1 s per config
    const float  resVals[] = { 0.0f, 0.5f, 0.985f };

    // ---- [parity] -------------------------------------------------------
    std::printf("[parity]\n");
    for (int t = 0; t <= 6; ++t) {
        for (int os = 1; os <= 2; ++os) {
            for (float res : resVals) {
                rhino::RubberFilter   a;
                dreamer::RubberFilter b;
                a.prepare(SR); b.prepare(SR);
                a.setOversample(os); b.setOversample(os);
                const rhino::RubberFilter::Character   ca{};
                const dreamer::RubberFilter::Character cb{};
                Noise na(0xDEADBEEFu), nb(0xDEADBEEFu);
                double sumSq = 0.0, sumRef = 0.0;
                float maxDiff = 0.0f;
                for (int i = 0; i < N; ++i) {
                    if (i % 16 == 0) {   // control-rate cadence, gentle sweep
                        const float hz = 200.0f + 8000.0f * (float)i / (float)N;
                        a.setParams(hz, res, ca, (rhino::RubberFilter::Type)t);
                        b.setParams(hz, res, cb, (dreamer::RubberFilter::Type)t);
                    }
                    const float x  = na.next(); (void)nb.next();
                    const float ya = a.processSample(x);
                    const float yb = b.processSample(x);
                    const float d  = ya - yb;
                    maxDiff = std::fmax(maxDiff, std::fabs(d));
                    sumSq  += (double)d * d;
                    sumRef += (double)ya * ya;
                }
                const double residDb = sumSq > 0.0
                    ? 10.0 * std::log10(sumSq / (sumRef > 0.0 ? sumRef : 1.0))
                    : -999.0;
                if (maxDiff != 0.0f)
                    std::printf("  type %d os %d res %.3f: maxDiff=%g resid=%.1f dB\n",
                                t, os, (double)res, (double)maxDiff, residDb);
                CHECK(maxDiff == 0.0f, "donor vs port bitwise identical");
                CHECK(residDb < -120.0, "residual below -120 dB RMS (minimum bar)");
            }
        }
    }
    std::printf("  7 types x 2 os x 3 res: bitwise identical\n");

    // ---- [adapter] ------------------------------------------------------
    std::printf("[adapter]\n");
    for (int t = 0; t <= 6; ++t) {
        dreamer::RubberFilter direct;
        direct.prepare(SR);
        direct.setOversample(1);
        const dreamer::RubberFilter::Character ch{};
        dreamer::RhinoFilterSlot slot;
        slot.setSampleRate(SR);
        slot.setOversampleFactor(1);
        slot.setType(t);
        Noise na(0x12345678u), nb(0x12345678u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            if (i % 16 == 0) {
                const float hz = 300.0f + 6000.0f * (float)i / (float)N;
                direct.setParams(hz, 0.7f, ch, (dreamer::RubberFilter::Type)t);
                slot.setCutoffRes(hz, 0.7);
            }
            const float x = na.next(); (void)nb.next();
            maxDiff = std::fmax(maxDiff, std::fabs(direct.processSample(x) - slot.process(x)));
        }
        if (maxDiff != 0.0f) std::printf("  type %d: maxDiff=%g\n", t, (double)maxDiff);
        CHECK(maxDiff == 0.0f, "adapter vs direct bitwise identical");
    }
    std::printf("  adapter == direct for all 7 types\n");

    // ---- [sweep] --------------------------------------------------------
    std::printf("[sweep]\n");
    for (int t = 0; t <= 6; ++t) {
        for (int os = 1; os <= 2; ++os) {
            dreamer::RhinoFilterSlot slot;
            slot.setSampleRate(SR);
            slot.setOversampleFactor(os);
            slot.setType(t);
            Noise n(0xCAFEBABEu);
            bool finite = true;
            float peak = 0.0f;
            for (int i = 0; i < N * 2; ++i) {      // up and back down
                if (i % 16 == 0) {
                    const float ph = (float)i / (float)(N * 2);
                    const float up = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
                    slot.setCutoffRes(60.0 + (12000.0 - 60.0) * up, 1.0);
                }
                const float y = slot.process(n.next());
                if (!std::isfinite(y)) { finite = false; break; }
                peak = std::fmax(peak, std::fabs(y));
            }
            CHECK(finite, "sweep output finite at res=max");
            CHECK(peak < 16.0f, "sweep output bounded at res=max");
        }
    }
    std::printf("  res=max sweeps finite/bounded, all types, os 1+2\n");

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
