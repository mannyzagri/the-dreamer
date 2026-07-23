// ToneFilter -- TD-007 per-tone TVF composite (new glue, NOT a port; rule 1
// does not apply to this file -- the ported RubberFilter it OWNS is untouched).
// One mono filter per tone dispatching the FULL 14-entry panel type list with
// GlobalFilter's EXACT index map:
//
//   0-3   LP24 / LP12 / BP / HP        built-in ToneSvf core
//   4-6   LIQUID / CLASSIC / LADDER    dreamer::RubberFilter (port, verbatim)
//   7-12  NOTCH / COMB+ / COMB- / N+LP / FORMANT / ALLPASS   FilterExtra glue
//   13    DREAMPLN                     ZPlaneFilter glue (E-MU Z-plane morph)
//
// BIT-EXACTNESS RULE (deliberate asymmetry -- do not "clean up"): for types
// 0-3 the output is the BARE ToneSvf result -- no safety() soft-limit, no
// trim, no extra arithmetic of any kind -- float-for-float identical to the
// pre-TD-007 per-tone `svf_.process(x)` path, so every existing golden render
// with tvf_type 0-3 still hashes. safety() bounding and the kFamilyTrim level
// match apply to families 4-13 ONLY (they are new per-tone territory).
//
// RESET RULE: intra-0-3 mode changes call svf setMode only (today's live
// behavior -- no reset, no click). A cross-family type change resets ONLY the
// entering family's filter (the others keep their state but are not audible).
// Intra-4-6 Rhino subtype changes reset the Rhino core (mirrors GlobalFilter,
// which resets on any type change); intra-7-12 kind changes reset inside
// FilterExtra::setKind itself.
//
// RES CURVE: families 4-6 replicate GlobalFilter's res^0.35 perceptual curve
// (FIX1) glue-side, so the per-tone CLASSIC/LIQUID resonance knob feels the
// same as the global slots did. The ported filter math is untouched -- only
// the VALUE passed to setParams is shaped.
//
// R3 RECALC STAGGER (families 4-13 only): the tones' 16-sample control
// counters all start at 0 on noteOn, so a big chord would fire up to 96
// expensive coefficient recalcs (Rhino setParams / FilterExtra recalc /
// ZPlane recalc) in the SAME sample. setCutoffRes/setMorph therefore only
// LATCH pending values for 4-13; the expensive apply happens on this
// instance's own free-running 16-sample grid, whose phase is staggered per
// (voice,tone) via setStagger(). Types 0-3 forward DIRECTLY (ToneSvf's
// setCutoffRes is two transcendentals -- cheap) so their update timing is
// untouched (bit-exactness rule). Worst-case coefficient latency for 4-13 is
// <16 samples (~0.33 ms @48k) -- below the smoothing already inside those
// filters.
//
// LEVEL TRIM: kFamilyTrim[14] is a glue-side, ear-tunable per-type output
// trim (TD-006 level-match approach: single constants, tune by ear). All
// 1.0f until the ear pass says otherwise. Applied to families 4-13 ONLY
// (0-3 must stay bit-exact).
//
// All members fixed storage; nothing allocates after prepare. C++17,
// header-only, JUCE-free, real-time safe after setSampleRate().

#pragma once
#include <cmath>
#include "dsp/glue/ToneSvf.h"
#include "dsp/glue/FilterExtra.h"
#include "dsp/glue/ZPlaneFilter.h"
#include "dsp/ported/RhinoFilter.h"

namespace dreamer {

class ToneFilter {
public:
    static constexpr int kNumTypes = 14;

    void setSampleRate(double sr) noexcept {
        svf_.setSampleRate(sr);
        rhino_.prepare(sr);
        extra_.setSampleRate(sr);
        zplane_.setSampleRate(sr);
    }

    // R3: per-(voice,tone) initial phase 0..15 of the 4-13 coefficient grid.
    // Call once at prepare; the grid then free-runs so chord noteOns stay
    // phase-spread.
    void setStagger(int phase) noexcept {
        if (phase < 0) phase = 0;
        grid_ = 1 + (phase & 15);
    }

    void setOversample(int f) noexcept { rhino_.setOversample(f); }

