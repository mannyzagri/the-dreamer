// Dimension -- Roland Dimension-D-style stereo BBD chorus (FEATURES.md 11.2).
// Fixed 4-mode chorus: NO rate/depth knobs; MODE (1..4) selects preset
// chorus depth/mix, TONE rolls off BBD HF. A single slow (~0.5 Hz) sine
// modulates the L and R bucket-brigade delay lines in anti-phase, giving the
// characteristic wide shimmer without the obvious wobble of a rate knob.
//
// PARAMS extras (p0..p3): p0 = MODE (0..1 -> 4 steps), p1 = TONE (HF loss),
// p2/p3 unused. New glue (Dim-D is not in the Rhino set). C++17, JUCE-free,
// RT-safe after prepare().

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer {

class Dimension {
public:
    void prepare(double sampleRate) noexcept {
        fs_ = sampleRate;
        len_ = (int)(fs_ * 0.020) + 4;                 // 20 ms line
        bufL_.assign((size_t)len_, 0.0f);
        bufR_.assign((size_t)len_, 0.0f);
        // magic-circle quadrature LFO at a fixed 0.5 Hz -- per-sample sine
        // with 2 mults/add, no transcendental in the hot path (F2).
        eps_ = 2.0f * std::sin(3.14159265f * 0.5f / (float)fs_);
        w_ = 0; s_ = 0.0f; c_ = 1.0f; toneL_ = toneR_ = 0.0f; lastMode_ = -1; fade_ = 1.0f;
    }
    void reset() noexcept {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        s_ = 0.0f; c_ = 1.0f; toneL_ = toneR_ = 0.0f; lastMode_ = -1; fade_ = 1.0f;
    }

    // p0 = mode select, p1 = tone; mix parameter is the slot MIX knob.
    void process(float& l, float& r, float p0_mode, float p1_tone, float mix) noexcept {
        const int mode = std::min(3, std::max(0, (int)(p0_mode * 3.999f)));   // 0..3
        // per-mode base delay + modulation depth (Dim-D 1..4 character)
        static constexpr float baseMs[4]  = { 4.0f, 5.0f, 6.5f, 8.0f };
        static constexpr float rangeMs[4] = { 0.6f, 1.1f, 1.8f, 2.6f };
        static constexpr float depth[4]   = { 0.35f, 0.5f, 0.68f, 0.85f };

        // MODE jump changes base delay -> de-click by fading the wet in (F7)
        if (mode != lastMode_) { lastMode_ = mode; fade_ = 0.0f; }
        fade_ = std::min(1.0f, fade_ + 1.0f / (0.006f * (float)fs_));

        bufL_[(size_t)w_] = l;
        bufR_[(size_t)w_] = r;

        s_ += eps_ * c_; c_ -= eps_ * s_;               // quadrature step
        const float lfo = s_;

        const float msL = baseMs[mode] + rangeMs[mode] * lfo;
        const float msR = baseMs[mode] - rangeMs[mode] * lfo;    // anti-phase
        float wetL = read(bufL_, msL);
        float wetR = read(bufR_, msR);

        // TONE: shared BBD HF loss one-pole (0 = bright .. 1 = dark)
        const float a = 0.05f + 0.9f * p1_tone;
        toneL_ += a * (wetL - toneL_); wetL = toneL_;
        toneR_ += a * (wetR - toneR_); wetR = toneR_;

        const float m = std::clamp(mix, 0.0f, 1.0f) * depth[mode] * fade_;
        l = l * (1.0f - m) + wetL * m;
        r = r * (1.0f - m) + wetR * m;

        if (++w_ >= len_) w_ = 0;
    }

private:
    float read(const std::vector<float>& buf, float ms) const noexcept {
        float d = ms * (float)fs_ * 0.001f;
        if (d < 1.0f) d = 1.0f;
        if (d > (float)(len_ - 2)) d = (float)(len_ - 2);
        float pos = (float)w_ - d;
        if (pos < 0.0f) pos += (float)len_;
        if (pos >= (float)len_) pos = 0.0f;   // TD-001: float wrap can land exactly on len_ (see Ensemble.h)
        const int   i0 = (int)pos;
        const int   i1 = i0 + 1 >= len_ ? 0 : i0 + 1;
        const float fr = pos - (float)i0;
        return buf[(size_t)i0] + (buf[(size_t)i1] - buf[(size_t)i0]) * fr;
    }

    std::vector<float> bufL_, bufR_;
    double fs_ = 44100.0;
    float  eps_ = 0.0f, s_ = 0.0f, c_ = 1.0f, toneL_ = 0.0f, toneR_ = 0.0f, fade_ = 1.0f;
    int    len_ = 1, w_ = 0, lastMode_ = -1;
};

} // namespace dreamer
