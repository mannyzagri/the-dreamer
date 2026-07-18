// FilterExtra -- the V1.1 global-filter TYPES (GUI indices 7..12), built new
// for this project (NOT ports; rule 1 does not apply -- these are original
// glue). One class covers all six; GlobalFilter routes types 7..12 here.
//
//   7 NOTCH    RBJ band-reject; RES -> notch depth/Q, CUT -> centre.
//   8 COMB +   feedback comb (positive), CUT -> comb freq (delay = fs/fc),
//              RES -> feedback. Reinforces harmonics of the comb freq.
//   9 COMB -   feedback comb (negative feedback) -- the hollow/phasey twin.
//  10 N+LP     notch at CUT cascaded into a 12 dB lowpass just above it: a
//              lowpass with a formant-style dip at the corner (the panel name).
//  11 FORMANT  three bandpass resonators on a vowel; CUT sweeps A->E->I->O->U
//              (and scales the set), RES -> resonator Q. Vowel table shared
//              with the FX-bus Talkbox.
//  12 ALLPASS  4x cascaded allpass mixed 50/50 with dry -> swept phase notches
//              (static-phaser character; a bare allpass is inaudible on tone).
//
// All cutoff/res driven exactly like the other slots (Hz + 0..1). Fixed
// storage, no allocation, no locks -- RT-safe after setSampleRate(). C++17,
// header-only, JUCE-free.

#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace dreamer {

class FilterExtra {
public:
    // internal ids (GUI index - 7)
    enum Kind : int { notch = 0, combPlus = 1, combMinus = 2, nlp = 3,
                      formant = 4, allpass = 5 };

    void setSampleRate(double sr) noexcept { fs_ = sr > 0.0 ? sr : 44100.0; dirty_ = true; }

    void setKind(int k) noexcept {
        k = std::min((int)allpass, std::max(0, k));
        if (k != kind_) { kind_ = k; reset(); }
        dirty_ = true;
    }

    void setCutoffRes(double hz, double res01) noexcept {
        const double h = std::min(std::max(hz, 20.0), fs_ * 0.45);
        const double r = std::min(1.0, std::max(0.0, res01));
        if (h != hz_ || r != res_ || dirty_) { hz_ = h; res_ = r; dirty_ = false; recalc(); }
    }

    void reset() noexcept {
        for (auto& b : bq_) b.reset();
        for (auto& a : ap_) a.reset();
        for (auto& s : comb_) s = 0.0f;
        combIdx_ = 0;
        combDf_ = combDfTarget_;                 // snap (no glide across type/reset)
    }

    float process(float x) noexcept {
        switch (kind_) {
        case notch:     return bq_[0].tick(x);
        case combPlus:  return comb(x, +1.0f);
        case combMinus: return comb(x, -1.0f);
        case nlp:       return bq_[1].tick(bq_[0].tick(x));            // notch -> LP
        case formant:   return formantTick(x);
        case allpass: {
            float y = x;
            for (int i = 0; i < 4; ++i) y = ap_[i].tick(y);
            return 0.5f * (x + y);                                     // swept phase notches
        }
        default: return x;
        }
    }

private:
    // -------- RBJ biquad, Direct Form I, a0-normalised --------
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void reset() noexcept { x1 = x2 = y1 = y2 = 0; }
        float tick(float x) noexcept {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };
    // -------- first-order allpass (pole/zero at coeff) --------
    struct Allpass1 {
        float c = 0.0f, z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        float tick(float x) noexcept { const float y = -c * x + z; z = x + c * y; return y; }
    };

