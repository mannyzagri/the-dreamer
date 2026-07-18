// Bank3 -- The Dreamer bank v3 (DSP_BUILD.md section 1): three entry types.
// 78 CYCLE entries (v2 bank, unchanged) + 16 LOOP entries (category "Ens",
// bake_loops.py material) + 10 ONESHOT entries (category "Shot",
// tools/bake_shots.cpp). All data 12-bit left-justified (low nibble zero),
// read-only. Entry order is LOCKED (wave-choice param index = table index):
// cycles 0-77 (v2 order), loops 78-93 (recipe order), shots 94-103.
//
// v2 files (RomplerBank.h / PcmOscillator.h) stay untouched -- this header
// layers the v3 index on top of them.

#pragma once
#include <array>
#include <cstdint>
#include "RomplerBank.h"
#include "LoopBankData.h"
#include "ShotBankData.h"

namespace rompler::bank3 {

enum class WaveType : uint8_t { Cycle, Loop, OneShot };

struct Waveform {
    const char*    category;
    const char*    name;
    WaveType       type;
    const int16_t* samples;   // 12-bit left-justified (low nibble zero)
    uint32_t       length;    // samples. Cycle: always 600
    uint32_t       loopStart; // Loop: loop point (baked loops: 0, full-buffer)
    float          rootHz;    // Loop/OneShot: baked pitch (220.0). Cycle: unused
};

inline constexpr int kNumCycles    = rompler::bank::kNumWaveforms;   // 78
inline constexpr int kNumLoops     = 26;   // v1 16 (indices 78-93) + v2 batch2 10 (94-103)
inline constexpr int kNumShots     = 10;
inline constexpr int kNumWaveforms = kNumCycles + kNumLoops + kNumShots;

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
        t[(size_t)(kNumCycles + i)] = { "Ens",
                         rompler::bank::loopdata::kLoops[i].name,
                         WaveType::Loop,
                         rompler::bank::loopdata::kLoops[i].samples,
                         rompler::bank::loopdata::kLoops[i].length,
                         0u, 220.0f };
    for (int i = 0; i < kNumShots; ++i)
        t[(size_t)(kNumCycles + kNumLoops + i)] = { "Shot",
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
