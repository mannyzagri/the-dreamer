// bake_shots.cpp -- generates dsp/bank/ShotBankData.h: 10 short one-shot
// transients (DSP_BUILD.md section 1, category "Shot"), 30-200 ms, mono
// 44.1k, synthesized from filtered noise bursts + short cycle fragments
// (fragments borrowed from the v2 cycle bank at build time), 12-bit
// left-justified. C++ stand-in for the spec's bake_shots.py (no python on
// the VM). Deterministic (fixed xorshift seeds) -- regeneration is
// byte-identical.
//
// Build + run (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tools\bake_shots.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\bake_shots.exe
//   tests\bin\bake_shots.exe dsp\bank\ShotBankData.h

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "dsp/bank/RomplerBank.h"   // cycle fragments source

static const double SR = 44100.0;

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed) {}
    double next() {                         // bipolar white, deterministic
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (int32_t)s / 2147483648.0;
    }
};

static int findWave(const char* cat, const char* name) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank::kWaveforms[i].category, cat) &&
            !std::strcmp(rompler::bank::kWaveforms[i].name, name)) return i;
    return 0;
}

static int firstOfCategory(const char* cat) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank::kWaveforms[i].category, cat)) return i;
    return 0;
}

// building blocks ------------------------------------------------------------
static std::vector<double> noiseBurst(double ms, double attMs, double decMs,
                                      double lpCoef, double hp, uint32_t seed) {
    const int n = (int)(SR * ms / 1000.0);
    std::vector<double> out((size_t)n);
    Rng rng(seed);
    double lp = 0.0, dc = 0.0;
    for (int i = 0; i < n; ++i) {
        lp += lpCoef * (rng.next() - lp);                    // one-pole colour
        double v = lp;
        if (hp > 0.0) { dc += hp * (v - dc); v -= dc; }      // tilt
        const double t = i / SR * 1000.0;
        const double env = (t < attMs ? t / attMs : std::exp(-(t - attMs) / decMs));
        out[(size_t)i] = v * env;
    }
    return out;
}

static std::vector<double> cycleFrag(int waveIdx, double hz, double ms,
                                     double decMs, double phase0) {
    const int n = (int)(SR * ms / 1000.0);
    std::vector<double> out((size_t)n);
    const int16_t* c = rompler::bank::kWaveforms[waveIdx].samples;
    double ph = phase0;
    for (int i = 0; i < n; ++i) {
        const int i0 = (int)ph % 600;
        const int i1 = (i0 + 1) % 600;
        const double fr = ph - std::floor(ph);
        const double t = i / SR * 1000.0;
        out[(size_t)i] = (c[i0] * (1 - fr) + c[i1] * fr) / 32768.0 * std::exp(-t / decMs);
        ph += hz * 600.0 / SR;
        if (ph >= 600.0) ph -= 600.0;
    }
    return out;
}

static std::vector<double> mix(std::vector<std::vector<double>> parts) {
    size_t n = 0;
    for (auto& p : parts) n = std::max(n, p.size());
    std::vector<double> out(n, 0.0);
    for (auto& p : parts)
        for (size_t i = 0; i < p.size(); ++i) out[i] += p[i];
    return out;
}

