// ZPlaneFilter -- "DreamPlane" (GlobalFilter type 13). A credible E-MU
// Audity/Morpheus/Xtreme-Lead-style Z-PLANE MORPHING filter, built NEW for
// this project (NOT a port; rule 1 does not apply -- original glue). C++17,
// header-only, JUCE-free, RT-safe after setSampleRate().
//
// Structure: a SERIES cascade of kSections (6) second-order PEAKING sections
// -> a 12-pole filter whose combined response is a multi-peak "vowel/formant"
// shape, the Z-plane signature. Each section is a resonant peaking (bell)
// biquad centred at angle theta with a sharpness set by the pole radius r
// (Q = 1/(2(1-r))): unity gain away from theta, a resonant boost AT theta, so
// cascading the six stays full-level in the passband (a narrow-bandpass
// cascade would multiply to silence) and MORPH moves where the six bumps sit.
//
//   FRAMES  -- kFrames (3) fixed pole sets, each = 6 (freqHz, r) pairs giving
//              a distinct musical shape:
//                frame 0 "GLASS"  : low round lowpass-tilt stack
//                frame 1 "VOWEL"  : sharp Ah/Aa-ish formant cluster
//                frame 2 "BRITE"  : bright nasal/upper band stack
//   MORPH   -- 0..1 crossfades frame0 -> frame1 -> frame2 by interpolating
//              each section's (log freq, r) between the bracketing frames.
//              Coeffs recomputed at CONTROL RATE and one-pole smoothed.
//   CUTOFF  -- global frequency shift: scales every section freq by hz/kRefHz
//              (a Z-plane "transform"), so a filter-envelope cutoff sweep
//              walks the whole formant set up/down.
//   RES     -- pushes every pole radius toward the unit circle (sharper, more
//              resonant peaks), clamped < kRmax for guaranteed stability.
//
// Not a bit-exact model of any specific E-MU chip; a musical, stable morph.
// (Exact X-Lead frame tables may replace kFrame* later.)

#pragma once
#include <cmath>
#include <algorithm>

namespace dreamer {

class ZPlaneFilter {
public:
    static constexpr int kSections = 6;
    static constexpr int kFrames   = 3;

    void setSampleRate(double sr) noexcept { fs_ = sr > 0.0 ? sr : 44100.0; dirty_ = true; }

    void setMorph(double m) noexcept {
        m = std::clamp(m, 0.0, 1.0);
        if (m != morph_) { morph_ = m; dirty_ = true; }
    }

    void setCutoffRes(double hz, double res01) noexcept {
        const double h = std::clamp(hz, 40.0, fs_ * 0.48);
        const double r = std::clamp(res01, 0.0, 1.0);
        if (h != hz_ || r != res_ || dirty_) { hz_ = h; res_ = r; dirty_ = false; recalc(); }
    }

    void reset() noexcept {
        for (auto& s : sec_) s.reset();
        smoothed_ = false;
    }