    void recalc() noexcept {
        const double w0 = 2.0 * 3.14159265358979323846 * hz_ / fs_;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double Q  = 0.5 + res_ * res_ * 24.0;          // 0.5 .. ~24.5
        const double al = sw / (2.0 * Q);
        const double a0 = 1.0 + al;

        // NOTCH (bq_[0]) -- also the N+LP front stage
        bq_[0].b0 = (float)(1.0 / a0);
        bq_[0].b1 = (float)((-2.0 * cw) / a0);
        bq_[0].b2 = (float)(1.0 / a0);
        bq_[0].a1 = (float)((-2.0 * cw) / a0);
        bq_[0].a2 = (float)((1.0 - al) / a0);

        // N+LP back stage (bq_[1]) -- RBJ LP a touch above the notch (12 dB)
        {
            const double wl = 2.0 * 3.14159265358979323846
                            * std::min(hz_ * 1.5, fs_ * 0.45) / fs_;
            const double cwl = std::cos(wl), swl = std::sin(wl);
            const double Ql = 0.707 + res_ * 3.0, all = swl / (2.0 * Ql), a0l = 1.0 + all;
            bq_[1].b1 = (float)((1.0 - cwl) / a0l);
            bq_[1].b0 = (float)(((1.0 - cwl) * 0.5) / a0l);
            bq_[1].b2 = bq_[1].b0;
            bq_[1].a1 = (float)((-2.0 * cwl) / a0l);
            bq_[1].a2 = (float)((1.0 - all) / a0l);
        }

        // COMB: FRACTIONAL delay length from cutoff (comb fundamental = fs/D).
        // Kept fractional + linearly interpolated on read so the comb tuning
        // glides continuously with CUT instead of snapping to fs/integer
        // (the audible stepping). combDf_ is one-pole smoothed toward this
        // target in comb() so a CUT sweep flanges smoothly.
        combDfTarget_ = (float)std::min(std::max(fs_ / std::max(hz_, 20.0), 2.0),
                                        (double)kCombMax - 2.0);
        combG_ = (float)(0.5 + res_ * 0.48);                 // 0.5 .. 0.98 feedback

        // FORMANT: CUT position (log 60..4000) sweeps the vowel set + scale
        {
            const double t = std::clamp((std::log2(std::max(hz_, 60.0)) - std::log2(60.0))
                                        / (std::log2(4000.0) - std::log2(60.0)), 0.0, 1.0);
            const double scale = 0.6 + t * 1.6;              // shift formants up with CUT
            const int    vi = (int)std::min(4.0, t * 4.0);
            const double vf = std::min(4.0, t * 4.0) - vi;
            float fA[3], fB[3];
            vowel(vi, fA); vowel(std::min(4, vi + 1), fB);
            const double Qf = 6.0 + res_ * 18.0;
            for (int i = 0; i < 3; ++i) {
                const double fc = (fA[i] + (fB[i] - fA[i]) * vf) * scale;
                setBandpass(fbq_[i], std::min(fc, fs_ * 0.45), Qf);
            }
        }

        // ALLPASS: 4 stages, coeff from cutoff (all at the same corner)
        {
            const double t = std::tan(3.14159265358979323846 * std::min(hz_, fs_ * 0.45) / fs_);
            const float  c = (float)((t - 1.0) / (t + 1.0));
            for (auto& a : ap_) a.c = c;
        }
    }

    float comb(float x, float sign) noexcept {
        combDf_ += 0.0015f * (combDfTarget_ - combDf_);      // glide delay length
        float rd = (float)combIdx_ - combDf_;                // fractional read pos
        while (rd < 0.0f) rd += (float)kCombMax;
        const int   i0 = (int)rd;
        const float fr = rd - (float)i0;
        const int   i1 = (i0 + 1) % kCombMax;
        const float d  = comb_[i0] + fr * (comb_[i1] - comb_[i0]);   // linear interp
        const float y  = x + sign * combG_ * d;
        comb_[combIdx_] = y;
        combIdx_ = (combIdx_ + 1) % kCombMax;
        return 0.7f * y;                                     // headroom vs the feedback peak
    }

    float formantTick(float x) noexcept {
        static constexpr float g[3] = { 1.0f, 0.7f, 0.45f };
        float y = 0.0f;
        for (int i = 0; i < 3; ++i) y += fbq_[i].tick(x) * g[i];
        return y * kFormantMakeup;   // narrowband -> lift to a usable level
    }

    static void vowel(int v, float f[3]) noexcept {
        static constexpr float T[5][3] = {
            { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
            { 570,  840, 2410 }, { 300,  870, 2240 } };
        v = std::min(4, std::max(0, v));
        f[0] = T[v][0]; f[1] = T[v][1]; f[2] = T[v][2];
    }
    void setBandpass(Biquad& bq, double fc, double Q) noexcept {
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs_;
        const double al = std::sin(w0) / (2.0 * Q), a0 = 1.0 + al;
        bq.b0 = (float)(al / a0); bq.b1 = 0.0f; bq.b2 = (float)(-al / a0);
        bq.a1 = (float)((-2.0 * std::cos(w0)) / a0);
        bq.a2 = (float)((1.0 - al) / a0);
    }

    static constexpr float kFormantMakeup = 2.2f;            // narrowband gain lift
    static constexpr int kCombMax = 2048;                    // >= fs/20 at 44.1/48k
    int    kind_ = notch;
    double fs_ = 44100.0, hz_ = 1000.0, res_ = 0.0;
    bool   dirty_ = true;

    Biquad bq_[2];                                           // notch, N+LP-LP
    Biquad fbq_[3];                                          // formant resonators
    Allpass1 ap_[4];
    float  comb_[kCombMax] = {};
    int    combIdx_ = 0;
    float  combDf_ = 100.0f, combDfTarget_ = 100.0f, combG_ = 0.5f;
};

} // namespace dreamer
