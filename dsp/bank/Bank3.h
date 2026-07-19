// Bank3 -- The Dreamer bank v3 (DSP_BUILD.md section 1): three entry types.
// 78 CYCLE entries (v2 bank, unchanged) + 130 LOOP entries (full v3 sound
// library, per-family categories Pad/Airy/Vox/Ether/FM/Wind/Metal/Morph,
// tools/bake_loops_header.cpp) + 10 ONESHOT entries (category "Hit", the
// delivered HIT one-shots, tools/bake_shots_header.cpp). All data 12-bit
// left-justified (low nibble zero), read-only. Entry order is LOCKED
// (wave-choice param index = table index): cycles 0-77 (v2 order), loops
// 78-207 (manifest.json order), shots 208-217. Total = 218 waveforms.
//
// Family loop blocks (in order): Pad 28, Airy 18, Vox 16, Ether 16, FM 14,
// Wind 12, Metal 12, Morph 14 (= 130). Categories come from each entry
// (kLoops[i].category / kShots[i].category), not hardcoded here.
//
// v2 files (RomplerBank.h / PcmOscillator.h) stay untouched -- this header
// layers the v3 index on top of them.

#pragma once
#include <array>
#include <cstdint>
#include "RomplerBank.h"
#include "LoopBankData.h"
#include "ShotBankData.h"
#include "LoopRoots.h"      // per-loop measured rootHz (DSP_BUILD s1b), manifest order

namespace rompler::bank3 {

enum class WaveType : uint8_t { Cycle, Loop, OneShot };

struct Waveform {
    const char*    category;
    const char*    name;
    WaveType       type;
    const int16_t* samples;   // 12-bit left-justified (low nibble zero)
    uint32_t       length;    // samples. Cycle: always 600
    uint32_t       loopStart; // Loop: loop point (baked loops: 0, full-buffer)
    float          rootHz;    // Loop: per-loop MEASURED root (LoopRoots.h, s1b;
                              // inharmonic == 220). OneShot: 220 nominal. Cycle: unused
};

inline constexpr int kNumCycles    = rompler::bank::kNumWaveforms;   // 78
inline constexpr int kNumLoops     = 130;  // full v3 library (indices 78-207)
inline constexpr int kNumShots     = 10;   // HIT one-shots (indices 208-217)
inline constexpr int kNumWaveforms = kNumCycles + kNumLoops + kNumShots;  // 218

namespace detail {
constexpr std::array<Waveform, kNumWaveforms> makeTable() {
    std::array<Waveform, kNumWaveforms> t {};
    for (int i = 0; i < kNumCycles; ++i)
        t[(size_t)i] = { rompler::bank::kWaveforms[i].category,
                         rompler::bank::kWaveforms[i].name,
                         WaveType::Cycle,
                         rompler::bank::kWaveforms[i].samples,
                         600u, 0u, 0.0f };
    for (int i = 0; i < kNumLoops; ++i)
        t[(size_t)(kNumCycles + i)] = { rompler::bank::loopdata::kLoops[i].category,
                         rompler::bank::loopdata::kLoops[i].name,
                         WaveType::Loop,
                         rompler::bank::loopdata::kLoops[i].samples,
                         rompler::bank::loopdata::kLoops[i].length,
                         0u, kLoopRoots[i] };   // DSP_BUILD s1b: per-loop measured
                                                // root (manifest order); inharmonic
                                                // loops (METAL/inharmonic MORPH) == 220
    for (int i = 0; i < kNumShots; ++i)
        t[(size_t)(kNumCycles + kNumLoops + i)] = { rompler::bank::shotdata::kShots[i].category,
                         rompler::bank::shotdata::kShots[i].name,
                         WaveType::OneShot,
                         rompler::bank::shotdata::kShots[i].samples,
                         rompler::bank::shotdata::kShots[i].length,
                         0u, 220.0f };
    return t;
}
} // namespace detail

inline constexpr auto kWaveforms = detail::makeTable();

} // namespace rompler::bank3
