// StereoWidth -- width / image stage (FEATURES.md 11.3): mid/side gain on
// the side channel, optional Haas delay on one channel for extra spread, and
// a BASS-MONO switch that keeps low frequencies centered (a mastering-safe
// staple). This is the stage that can explicitly promote the mono voice bus
// to stereo before delay/reverb.
//
// Params (dedicated, per DSP_BUILD s9): width (0..1 -> side gain 0..2),
// haas (0..1 -> 0..12 ms on R), bassmono (bool, split ~200 Hz). New glue.
// C++17, JUCE-free, RT-safe after prepare().

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer {

class StereoWidth {
public:
    void prepare(double sampleRate) noexcept {
        fs_ = sampleRate;
        len_ = (int)(fs_ * 0.013) + 4;                 // up to 13 ms Haas
        haasBuf_.assign((size_t)len_, 0.0f);
        w_ = 0; lowL_ = lowR_ = 0.0f;
    }
    void reset() noexcept {
        std::fill(haasBuf_.begin(), haasBuf_.end(), 0.0f);
        w_ = 0; lowL_ = lowR_ = lowL2_ = lowR2_ = 0.0f;
    }

    void process(float& l, float& r, float width01, float haas01, bool bassMono) noexcept {
        // Haas: FIXED 8 ms delay tap on R; HAAS crossfades dry R -> delayed R.
        // Length is fixed (never modulated) so automating HAAS can't produce a
        // comb-sweep/pitch artifact (F8) -- only the mix moves.
        haasBuf_[(size_t)w_] = r;
        if (haas01 > 0.0f) {
            const int   d  = std::min(len_ - 2, (int)(0.008f * (float)fs_));
            int         i0 = w_ - d;
            if (i0 < 0) i0 += len_;
            const float amt = std::clamp(haas01, 0.0f, 1.0f);
            r = r * (1.0f - amt) + haasBuf_[(size_t)i0] * amt;
        }
        if (++w_ >= len_) w_ = 0;

        // mid/side width
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r);
        side *= std::clamp(width01, 0.0f, 1.0f) * 2.0f;   // 0 = mono, 2 = extra wide

        float outL = mid + side;
        float outR = mid - side;

        // BASS-MONO: below ~250 Hz collapse to mono (keeps lows centered).
        // 2-pole (cascaded one-pole) split so the low band is captured steeply
        // enough that pure-side bass genuinely mono's, not a gentle 1-pole leak.
        if (bassMono) {
            const float a = 1.0f - std::exp(-2.0f * 3.14159265f * 250.0f / (float)fs_);
            lowL_  += a * (outL - lowL_);   lowL2_ += a * (lowL_ - lowL2_);
            lowR_  += a * (outR - lowR_);   lowR2_ += a * (lowR_ - lowR2_);
            const float lowMono = 0.5f * (lowL2_ + lowR2_);
            outL = (outL - lowL2_) + lowMono;             // highs stay, lows -> mono
            outR = (outR - lowR2_) + lowMono;
        }

        l = outL;
        r = outR;
    }

private:
    std::vector<float> haasBuf_;
    double fs_ = 44100.0;
    float  lowL_ = 0.0f, lowR_ = 0.0f, lowL2_ = 0.0f, lowR2_ = 0.0f;
    int    len_ = 1, w_ = 0;
};

} // namespace dreamer
