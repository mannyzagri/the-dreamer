// bake_loops_header.cpp -- packs assets/loops/*.wav (the v3 sound-library
// deliverable: 16-bit mono 44.1k PCM family loops) into dsp/bank/LoopBankData.h
// as 12-bit-left-justified int16 arrays (round-to-nearest 12-bit, low nibble
// zero by construction -- the bank invariant). This is the DSP_BUILD.md
// section 1 "--emit-header mode", implemented as a C++ tool (no-python house
// rule).
//
// Build + run (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc tools\bake_loops_header.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\bake_loops_header.exe
//   tests\bin\bake_loops_header.exe assets\loops dsp\bank\LoopBankData.h
//
// Loop order below = assets/library-src/manifest.json order (LOCKED: the
// wave-choice param index depends on it, so loops are emitted EXACTLY in this
// order). Each loop carries a per-family category derived from its filename
// prefix (PAD->Pad, AIRY->Airy, VOX->Vox, ETHER->Ether, FM->FM, WIND->Wind,
// METAL->Metal, MORPH->Morph). Bank3 sets rootHz 220.0 / loopStart 0.
//
// MSVC compile-size note: 130 loops (~28 MB int16) => this header is ~110+ MB
// of text. The big sample arrays are emitted as `inline const int16_t` (NOT
// constexpr) so MSVC does not perform constexpr evaluation over every element;
// Bank3's constexpr makeTable() only stores each array's ADDRESS (the address
// of an inline const array with static storage duration is a constant
// expression), so it still compiles as constexpr.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

// Loop order = manifest.json (LOCKED). 130 names.
static const char* kLoopNames[] = {
    // PAD (28)
    "PAD_01", "PAD_02", "PAD_03", "PAD_04", "PAD_05", "PAD_06", "PAD_07",
    "PAD_08", "PAD_09", "PAD_10", "PAD_11", "PAD_12", "PAD_13", "PAD_14",
    "PAD_15", "PAD_16", "PAD_17", "PAD_18", "PAD_19", "PAD_20", "PAD_21",
    "PAD_22", "PAD_23", "PAD_24", "PAD_25", "PAD_26", "PAD_27", "PAD_28",
    // AIRY (18)
    "AIRY_01", "AIRY_02", "AIRY_03", "AIRY_04", "AIRY_05", "AIRY_06",
    "AIRY_07", "AIRY_08", "AIRY_09", "AIRY_10", "AIRY_11", "AIRY_12",
    "AIRY_13", "AIRY_14", "AIRY_15", "AIRY_16", "AIRY_17", "AIRY_18",
    // VOX (16)
    "VOX_01", "VOX_02", "VOX_03", "VOX_04", "VOX_05", "VOX_06", "VOX_07",
    "VOX_08", "VOX_09", "VOX_10", "VOX_11", "VOX_12", "VOX_13", "VOX_14",
    "VOX_15", "VOX_16",
    // ETHER (16)
    "ETHER_01", "ETHER_02", "ETHER_03", "ETHER_04", "ETHER_05", "ETHER_06",
    "ETHER_07", "ETHER_08", "ETHER_09", "ETHER_10", "ETHER_11", "ETHER_12",
    "ETHER_13", "ETHER_14", "ETHER_15", "ETHER_16",
    // FM (14)
    "FM_01", "FM_02", "FM_03", "FM_04", "FM_05", "FM_06", "FM_07", "FM_08",
    "FM_09", "FM_10", "FM_11", "FM_12", "FM_13", "FM_14",
    // WIND (12)
    "WIND_01", "WIND_02", "WIND_03", "WIND_04", "WIND_05", "WIND_06",
    "WIND_07", "WIND_08", "WIND_09", "WIND_10", "WIND_11", "WIND_12",
    // METAL (12)
    "METAL_01", "METAL_02", "METAL_03", "METAL_04", "METAL_05", "METAL_06",
    "METAL_07", "METAL_08", "METAL_09", "METAL_10", "METAL_11", "METAL_12",
    // MORPH (14)
    "MORPH_PADAIR", "MORPH_VOXETHER", "MORPH_STRWIND", "MORPH_ETHERFM",
    "MORPH_VOXMETAL", "MORPH_AIRGLASS", "MORPH_FMBELL", "MORPH_PADVOX",
    "MORPH_WINDVOX", "MORPH_METALAIR", "MORPH_PADPAD", "MORPH_VOXVOX",
    "MORPH_ETHMETAL", "MORPH_FMPAD",
};
static const int kNumLoops = (int)(sizeof(kLoopNames) / sizeof(kLoopNames[0]));

