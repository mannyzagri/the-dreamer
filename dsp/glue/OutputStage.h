// OutputStage.h -- final output soft-clip + brickwall ceiling (GAIN_STAGING s5).
//
// Sits AFTER the master gain, just before the buffer leaves the processor. Two
// stages, in order:
//
//   1. tanh soft-clip (character): out = tanh(drive*x) / tanh(drive). A gentle
//      drive keeps small signals near-transparent and rounds transients the way
//      a real output stage does -- on-theme with the 12-bit voice. By
//      construction x == 1 maps to exactly 1.0 (0 dBFS in -> 0 dBFS out); above
//      unity it saturates smoothly. Small-signal gain is drive/tanh(drive)
//      (a hair above unity for a mild drive) -- negligible, documented.
//   2. hard ceiling at -0.1 dBFS (~0.98855): a plain clamp so NOTHING can ever
//      leave the plugin digitally clipped, catching any residual overshoot from
//      the soft-clip's >unity asymptote or upstream peaks.
//
// Stateless and branch-light; the only precompute is 1/tanh(drive), done once
// in prepare() (tanh is not constexpr in C++17). RT-safe: no alloc/lock/log.
//
// C++17, header-only, JUCE-free.

#pragma once
#include <cmath>

namespace dreamer {

class OutputStage {
public:
    // drive: soft-clip curvature. 0.5 is deliberately gentle -- the curve stays
    // within ~8% of linear across [-1, 1], so normal-level material is barely
    // touched while hot peaks get rounded before the ceiling.
    void prepare(float drive = 0.5f) noexcept {
        drive_       = drive > 1.0e-3f ? drive : 1.0e-3f;
        invTanhDrive_ = 1.0f / std::tanh(drive_);
    }

    // -0.1 dBFS true-peak ceiling: 10^(-0.1/20) = 0.988553.
    static constexpr float kCeiling = 0.98855309f;

    static float clampCeiling(float x) noexcept {
        if (x >  kCeiling) return  kCeiling;
        if (x < -kCeiling) return -kCeiling;
        return x;
    }

    float processSample(float x) noexcept {
        const float soft = std::tanh(drive_ * x) * invTanhDrive_;
        return clampCeiling(soft);
    }

    void process(float& l, float& r) noexcept {
        l = processSample(l);
        r = processSample(r);
    }

private:
    float drive_        = 0.5f;
    float invTanhDrive_ = 1.0f / 0.46211716f;   // 1/tanh(0.5), overwritten by prepare()
};

} // namespace dreamer