    float process(float x) noexcept {
        // glide coefficients toward target (control-rate smoothing, de-click)
        for (int k = 0; k < kSections; ++k) sec_[k].glide(tgt_[k], 0.02f);
        float y = x;
        for (int k = 0; k < kSections; ++k) y = sec_[k].tick(y);
        return y * kMakeup;
    }

private:
    struct Biquad {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;   // live coeffs
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void reset() noexcept { x1 = x2 = y1 = y2 = 0; }
        void glide(const Biquad& t, float c) noexcept {
            b0 += c * (t.b0 - b0); b1 += c * (t.b1 - b1); b2 += c * (t.b2 - b2);
            a1 += c * (t.a1 - a1); a2 += c * (t.a2 - a2);
        }
        float tick(float x) noexcept {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    static constexpr double kRefHz  = 1000.0;   // frames authored at this cutoff
    static constexpr float  kRmax   = 0.9975f;  // pole-radius stability clamp
    // TD (2.5.6): the 6 cascaded peaking sections accumulate output level, so
    // DreamPlane measured +9.5 dB vs LP24 / +5.75 dB vs the mean of the other
    // 13 types (probe: white noise, cut 2000, res 0.4) -- audibly the loudest.
    // -5.2 dB makeup lands its broadband RMS on the pack mean (+0.5 dB) while
    // keeping the resonant formant peak audible (~6.4 dB, was ~11.6). Ear-
    // tunable single constant (down = quieter/tamer, up = louder/more resonant).
    static constexpr float  kMakeup = 0.55f;    // level-match to the filter pack (was 1.0)
    static constexpr double kBoostDb = 11.0;    // per-section resonant boost

    // frame tables: {freqHz, radius} x 6 sections, x 3 frames.
    static const double (&frame(int f))[kSections][2] {
        static const double G[kFrames][kSections][2] = {
            // 0 GLASS -- low, round, lowpass-tilted (stronger/lower = louder)
            { {180,0.90},{300,0.90},{520,0.88},{820,0.86},{1250,0.83},{1900,0.79} },
            // 1 VOWEL -- Ah/Aa formant cluster (sharp, high-r pairs)
            { {650,0.965},{720,0.965},{1050,0.955},{1180,0.955},{2500,0.945},{2750,0.945} },
            // 2 BRITE -- bright nasal / upper band spread
            { {330,0.86},{950,0.90},{2050,0.945},{3100,0.95},{4400,0.94},{6000,0.90} },
        };
        return G[std::clamp(f, 0, kFrames - 1)];
    }

    // RBJ peaking (bell) biquad: centre theta, sharpness from the pole radius
    // r (Q = 1/(2(1-r))), fixed resonant boost. Unity away from theta -> the
    // six-section cascade keeps passband level while bumping the formants.
    // Always stable (peaking-EQ poles are inside the unit circle for any Q>0).
    static void makeSection(Biquad& q, double r, double theta) noexcept {
        r = std::min(r, (double)kRmax);
        const double Q  = 1.0 / (2.0 * (1.0 - r));          // r -> Q (radius = sharpness)
        const double A  = std::pow(10.0, kBoostDb / 40.0);  // sqrt of linear boost
        const double w0 = theta;
        const double al = std::sin(w0) / (2.0 * Q);
        const double cw = std::cos(w0);
        const double a0 = 1.0 + al / A;
        q.b0 = (float)((1.0 + al * A) / a0);
        q.b1 = (float)((-2.0 * cw)   / a0);
        q.b2 = (float)((1.0 - al * A) / a0);
        q.a1 = (float)((-2.0 * cw)   / a0);
        q.a2 = (float)((1.0 - al / A) / a0);
    }

    void recalc() noexcept {
        const double seg   = morph_ * (kFrames - 1);       // 0 .. kFrames-1
        const int    i0    = std::min((int)seg, kFrames - 2);
        const double fr    = seg - i0;                       // 0..1 within segment
        const auto&  A     = frame(i0);
        const auto&  B     = frame(i0 + 1);
        const double cutoffScale = hz_ / kRefHz;             // Z-plane freq shift
        const double rBoost      = res_;                     // 0..1

        for (int k = 0; k < kSections; ++k) {
            // interpolate freq in log2 (musical), radius linearly
            const double lf = (1.0 - fr) * std::log2(A[k][0]) + fr * std::log2(B[k][0]);
            double freq = std::exp2(lf) * cutoffScale;
            freq = std::clamp(freq, 30.0, fs_ * 0.47);
            double rr = (1.0 - fr) * A[k][1] + fr * B[k][1];
            rr = rr + rBoost * (kRmax - rr);                 // RES -> toward unit circle
            rr = std::min(rr, (double)kRmax);
            makeSection(tgt_[k], rr, 2.0 * 3.14159265358979323846 * freq / fs_);
        }
        if (!smoothed_) {                                    // first calc: snap, no glide
            for (int k = 0; k < kSections; ++k) {
                Biquad t = tgt_[k];
                t.x1 = sec_[k].x1; t.x2 = sec_[k].x2; t.y1 = sec_[k].y1; t.y2 = sec_[k].y2;
                sec_[k] = t;
            }
            smoothed_ = true;
        }
    }

    double fs_ = 44100.0, hz_ = 1000.0, res_ = 0.0, morph_ = 0.0;
    bool   dirty_ = true, smoothed_ = false;
    Biquad sec_[kSections];   // live (glided) sections
    Biquad tgt_[kSections];   // control-rate targets
};

} // namespace dreamer