int main(int argc, char** argv) {
    const char* outPath = argc > 1 ? argv[1] : "dsp\\bank\\ShotBankData.h";

    const int bell  = findWave("Bell",  "FM Bell 4");
    const int tine  = findWave("Bell",  "EP Tine 1");
    const int vox   = findWave("Vox",   "Voice 1");
    const int metal = firstOfCategory("Metal");
    const int saw   = findWave("Basic", "Saw");
    const int sine  = findWave("Basic", "Sine");

    struct Shot { const char* name; std::vector<double> s; };
    std::vector<Shot> shots;
    // 10 transients: filtered noise bursts + short cycle fragments
    shots.push_back({ "CHIFF",       mix({ noiseBurst(60, 1, 14, 0.55, 0.02, 11u),
                                           cycleFrag(saw, 880, 45, 12, 0) }) });
    shots.push_back({ "BREATH",      noiseBurst(180, 35, 60, 0.12, 0.01, 22u) });
    shots.push_back({ "CLICK",       noiseBurst(30, 0.3, 4, 0.9, 0.05, 33u) });
    shots.push_back({ "MALLET_TICK", mix({ noiseBurst(35, 0.5, 6, 0.7, 0.0, 44u),
                                           cycleFrag(tine, 660, 60, 18, 150) }) });
    shots.push_back({ "NOISE_SWELL", noiseBurst(200, 120, 55, 0.2, 0.0, 55u) });
    shots.push_back({ "GLASS_TICK",  mix({ cycleFrag(bell, 1320, 80, 22, 0),
                                           noiseBurst(25, 0.5, 5, 0.8, 0.03, 66u) }) });
    shots.push_back({ "VOX_PLOSIVE", mix({ noiseBurst(50, 2, 12, 0.35, 0.02, 77u),
                                           cycleFrag(vox, 220, 90, 30, 0) }) });
    shots.push_back({ "METAL_HIT",   mix({ cycleFrag(metal, 550, 120, 35, 300),
                                           noiseBurst(40, 0.5, 10, 0.6, 0.0, 88u) }) });
    shots.push_back({ "SUB_THUMP",   mix({ cycleFrag(sine, 55, 120, 40, 0),
                                           noiseBurst(20, 0.5, 4, 0.5, 0.0, 99u) }) });
    shots.push_back({ "TAPE_START",  noiseBurst(150, 90, 40, 0.08, 0.015, 111u) });

    FILE* o = std::fopen(outPath, "wb");
    if (!o) { std::printf("cannot open %s\n", outPath); return 1; }
    std::fprintf(o,
        "// Auto-generated by tools/bake_shots.cpp -- DO NOT EDIT.\n"
        "// One-shot transients (DSP_BUILD.md section 1, category Shot), mono 44.1k,\n"
        "// 12-bit left-justified int16 (low nibble zero), root 220 Hz convention.\n"
        "#pragma once\n#include <cstdint>\n\nnamespace rompler::bank::shotdata {\n\n");

    std::vector<uint32_t> lengths;
    for (auto& sh : shots) {
        // peak-normalize to 0.9995, quantize 12-bit
        double peak = 0.0;
        for (double v : sh.s) peak = std::fmax(peak, std::fabs(v));
        const double g = peak > 0.0 ? 0.9995 / peak : 1.0;
        lengths.push_back((uint32_t)sh.s.size());
        std::fprintf(o, "inline constexpr int16_t sh_%s[%u] = {",
                     sh.name, (unsigned)sh.s.size());
        for (size_t i = 0; i < sh.s.size(); ++i) {
            if (i % 12 == 0) std::fprintf(o, "\n  ");
            int q = (int)std::lround(sh.s[i] * g * 2047.0);
            if (q > 2047) q = 2047; if (q < -2048) q = -2048;
            std::fprintf(o, "%6d,", (int16_t)(q * 16));
        }
        std::fprintf(o, "\n};\n\n");
        std::printf("%-12s %6u samples (%.0f ms)\n", sh.name,
                    (unsigned)sh.s.size(), sh.s.size() / SR * 1000.0);
    }

    std::fprintf(o, "struct ShotEntry { const char* name; const int16_t* samples; uint32_t length; };\n");
    std::fprintf(o, "inline constexpr ShotEntry kShots[%d] = {\n", (int)shots.size());
    for (size_t i = 0; i < shots.size(); ++i)
        std::fprintf(o, "  { \"%s\", sh_%s, %uu },\n",
                     shots[i].name, shots[i].name, (unsigned)lengths[i]);
    std::fprintf(o, "};\n\n} // namespace rompler::bank::shotdata\n");
    std::fclose(o);
    std::printf("%d shots written\n", (int)shots.size());
    return 0;
}
