// Rotary -- Leslie rotary-speaker sim (FEATURES.md 11.2). Splits the input
// at a ~800 Hz crossover into a fast HORN rotor and a slow DRUM rotor; each
// rotor gives Doppler (a modulated delay) + amplitude tremolo, picked up by
// two stereo "mics" 180 deg apart. SPEED toggles chorale(slow)/tremolo(fast)
// with an ACCEL-controlled spin ramp; BALANCE mixes horn vs drum; MIC WIDTH
// sets the stereo spread.
//
// PARAMS extras (p0..p3): p0 = SPEED (<=.5 slow / >.5 fast), p1 = ACCEL,
// p2 = BALANCE (horn<->drum), p3 = MIC WIDTH. New glue. C++17, JUCE-free,
// RT-safe after prepare(). Rotor angles run as magic-circle quadrature
// oscillators (2 mults/add, no per-sample transcendental -- F2); the
// oscillator rate (eps) is refreshed at control rate as the spin glides.

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer {

class Rotary {
public:
    void prepare(double sampleRate) noexcept {
        fs_ = sampleRate;
        len_ = (int)(fs_ * 0.006) + 4;                 // 6 ms Doppler line
        hL_.assign((size_t)len_, 0.0f); hR_.assign((size_t)len_, 0.0f);
        dL_.assign((size_t)len_, 0.0f); dR_.assign((size_t)len_, 0.0f);
        w_ = 0;
        hs_ = ds_ = 0.0f; hc_ = dc_ = 1.0f;
        hornRate_ = 6.8f; drumRate_ = 0.8f;
        hEps_ = epsFor(hornRate_); dEps_ = epsFor(drumRate_);
        xLP_ = 0.0f; ctrl_ = 0;
    }
    void reset() noexcept {
        std::fill(hL_.begin(), hL_.end(), 0.0f); std::fill(hR_.begin(), hR_.end(), 0.0f);
        std::fill(dL_.begin(), dL_.end(), 0.0f); std::fill(dR_.begin(), dR_.end(), 0.0f);
        hs_ = ds_ = 0.0f; hc_ = dc_ = 1.0f; xLP_ = 0.0f; ctrl_ = 0;
    }

    void process(float& l, float& r, float p0_speed, float p1_accel,
                 float p2_balance, float p3_micWidth, float mix) noexcept {
        const bool fast = p0_speed > 0.5f;
        const float hornTgt = fast ? 6.8f : 0.9f;       // Hz (tremolo vs chorale)
        const float drumTgt = fast ? 0.8f : 0.15f;
        const float glide = (0.2f + 4.8f * p1_accel) / (float)fs_;
        hornRate_ += glide * (hornTgt - hornRate_);
        drumRate_ += glide * (drumTgt - drumRate_);

        if (ctrl_-- <= 0) {                             // refresh oscillator rates
            ctrl_ = 15;
            hEps_ = epsFor(hornRate_);
            dEps_ = epsFor(drumRate_);
        }

        const float mono = 0.5f * (l + r);
        // crossover ~800 Hz: xLP_ = lows (drum), hi = highs (horn)
        const float a = 1.0f - std::exp(-2.0f * 3.14159265f * 800.0f / (float)fs_);
        xLP_ += a * (mono - xLP_);
        const float lows = xLP_, highs = mono - xLP_;

        hL_[(size_t)w_] = highs; hR_[(size_t)w_] = highs;
        dL_[(size_t)w_] = lows;  dR_[(size_t)w_] = lows;

        hs_ += hEps_ * hc_; hc_ -= hEps_ * hs_;         // horn quadrature
        ds_ += dEps_ * dc_; dc_ -= dEps_ * ds_;         // drum quadrature

        float hornL = read(hL_, 2.5f + 1.6f * hs_) * (0.7f + 0.3f * hc_);
        float hornR = read(hR_, 2.5f - 1.6f * hs_) * (0.7f - 0.3f * hc_);   // mic 180
        float drumL = read(dL_, 3.0f + 1.2f * ds_) * (0.8f + 0.2f * dc_);
        float drumR = read(dR_, 3.0f - 1.2f * ds_) * (0.8f - 0.2f * dc_);

        const float bal = std::clamp(p2_balance, 0.0f, 1.0f);   // 0 horn..1 drum
        const float wH = 1.0f - bal, wD = bal;
        const float width = std::clamp(p3_micWidth, 0.0f, 1.0f);
        const float wetLraw = wH * hornL + wD * drumL;
        const float wetRraw = wH * hornR + wD * drumR;
        const float wetMid = 0.5f * (wetLraw + wetRraw);
        float wetL = wetMid + width * (wetLraw - wetMid);       // MIC WIDTH spread
        float wetR = wetMid + width * (wetRraw - wetMid);

        const float m = std::clamp(mix, 0.0f, 1.0f);
        l = l * (1.0f - m) + wetL * m;
        r = r * (1.0f - m) + wetR * m;

        if (++w_ >= len_) w_ = 0;
    }

private:
    float epsFor(float hz) const noexcept {
        return 2.0f * std::sin(3.14159265f * std::max(0.01f, hz) / (float)fs_);
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

    std::vector<float> hL_, hR_, dL_, dR_;
    double fs_ = 44100.0;
    float  hs_ = 0.0f, ds_ = 0.0f, hc_ = 1.0f, dc_ = 1.0f;
    float  hornRate_ = 6.8f, drumRate_ = 0.8f, hEps_ = 0.0f, dEps_ = 0.0f, xLP_ = 0.0f;
    int    len_ = 1, w_ = 0, ctrl_ = 0;
};

} // namespace dreamer
