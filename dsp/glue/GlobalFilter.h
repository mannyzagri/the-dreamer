// GlobalFilter -- one channel of a global filter slot (FEATURES.md section 5).
// Two slots sit on the voice-sum bus with SER/PAR routing (the processor owns
// 4 instances: slot1 L/R + slot2 L/R).
//
// Type list is the FULL 14-entry panel order from the spec so the choice
// count NEVER changes post-release (Cubase save-compat). v1 implements 1-7;
// the V1.1 / V2 entries (8-14) are BYPASS (transparent) until their phases
// land -- documented, not silent aliases.
//
//   1 LP24  2 LP12  3 BP  4 HP        built-in ToneSvf core
//   5 LIQUID  6 CLASSIC  7 LADDER     Rubber-Rhino ports (verbatim, Character off)
//   8 NOTCH 9 COMB+ 10 COMB- 11 NL3 12 FORMANT 13 ALLPASS   [V1.1 -> bypass]
//   14 DREAMPLN                                              [V2  -> bypass]
//
// C++17, header-only, JUCE-free, real-time safe after setSampleRate().

#pragma once
#include <cmath>
#include "dsp/glue/ToneSvf.h"
#include "dsp/ported/RhinoFilter.h"

namespace dreamer {

class GlobalFilter {
public:
    static constexpr int kNumTypes = 14;

    void setSampleRate(double sr) noexcept {
        svf_.setSampleRate(sr);
        rhino_.prepare(sr);
    }
    void setOversample(int f) noexcept { rhino_.setOversample(f); }

    void setType(int t) noexcept {
        if (t < 0) t = 0; if (t >= kNumTypes) t = kNumTypes - 1;
        if (t != type_) { type_ = t; svf_.reset(); rhino_.reset(); }
        if (t <= 3) svf_.setMode(t);
    }

    void setCutoffRes(double hz, double res01) noexcept {
        if (type_ <= 3) {
            svf_.setCutoffRes(hz, res01);
        } else if (type_ <= 6) {
            static constexpr RubberFilter::Type map[3] = {
                RubberFilter::Type::liquid, RubberFilter::Type::classic,
                RubberFilter::Type::ladder };
            rhino_.setParams((float)hz, (float)res01, character_, map[type_ - 4]);
        }
    }

    float process(float in) noexcept {
        float y;
        if (type_ <= 3)      y = svf_.process(in);
        else if (type_ <= 6) return rhino_.processSample(in);   // Rhino ports self-safety
        else                 return in;                          // 8-14: bypass
        return safety(y);   // built-in SVF has no resonance clamp -> bound it
    }

    void reset() noexcept { svf_.reset(); rhino_.reset(); }

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
    RubberFilter::Character character_{};   // permanently disabled (scope decision 5)
};

} // namespace dreamer
