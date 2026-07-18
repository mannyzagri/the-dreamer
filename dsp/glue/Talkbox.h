// Talkbox -- FX-bus vowel/formant filter (FEATURES.md 11.3), distinct from
// the FORMANT filter TYPE. Three parallel bandpass resonators track a vowel's
// first three formants; VOWEL-A and VOWEL-B pick two vowels from A/E/I/O/U
// and MORPH crossfades their formant frequencies. SENS drives the morph from
// an input envelope follower (auto-wah-like "talking"): at SENS 0 morph is
// static, at SENS 1 the envelope sweeps A->B.
//
// Params (dedicated, per DSP_BUILD s9): talk_va (0..1 -> vowel A index),
// talk_vb (0..1 -> vowel B index), talk_morph (0..1 static A<->B),
// talk_sens (0..1 envelope depth). Coeffs at control rate. New glue.
// C++17, JUCE-free, RT-safe after prepare().

#pragma once
#include <cmath>
#include <algorithm>

namespace dreamer {

class Talkbox {
public:
    void prepare(double sampleRate) noexcept { fs_ = sampleRate; reset(); }
    void reset() noexcept {
        for (int i = 0; i < 3; ++i) { bpL_[i] = {}; bpR_[i] = {}; }
        env_ = 0.0f; ctrl_ = 0;
    }

    // 5 vowels x 3 formant frequencies (Hz): A E I O U
    static void formants(int vowel, float f[3]) noexcept {
        static constexpr float T[5][3] = {
            { 730.0f, 1090.0f, 2440.0f },   // A
            { 530.0f, 1840.0f, 2480.0f },   // E
            { 270.0f, 2290.0f, 3010.0f },   // I
            { 570.0f,  840.0f, 2410.0f },   // O
            { 300.0f,  870.0f, 2240.0f },   // U
        };
        vowel = std::min(4, std::max(0, vowel));
        f[0] = T[vowel][0]; f[1] = T[vowel][1]; f[2] = T[vowel][2];
    }

    void process(float& l, float& r, float va01, float vb01,
                 float morph01, float sens01, float mix) noexcept {
        // envelope follower on the mono sum drives extra morph
        const float mono = 0.5f * (l + r);
        const float rect = std::fabs(mono);
        const float ea = rect > env_ ? 0.02f : 0.0008f;   // fast attack, slow release
        env_ += ea * (rect - env_);
        float morph = std::clamp(morph01 + sens01 * env_ * 2.0f, 0.0f, 1.0f);

        if (ctrl_-- <= 0) {                               // control-rate coeffs
            ctrl_ = 15;
            const int va = (int)(std::clamp(va01, 0.0f, 1.0f) * 4.999f);
            const int vb = (int)(std::clamp(vb01, 0.0f, 1.0f) * 4.999f);
            float fa[3], fb[3];
            formants(va, fa); formants(vb, fb);
            for (int i = 0; i < 3; ++i) {
                const float fc = fa[i] + (fb[i] - fa[i]) * morph;
                setBandpass(bpL_[i], fc);
                bpR_[i].b0 = bpL_[i].b0; bpR_[i].a1 = bpL_[i].a1; bpR_[i].a2 = bpL_[i].a2;
            }
        }

        float wetL = 0.0f, wetR = 0.0f;
        static constexpr float g[3] = { 1.0f, 0.7f, 0.45f };   // formant weights
        for (int i = 0; i < 3; ++i) {
            wetL += bpL_[i].tick(l) * g[i];
            wetR += bpR_[i].tick(r) * g[i];
        }
        const float m = std::clamp(mix, 0.0f, 1.0f);
        l = l * (1.0f - m) + wetL * m;
        r = r * (1.0f - m) + wetR * m;
    }

private:
    struct Biquad {
        // constant-skirt bandpass: b0 = alpha, b1 = 0, b2 = -alpha (a0-normalized)
        float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
        float tick(float x) noexcept {
            const float out = b0 * (x - x2_) - a1 * y1_ - a2 * y2_;
            x2_ = x1_; x1_ = x;
            y2_ = y1_; y1_ = out;
            return out;
        }
    };
    void setBandpass(Biquad& bq, float fc) noexcept {
        const float w0 = 2.0f * 3.14159265f * std::min(fc, (float)fs_ * 0.45f) / (float)fs_;
        const float Q = 8.0f;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha;
        bq.b0 = alpha / a0;                 // constant skirt gain BPF (b0=alpha, b2=-alpha)
        bq.a1 = (-2.0f * std::cos(w0)) / a0;
        bq.a2 = (1.0f - alpha) / a0;
    }

    Biquad bpL_[3], bpR_[3];
    double fs_ = 44100.0;
    float  env_ = 0.0f;
    int    ctrl_ = 0;
};

} // namespace dreamer
