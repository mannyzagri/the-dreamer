// test_bank3_lib.cpp -- JUCE-free validation of the full v3 sound library
// integration (Bank3 = 78 cycles + 130 loops + 10 HIT one-shots = 218 waves).
//
// Build + run (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O1 /EHsc /bigobj /I. tests\test_bank3_lib.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_bank3_lib.exe
//   tests\bin\test_bank3_lib.exe
//
// Checks:
//   1. kNumWaveforms == 218 (78 + 130 + 10).
//   2. 12-bit invariant: (sample & 0xF) == 0 across ALL 218 entries' data.
//   3. Per-family Loop block counts: Pad 28, Airy 18, Vox 16, Ether 16, FM 14,
//      Wind 12, Metal 12, Morph 14 (counted over the Loop region only, so cycle
//      categories of the same name are not conflated); Hit 10 over the OneShot
//      region.
//   4. Per-Loop wrap-seam delta |samples[length-1] - samples[loopStart]|
//      (reported; max printed; NOT a hard fail).

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "dsp/bank/Bank3.h"

int main() {
    using namespace rompler::bank3;
    int failures = 0;

    // --- 1. total count ---
    std::printf("kNumCycles=%d kNumLoops=%d kNumShots=%d kNumWaveforms=%d\n",
                kNumCycles, kNumLoops, kNumShots, kNumWaveforms);
    if (kNumWaveforms != 218) {
        std::printf("FAIL: kNumWaveforms != 218 (got %d)\n", kNumWaveforms);
        ++failures;
    }
    if (kNumCycles != 78 || kNumLoops != 130 || kNumShots != 10) {
        std::printf("FAIL: expected 78/130/10 cycles/loops/shots\n");
        ++failures;
    }

    // --- 2. 12-bit invariant across every entry's samples ---
    uint64_t totalSamples = 0;
    int nibbleViolations = 0;
    for (int i = 0; i < kNumWaveforms; ++i) {
        const Waveform& w = kWaveforms[(size_t)i];
        for (uint32_t n = 0; n < w.length; ++n) {
            totalSamples += 1;
            if ((w.samples[n] & 0xF) != 0) {
                if (nibbleViolations < 8)
                    std::printf("FAIL 12-bit: %s:%s idx %d sample[%u]=%d (low nibble != 0)\n",
                                w.category, w.name, i, n, w.samples[n]);
                ++nibbleViolations;
            }
        }
    }
    std::printf("scanned %llu int16 samples (%.2f MB embedded) across %d entries\n",
                (unsigned long long)totalSamples,
                (double)totalSamples * 2.0 / 1048576.0, kNumWaveforms);
    if (nibbleViolations) {
        std::printf("FAIL: %d low-nibble-nonzero samples (12-bit invariant broken)\n",
                    nibbleViolations);
        ++failures;
    } else {
        std::printf("12-bit invariant OK: every sample has low nibble == 0\n");
    }

    // --- 3. per-family block counts ---
    struct Fam { const char* cat; int want; };
    const Fam loopFams[] = {
        {"Pad",28},{"Airy",18},{"Vox",16},{"Ether",16},
        {"FM",14},{"Wind",12},{"Metal",12},{"Morph",14},
    };
    // Count categories only within the Loop region (indices 78..207).
    for (const Fam& fam : loopFams) {
        int got = 0;
        for (int i = kNumCycles; i < kNumCycles + kNumLoops; ++i)
            if (kWaveforms[(size_t)i].type == WaveType::Loop &&
                !std::strcmp(kWaveforms[(size_t)i].category, fam.cat))
                ++got;
        std::printf("loop family %-6s want %2d got %2d %s\n",
                    fam.cat, fam.want, got, got == fam.want ? "OK" : "FAIL");
        if (got != fam.want) ++failures;
    }
    // Hit count over the OneShot region.
    {
        int got = 0;
        for (int i = kNumCycles + kNumLoops; i < kNumWaveforms; ++i)
            if (kWaveforms[(size_t)i].type == WaveType::OneShot &&
                !std::strcmp(kWaveforms[(size_t)i].category, "Hit"))
                ++got;
        std::printf("shot family %-6s want %2d got %2d %s\n",
                    "Hit", 10, got, got == 10 ? "OK" : "FAIL");
        if (got != 10) ++failures;
    }

    // --- 4. wrap-seam delta per Loop entry (report only) ---
    int   maxSeam = 0;
    const char* maxSeamName = "";
    for (int i = 0; i < kNumWaveforms; ++i) {
        const Waveform& w = kWaveforms[(size_t)i];
        if (w.type != WaveType::Loop || w.length == 0) continue;
        int a = w.samples[w.length - 1];
        int b = w.samples[w.loopStart];
        int d = a > b ? a - b : b - a;
        if (d > maxSeam) { maxSeam = d; maxSeamName = w.name; }
    }
    std::printf("max wrap-seam delta over %d loops = %d (at %s) "
                "[%.1f quant steps of 16] -- reported, not a hard fail\n",
                kNumLoops, maxSeam, maxSeamName, maxSeam / 16.0);

    if (failures == 0) {
        std::printf("ALL CHECKS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", failures);
    return 1;
}