    void setType(int t) noexcept {
        if (t < 0) t = 0; if (t >= kNumTypes) t = kNumTypes - 1;
        if (t == type_) return;
        const int oldFam = familyOf(type_);
        const int newFam = familyOf(t);
        type_ = t;
        if (newFam == 0) {
            svf_.setMode(t);                       // intra-0-3: setMode only
            if (oldFam != 0) svf_.reset();         // cross-family: reset entrant
            return;
        }
        if (newFam == 1) {
            rhino_.reset();      // cross-family entrant OR intra-4-6 subtype
                                 // change (mirrors GlobalFilter's reset-on-change)
        } else if (newFam == 2) {
            extra_.setKind(t - 7);                 // resets itself on kind change
            if (oldFam != 2) extra_.reset();
        } else {
            if (oldFam != 3) zplane_.reset();
        }
        pend_ = true;                              // re-apply params to entrant
    }

    // Shared Z-plane MORPH (flt2_morph + matrix Morph dest). Latched; only
    // meaningful for type 13, harmlessly stored otherwise.
    void setMorph(float m) noexcept {
        if (m < 0.0f) m = 0.0f; else if (m > 1.0f) m = 1.0f;
        if (m != morph_) { morph_ = m; pend_ = true; }
    }

    void setCutoffRes(double hz, double res01) noexcept {
        if (type_ <= 3) { svf_.setCutoffRes(hz, res01); return; }   // direct, bit-exact cadence
        if (hz != hz_ || res01 != res_) { hz_ = hz; res_ = res01; pend_ = true; }
    }

    float process(float x) noexcept {
        if (type_ <= 3)
            return svf_.process(x);                // BARE -- bit-exactness rule
        if (--grid_ <= 0) {                        // staggered 16-sample grid
            grid_ = 16;
            if (pend_) { pend_ = false; applyPending(); }
        }
        float y;
        if (type_ <= 6)       y = rhino_.processSample(x);
        else if (type_ <= 12) y = extra_.process(x);
        else                  y = zplane_.process(x);
        return safety(y) * kFamilyTrim[type_];
    }

    void reset() noexcept {
        svf_.reset(); rhino_.reset(); extra_.reset(); zplane_.reset();
        // grid_ deliberately NOT reset -- the stagger phase free-runs (R3)
    }

private:
    static int familyOf(int t) noexcept {
        return t <= 3 ? 0 : t <= 6 ? 1 : t <= 12 ? 2 : 3;
    }

    void applyPending() noexcept {
        if (type_ <= 6) {                          // 4-6 Rhino (res^0.35 -- FIX1)
            const float resEff = std::pow((float)res_, 0.35f);
            static constexpr RubberFilter::Type map[3] = {
                RubberFilter::Type::liquid, RubberFilter::Type::classic,
                RubberFilter::Type::ladder };
            rhino_.setParams((float)hz_, resEff, character_, map[type_ - 4]);
        } else if (type_ <= 12) {
            extra_.setCutoffRes(hz_, res_);
        } else {
            zplane_.setMorph((double)morph_);
            zplane_.setCutoffRes(hz_, res_);
        }
    }

    // GlobalFilter's output safety net, replicated verbatim for families 4-13:
    // transparent up to ~0.8, soft-limits above, asymptote ~1.5.
    static float safety(float x) noexcept {
        const float a = std::fabs(x);
        if (a <= 0.8f) return x;
        const float s = x < 0.0f ? -1.0f : 1.0f;
        return s * (0.8f + 0.7f * std::tanh((a - 0.8f) / 0.7f));
    }

    // Ear-tunable per-type output trim (TD-006 approach). Index = panel type.
    // Entries 0-3 exist for table symmetry but are NEVER applied (bit-exact
    // rule: the 0-3 path returns before any trim).
    static constexpr float kFamilyTrim[kNumTypes] = {
        1.0f, 1.0f, 1.0f, 1.0f,        // 0-3  SVF (unused -- bare path)
        1.0f, 1.0f, 1.0f,              // 4-6  Rhino
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,   // 7-12 FilterExtra
        1.0f };                        // 13   DreamPlane (ZPlane has its own kMakeup)

    int                     type_  = 0;
    ToneSvf                 svf_;
    RubberFilter            rhino_;
    FilterExtra             extra_;
    ZPlaneFilter            zplane_;
    RubberFilter::Character character_{};   // permanently disabled (scope decision 5)
    double hz_ = 1000.0, res_ = 0.0;        // latched pending params (4-13)
    float  morph_ = 0.0f;
    bool   pend_  = true;
    int    grid_  = 1;                      // free-running 16-sample apply grid
};

} // namespace dreamer
