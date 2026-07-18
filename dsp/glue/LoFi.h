// LoFi -- lo-fi / degrade stage (FEATURES.md 11.3): variable bit depth (12
// down to ~6), sample-rate reduction (sample-and-hold decimation), optional
// mu-law companding, and an ALIAS switch (off = a gentle pre-decimation
// one-pole to tame aliasing; on = raw, authentic ugly). Output re-quantized
// to the 12-bit grain. "The most on-theme effect in the whole plan."
//
// Params (dedicated, per DSP_BUILD s9): bits, srate, compand (0..1),
// alias (bool). Operates per-channel on the stereo pair. New glue.
// C++17, JUCE-free, RT-safe after prepare().

#pragma once
#include <cmath>
#include <algorithm>

namespace dreamer {

class LoFi {
public:
    void prepare(double sampleRate) noexcept { fs_ = sampleRate; reset(); }
    void reset() noexcept {
        holdL_ = holdR_ = 0.0f; outL_ = outR_ = 0.0f; aaL_ = aaR_ = 0.0f; phase_ = 0.0f;
    }

    // 12-bit grain requant (shared with the tone chain's contract)
    static float requant12(float x) noexcept {
        int32_t q = (int32_t)(x * 32768.0f);
        if (q > 32767) q = 32767; else if (q < -32768) q = -32768;
        q = (q / 16) * 16;
        return (float)q * (1.0f / 32768.0f);
    }

    void process(float& l, float& r, float bits01, float srate01,
                 float compand01, bool alias) noexcept {
        // BITS: 12 (bits01=0) down to 6 (bits01=1)
        const float bits   = 12.0f - 6.0f * std::clamp(bits01, 0.0f, 1.0f);
        const float levels = std::pow(2.0f, bits);
        const float qstep  = 2.0f / levels;

        // SRATE: decimate factor 1x (srate01=0) .. ~32x (srate01=1)
        const float factor = 1.0f + 31.0f * std::clamp(srate01, 0.0f, 1.0f);

        // anti-alias one-pole before the sample-and-hold, unless ALIAS on
        float xl = l, xr = r;
        if (!alias) {
            const float cut = (fs_ / (2.0f * factor)) * 0.9f;
            const float a = 1.0f - std::exp(-2.0f * 3.14159265f * std::min(cut, (float)fs_ * 0.49f) / (float)fs_);
            aaL_ += a * (xl - aaL_); xl = aaL_;
            aaR_ += a * (xr - aaR_); xr = aaR_;
        }

        // sample-and-hold decimation: quantize/compand ONLY when a new sample
        // is latched (per-hold, not per-sample -- keeps the log/exp off the
        // hot path, F2), then hold the quantized output between latches.
        phase_ += 1.0f / factor;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
            holdL_ = xl; holdR_ = xr;
            float ql, qr;
            if (compand01 > 0.0f) {
                const float mu = 1.0f + 255.0f * compand01;
                ql = compandRound(holdL_, mu, levels);
                qr = compandRound(holdR_, mu, levels);
            } else {
                ql = std::round(holdL_ / qstep) * qstep;   // linear quantize
                qr = std::round(holdR_ / qstep) * qstep;
            }
            outL_ = requant12(std::clamp(ql, -1.0f, 1.0f));  // land on 12-bit grid
            outR_ = requant12(std::clamp(qr, -1.0f, 1.0f));
        }

        l = outL_;
        r = outR_;
    }

private:
    static float compandRound(float x, float mu, float levels) noexcept {
        const float s = x < 0 ? -1.0f : 1.0f;
        const float ax = std::min(std::fabs(x), 1.0f);
        float y = s * std::log(1.0f + mu * ax) / std::log(1.0f + mu);   // encode
        y = std::round(y * levels) / levels;                            // quantize
        const float ay = std::fabs(y);
        const float sd = y < 0 ? -1.0f : 1.0f;
        return sd * (std::pow(1.0f + mu, ay) - 1.0f) / mu;              // decode
    }

    double fs_ = 44100.0;
    float  holdL_ = 0.0f, holdR_ = 0.0f, outL_ = 0.0f, outR_ = 0.0f;
    float  aaL_ = 0.0f, aaR_ = 0.0f, phase_ = 0.0f;
};

} // namespace dreamer
