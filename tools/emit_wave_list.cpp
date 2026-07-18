// emit_wave_list.cpp -- regenerates the GUI's WAVES array from Bank3
// (order LOCKED: array index = wave_[t] choice index). Tags: cycle = no tag,
// Loop = "ENS", OneShot = "SHOT" (GUI_SPEC wave-overlay tags).
//   cl /nologo /std:c++17 /O2 /EHsc /I. tools\emit_wave_list.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\emit_wave_list.exe
//   tests\bin\emit_wave_list.exe design-handoff\v6\wave-list-bank3.js
#include <cstdio>
#include "dsp/bank/Bank3.h"

int main(int argc, char** argv) {
    const char* outPath = argc > 1 ? argv[1] : "design-handoff\\v6\\wave-list-bank3.js";
    FILE* f = std::fopen(outPath, "wb");
    if (!f) return 1;
    std::fprintf(f, "// Bank3 order is LOCKED (wave_[t] choice index = array index)\n");
    std::fprintf(f, "const WAVES = [\n");
    for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i) {
        const auto& e = rompler::bank3::kWaveforms[(size_t)i];
        const char* tag = e.type == rompler::bank3::WaveType::Loop ? "ENS"
                        : e.type == rompler::bank3::WaveType::OneShot ? "SHOT" : "";
        std::fprintf(f, "  { cat: \"%s\", name: \"%s\", tag: \"%s\" },   // %d\n",
                     e.category, e.name, tag, i);
    }
    std::fprintf(f, "];\n");
    std::fclose(f);
    std::printf("wrote %s (%d waves)\n", outPath, rompler::bank3::kNumWaveforms);
    return 0;
}
