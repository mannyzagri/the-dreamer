// RhinoFilterSlot -- adapts dreamer::RubberFilter (ported Rubber-Rhino 24 dB
// filter, all 7 types) to the rompler::FilterSlot seam from Mode2Voice.h.
//
// The conversion lives HERE; the ported filter is untouched (CLAUDE.md rule 1).
// FilterSlot's (hz, res01) maps directly onto setParams' native space -- the
// filter takes cutoff in Hz and normalized resonance 0..1. The Character
// ("modification") block is permanently disabled per scope decision 5: a
// default-constructed Character{} is passed on every update and never exposed.
//
// C++17, header-only, JUCE-free, real-time safe after setSampleRate().

#pragma once
#include "dsp/bank/Mode2Voice.h"      // rompler::FilterSlot
#include "dsp/ported/RhinoFilter.h"   // dreamer::RubberFilter

namespace dreamer {

class RhinoFilterSlot final : public rompler::FilterSlot {
public:
    void setSampleRate(double sr) override { filter_.prepare(sr); }

    // NOTE (GAIN_STAGING s4): the spec asked for a resonance-compensation input
    // trim (inGain = 1 - 0.4*res) HERE, but this adapter is under an IMMUTABLE
    // bitwise null test (tests/test_filter_port.cpp: adapter output must equal
    // the ported RubberFilter driven directly -- CLAUDE.md "null tests
    // immutable"), so any gain change here breaks that contract. This adapter is
    // also NOT in the live signal path: the plugin's resonant Rhino filters run
    // through dsp/glue/GlobalFilter.h (GlobalFilter::rhino_), which already wraps
    // every Rhino sample in safety() (soft-limit >0.8, asymptote ~1.5) AND the
    // ported ladder self-saturates (measured peak 1.27 under a 1.5x loud input).
    // So the trim is left OUT here; if the resonant path still clips by ear, the
    // right place is a 1-line input scale in GlobalFilter's type<=6 branch (glue,
    // but outside this task's authorized file set -- flagged to the orchestrator).
    void setCutoffRes(double hz, double res01) override {
        filter_.setParams(static_cast<float>(hz), static_cast<float>(res01),
                          character_, type_);
    }

    float process(float in) override { return filter_.processSample(in); }

    void reset() override { filter_.reset(); }

    // -- extensions used by the glue/processor (not part of the seam) --------
    void setType(int t) noexcept {
        if (t < 0) t = 0;
        if (t > static_cast<int>(RubberFilter::Type::wasp))
            t = static_cast<int>(RubberFilter::Type::wasp);
        type_ = static_cast<RubberFilter::Type>(t);
    }
    void setOversampleFactor(int f) noexcept { filter_.setOversample(f); }

private:
    RubberFilter                 filter_;
    RubberFilter::Character      character_{};   // enabled=false forever
    RubberFilter::Type           type_ = RubberFilter::Type::classic;
};

} // namespace dreamer
