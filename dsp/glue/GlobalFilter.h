// GlobalFilter -- one channel of a global filter slot (FEATURES.md section 5).
// Two slots sit on the voice-sum bus with SER/PAR routing (the processor owns
// 4 instances: slot1 L/R + slot2 L/R).
//
// Type list is the FULL 14-entry panel order from the spec so the choice
// count NEVER changes post-release (Cubase save-compat).
//
//   1 LP24  2 LP12  3 BP  4 HP        built-in ToneSvf core
//   5 LIQUID  6 CLASSIC  7 LADDER     Rubber-Rhino ports (verbatim, Character off)
//   8 NOTCH 9 COMB+ 10 COMB- 11 N+LP 12 FORMANT 13 ALLPASS   V1.1 -> FilterExtra
//   14 DREAMPLN                                    E-MU Z-plane morph -> ZPlaneFilter
//
// (0-based: 0-3 SVF, 4-6 Rhino, 7-12 FilterExtra, 13 ZPlaneFilter.)
// C++17, header-only, JUCE-free, real-time safe after setSampleRate().

#pragma once
#include <cmath>
#include "dsp/glue/ToneSvf.h"
#include "dsp/glue/FilterExtra.h"
#include "dsp/glue/ZPlaneFilter.h"
#include "dsp/ported/RhinoFilter.h"

namespace dreamer {

class GlobalFilter {
public:
    static constexpr int kNumTypes = 14;

    void setSampleRate(double sr) noexcept {
        svf_.setSampleRate(sr);
        rhino_.prepare(sr);
        extra_.setSampleRate(sr);
        zplane_.setSampleRate(sr);
    }
    void setOversample(int f) noexcept { rhino_.setOversample(f); }

    void setType(int t) noexcept {
        if (t < 0) t = 0; if (t >= kNumTypes) t = kNumTypes - 1;
        if (t != type_) { type_ = t; svf_.reset(); rhino_.reset(); extra_.reset(); zplane_.reset(); }
        if (t <= 3) svf_.setMode(t);
        else if (t >= 7 && t <= 12) extra_.setKind(t - 7);
    }

    // MORPH knob (flt2_morph) -- only meaningful for type 13 (DreamPlane);
    // stored harmlessly for other types. See GlobalFilter routing note.
    void setMorph(float m) noexcept { zplane_.setMorph((double)m); }

    void setCutoffRes(double hz, double res01) noexcept {
        if (type_ <= 3) {
            svf_.setCutoffRes(hz, res01);
        } else if (type_ <= 6) {
            // FIX1: the Rhino res->q law rings audibly only as q->0 (res near
            // 1), so a LINEAR res knob feels dead 0..~0.6 then rings hard near
            // the top (user report on LADDER; the q-law types LIQUID/CLASSIC
            // measure the classic bunching -- probe_filter). Apply a perceptual
            // curve to the res VALUE here (glue) so the ring onset spreads
            // evenly across the knob. res^0.35 measured to linearise the peak
            // rise (classic: +3 dB onset moves res 0.68 -> 0.35, smooth to the
            // top). NB the ported LADDER's feedback law never nears self-
            // oscillation, so it is only weakly resonant at any cutoff; the
            // curve spreads its mild tone change too but cannot conjure a ring
            // the port does not have. The ported filter math is UNTOUCHED --
            // only the value passed to setParams is shaped (rule-1 safe;
            // filter_port tests the RhinoFilterSlot adapter, not GlobalFilter).
            const float resEff = std::pow((float)res01, 0.35f);
            static constexpr RubberFilter::Type map[3] = {
                RubberFilter::Type::liquid, RubberFilter::Type::classic,
                RubberFilter::Type::ladder };
            rhino_.setParams((float)hz, resEff, character_, map[type_ - 4]);
        } else if (type_ <= 12) {
            extra_.setCutoffRes(hz, res01);          // V1.1 types 7-12
        } else {
            zplane_.setCutoffRes(hz, res01);         // 13 DREAMPLN (Z-plane morph)
        }
    }

    float process(float in) noexcept {
        if (type_ <= 3)       return safety(svf_.process(in));
        else if (type_ <= 6)  return safety(rhino_.processSample(in));  // clamp the Rhino path too
        else if (type_ <= 12) return safety(extra_.process(in));        // V1.1 (comb/formant can peak)
        else                  return safety(zplane_.process(in));       // 13 DREAMPLN Z-plane morph
    }

    void reset() noexcept { svf_.reset(); rhino_.reset(); extra_.reset(); zplane_.reset(); }

private:
    // Output safety net for the built-in resonant SVF: transparent up to ~0.8,
    // soft-limits above and asymptotes to ~1.5 so extreme resonance (or a
    // BAL sweep that solos a resonant slot) can't overflow into the FX/master.
    static float safety(float x) noexcept {
        const float a = std::fabs(x);
        if (a <= 0.8f) return x;
        const float s = x < 0.0f ? -1.0f : 1.0f;
        return s * (0.8f + 0.7f * std::tanh((a - 0.8f) / 0.7f));
    }

    int                     type_ = 0;
    ToneSvf                 svf_;
    RubberFilter            rhino_;
    FilterExtra             extra_;          // V1.1 types 7-12 (new glue)
    ZPlaneFilter            zplane_;         // type 13 DREAMPLN (new glue, Z-plane morph)
    RubberFilter::Character character_{};   // permanently disabled (scope decision 5)
};

} // namespace dreamer
