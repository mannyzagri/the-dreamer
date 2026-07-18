// Ensemble -- the MOD FX "ENSEMBLE" mode (DSP_BUILD.md section 7).
// The Rhino chorus (ModDelayFx) is a single modulated tap, so per spec the
// ensemble is implemented as a 3-tap modulated delay with spread LFO phases
// (0 / 1/3 / 2/3 of a cycle), the classic string-machine BBD trick,
// 12-bit-era friendly: one delay line per channel, tri-shaped LFOs, no
// oversampling, equal-power dry/wet like the donor ModDelayFx.
//
// New glue DSP (not a port -- the donor has no ensemble). C++17, JUCE-free,
// real-time safe after prepare().

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer {

class Ensemble {
public:
    void prepare(double sampleRate) noexcept {
        fs_ = sampleRate;
        len_ = (int)(fs_ * 0.030) + 4;                 // 30 ms line
        bufL_.assign((size_t)len_, 0.0f);
        bufR_.assign((size_t)len_, 0.0f);
        w_ = 0;
        phase_ = 0.0f;
    }
    void reset() noexcept {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        phase_ = 0.0f; toneL_ = toneR_ = 0.0f;
    }

    // PARAMS extras: p0 = SPREAD (stereo tap offset), p1 = TONE (BBD HF loss).
    // p2/spare unused. Overload keeps the old 5-arg call working (defaults).
    void process(float& l, float& r, float rateHz, float depth01, float mix,
                 float spread01 = 0.5f, float tone01 = 0.0f) noexcept {
        bufL_[(size_t)w_] = l;
        bufR_[(size_t)w_] = r;

        phase_ += rateHz / (float)fs_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        const float baseMs  = 8.0f;
        const float rangeMs = 1.0f + depth01 * 6.0f;   // 1..7 ms swing

        const float spreadOff = 0.05f + 0.45f * std::clamp(spread01, 0.0f, 1.0f);   // R tap phase offset
        float wetL = 0.0f, wetR = 0.0f;
        for (int t = 0; t < 3; ++t) {
            const float phL = phase_ + (float)t * (1.0f / 3.0f);
            const float phR = phL + spreadOff;         // SPREAD: R taps offset
            const float ms  = baseMs + rangeMs * tri(phL);
            const float msR = baseMs + rangeMs * tri(phR);
            wetL += read(bufL_, ms);
            wetR += read(bufR_, msR);
        }
        wetL *= (1.0f / 3.0f);
        wetR *= (1.0f / 3.0f);

        // TONE: BBD HF loss one-pole (0 = bright .. 1 = dark)
        if (tone01 > 0.0f) {
            const float a = 1.0f - 0.9f * tone01;
            toneL_ += a * (wetL - toneL_); wetL = toneL_;
            toneR_ += a * (wetR - toneR_); wetR = toneR_;
        }

        const float m = std::clamp(mix, 0.0f, 1.0f);
        const float dry = std::cos(m * 1.57079633f);   // equal-power (donor idiom)
        const float wet = std::sin(m * 1.57079633f);
        l = l * dry + wetL * wet;
        r = r * dry + wetR * wet;

        if (++w_ >= len_) w_ = 0;
    }

private:
    static float tri(float ph) noexcept {              // 0..1 -> 0..1 triangle
        ph -= std::floor(ph);
        return 1.0f - std::fabs(2.0f * ph - 1.0f);
    }
    float read(const std::vector<float>& buf, float ms) const noexcept {
        float d = ms * (float)fs_ * 0.001f;
        if (d < 1.0f) d = 1.0f;
        if (d > (float)(len_ - 2)) d = (float)(len_ - 2);
        float pos = (float)w_ - d;
        if (pos < 0.0f) pos += (float)len_;
        const int   i0 = (int)pos;
        const int   i1 = i0 + 1 >= len_ ? 0 : i0 + 1;
        const float fr = pos - (float)i0;
        return buf[(size_t)i0] + (buf[(size_t)i1] - buf[(size_t)i0]) * fr;
    }

    std::vector<float> bufL_, bufR_;
    double fs_ = 44100.0;
    float  phase_ = 0.0f, toneL_ = 0.0f, toneR_ = 0.0f;
    int    len_ = 1, w_ = 0;
};

} // namespace dreamer