// Per-family category from filename prefix. Order matters: check "METAL_"
// before "MORPH_"? -- they differ, but both start 'M'; rfind(prefix,0) handles
// full-prefix match so ordering is irrelevant. Kept explicit for clarity.
static const char* categoryFor(const std::string& n) {
    if (n.rfind("PAD_",   0) == 0) return "Pad";
    if (n.rfind("AIRY_",  0) == 0) return "Airy";
    if (n.rfind("VOX_",   0) == 0) return "Vox";
    if (n.rfind("ETHER_", 0) == 0) return "Ether";
    if (n.rfind("FM_",    0) == 0) return "FM";
    if (n.rfind("WIND_",  0) == 0) return "Wind";
    if (n.rfind("METAL_", 0) == 0) return "Metal";
    if (n.rfind("MORPH_", 0) == 0) return "Morph";
    return "Loop";
}

// minimal RIFF/WAVE reader: 16-bit mono PCM, chunk-walks to "data"
static bool readWav(const std::string& path, std::vector<int16_t>& out, uint32_t& sr) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char id[4]; uint32_t sz;
    if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4)) { std::fclose(f); return false; }
    std::fread(&sz, 4, 1, f);
    std::fread(id, 1, 4, f);
    if (std::memcmp(id, "WAVE", 4)) { std::fclose(f); return false; }
    uint16_t fmt = 0, ch = 0, bits = 0;
    sr = 0;
    while (std::fread(id, 1, 4, f) == 4 && std::fread(&sz, 4, 1, f) == 1) {
        if (!std::memcmp(id, "fmt ", 4)) {
            uint16_t ba; uint32_t br;
            std::fread(&fmt, 2, 1, f); std::fread(&ch, 2, 1, f);
            std::fread(&sr, 4, 1, f);  std::fread(&br, 4, 1, f);
            std::fread(&ba, 2, 1, f);  std::fread(&bits, 2, 1, f);
            if (sz > 16) std::fseek(f, (long)sz - 16, SEEK_CUR);
        } else if (!std::memcmp(id, "data", 4)) {
            out.resize(sz / 2);
            std::fread(out.data(), 2, out.size(), f);
            std::fclose(f);
            return fmt == 1 && ch == 1 && bits == 16;
        } else {
            std::fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
        }
    }
    std::fclose(f);
    return false;
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "assets\\loops";
    const char* outPath   = argc > 2 ? argv[2] : "dsp\\bank\\LoopBankData.h";

    FILE* o = std::fopen(outPath, "wb");
    if (!o) { std::printf("cannot open %s\n", outPath); return 1; }
    std::fprintf(o,
        "// Auto-generated by tools/bake_loops_header.cpp from assets/loops/*.wav\n"
        "// (v3 sound-library deliverable) -- DO NOT EDIT. 12-bit left-justified\n"
        "// int16 (low nibble zero), mono 44.1k, full-buffer family loops, root\n"
        "// 220 Hz. Order = assets/library-src/manifest.json (LOCKED). Sample\n"
        "// arrays are `inline const` (not constexpr) to keep MSVC compile of a\n"
        "// ~110 MB header tractable; kLoops stores addresses only.\n"
        "#pragma once\n#include <cstdint>\n\nnamespace rompler::bank::loopdata {\n\n");

    uint64_t total = 0;
    std::vector<uint32_t> lengths;
    for (int i = 0; i < kNumLoops; ++i) {
        std::vector<int16_t> s;
        uint32_t sr = 0;
        const std::string path = dir + "\\" + kLoopNames[i] + ".wav";
        if (!readWav(path, s, sr) || sr != 44100 || s.empty()) {
            std::printf("FAILED reading %s (sr=%u n=%zu)\n", path.c_str(), sr, s.size());
            std::fclose(o);
            return 1;
        }
        lengths.push_back((uint32_t)s.size());
        total += s.size();
        std::fprintf(o, "inline const int16_t lp_%s[%u] = {",
                     kLoopNames[i], (unsigned)s.size());
        for (size_t n = 0; n < s.size(); ++n) {
            if (n % 12 == 0) std::fprintf(o, "\n  ");
            int q = (int)std::lround(s[n] / 16.0);          // 12-bit round-to-nearest
            if (q > 2047) q = 2047; if (q < -2048) q = -2048;
            std::fprintf(o, "%6d,", (int16_t)(q * 16));     // left-justified
        }
        std::fprintf(o, "\n};\n\n");
        std::printf("%-16s %-6s %7u samples\n",
                    kLoopNames[i], categoryFor(kLoopNames[i]), (unsigned)s.size());
    }

    std::fprintf(o, "struct LoopEntry { const char* name; const char* category; const int16_t* samples; uint32_t length; };\n");
    std::fprintf(o, "inline constexpr LoopEntry kLoops[%d] = {\n", kNumLoops);
    for (int i = 0; i < kNumLoops; ++i)
        std::fprintf(o, "  { \"%s\", \"%s\", lp_%s, %uu },\n",
                     kLoopNames[i], categoryFor(kLoopNames[i]), kLoopNames[i],
                     (unsigned)lengths[(size_t)i]);
    std::fprintf(o, "};\n\n} // namespace rompler::bank::loopdata\n");
    std::fclose(o);
    std::printf("total %llu samples (%.2f MB as int16), %d loops\n",
                (unsigned long long)total, (double)total * 2.0 / 1048576.0, kNumLoops);
    return 0;
}
