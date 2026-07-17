// PcmOscillator -- era-authentic single-cycle PCM playback (E-MU / Roland JD style)
//
// Authenticity notes (deliberate, do not "fix"):
//   * No band-limiting, no mip levels: transposing up ALIASES, exactly like
//     the 90s playback ASICs. This is the sound.
//   * Two interpolation modes:
//       DropSample -- nearest-neighbor fetch, the cheapest ASIC behavior.
//                     Adds zipper/ROM grain, strongest on low notes.
//       Linear     -- 2-point linear interp, what better ASICs (E-MU G-chip
//                     class, Roland PCM) actually did. Default.
//     Nothing higher-order is offered on purpose.
//   * Bank data is 12-bit requantized (left-justified in int16); the low
//     nibble is zero, so the ROM quantization grain survives all math.
//   * Fixed-point 32-bit phase accumulator (16.16 over the 600-sample cycle
//     mapped through an integer table index), like the hardware counters.
//
// C++17, header-only, no dependencies. Feed it rompler::bank data.

#pragma once
#include <cstdint>
#include "RomplerBank.h"

namespace rompler {

class PcmOscillator {
public:
    enum class Interp { DropSample, Linear };

    void setSampleRate(double sr) noexcept { sampleRate_ = sr; updateIncrement(); }
    void setFrequency(double hz)  noexcept { freq_ = hz;      updateIncrement(); }
    void setInterp(Interp m)      noexcept { interp_ = m; }

    void setWaveform(const int16_t* cycle) noexcept { cycle_ = cycle; }
    void setWaveform(int bankIndex) noexcept {
        if (bankIndex >= 0 && bankIndex < bank::kNumWaveforms)
            cycle_ = bank::kWaveforms[bankIndex].samples;
    }

    // Sample-start offset in samples [0, 600) -- the E-MU trick.
    void reset(double startOffsetSamples = 0.0) noexcept {
        phase_ = static_cast<uint32_t>(startOffsetSamples * 65536.0) % kPhaseWrap;
    }

    float process() noexcept {
        const uint32_t idx  = phase_ >> 16;            // integer sample index
        float out;
        if (interp_ == Interp::DropSample) {
            out = static_cast<float>(cycle_[idx]) * kNorm;
        } else {
            const uint32_t frac = phase_ & 0xFFFFu;    // 16-bit fraction
            const uint32_t nxt  = (idx + 1 == kLen) ? 0 : idx + 1;
            const int32_t a = cycle_[idx];
            const int32_t b = cycle_[nxt];
            // Integer lerp, as the ASICs did it: a + ((b-a)*frac)>>16
            const int32_t v = a + static_cast<int32_t>(
                (static_cast<int64_t>(b - a) * frac) >> 16);
            out = static_cast<float>(v) * kNorm;
        }
        phase_ += increment_;
        if (phase_ >= kPhaseWrap) phase_ -= kPhaseWrap;
        return out;
    }

private:
    static constexpr uint32_t kLen       = static_cast<uint32_t>(bank::kCycleLength); // 600
    static constexpr uint32_t kPhaseWrap = kLen << 16;   // 600 in 16.16
    static constexpr float    kNorm      = 1.0f / 32768.0f;

    void updateIncrement() noexcept {
        // phase step (16.16) = f0 * cycleLen / fs
        const double step = freq_ * static_cast<double>(kLen) / sampleRate_;
        increment_ = static_cast<uint32_t>(step * 65536.0 + 0.5);
    }

    const int16_t* cycle_    = bank::kWaveforms[0].samples;
    double   sampleRate_     = 44100.0;
    double   freq_           = 440.0;
    uint32_t phase_          = 0;
    uint32_t increment_      = 0;
    Interp   interp_         = Interp::Linear;
};

} // namespace rompler
