// PcmOsc3 -- bank-v3 playback oscillator (DSP_BUILD.md section 2).
// Extends the v2 PcmOscillator semantics without touching it:
//
//   Cycle   : the v2 16.16 code path COPIED VERBATIM (same increment
//             quantization, same integer lerp) -- bit-identical to v2 on
//             cycle entries (asserted in test_bank3).
//   Loop    : 32.32 fixed-point phase over the full buffer, wraps to
//             loopStart. Pitch ratio = noteHz / rootHz -- one root
//             stretched across the keyboard, chipmunk shift INTENTIONAL.
//   OneShot : Loop without the wrap; finished() goes true at the end and
//             output is silence after.
//
// Interp modes unchanged (Linear default / DropSample), integer lerp form,
// no band-limiting, no mip levels. START semantics (section 2): the caller
// passes a 0..1 fraction for every type; Cycle maps it to 0..599 samples.
//
// C++17, header-only, JUCE-free, real-time safe.

#pragma once
#include <cstdint>
#include "dsp/bank/Bank3.h"

namespace dreamer {

class PcmOsc3 {
public:
    enum class Interp { DropSample, Linear };

    void setSampleRate(double sr) noexcept { sampleRate_ = sr; updateIncrement(); }
    void setFrequency(double hz)  noexcept { freq_ = hz;      updateIncrement(); }
    void setInterp(Interp m)      noexcept { interp_ = m; }

    void setWaveform(int idx) noexcept {
        if (idx < 0 || idx >= rompler::bank3::kNumWaveforms) idx = 0;
        wf_ = &rompler::bank3::kWaveforms[(size_t)idx];
        updateIncrement();
    }

    // start = 0..1 fraction of the buffer (Cycle: of the 600-sample cycle)
    void reset(double start01 = 0.0) noexcept {
        if (start01 < 0.0) start01 = 0.0;
        if (start01 > 1.0) start01 = 1.0;
        finished_ = false;
        if (wf_->type == rompler::bank3::WaveType::Cycle) {
            phase32_ = (uint32_t)(start01 * 599.0 * 65536.0) % kCycleWrap;
        } else {
            const double s = start01 * (double)(wf_->length - 1);
            phase64_ = (uint64_t)(s * 4294967296.0);
        }
    }

    bool finished() const noexcept { return finished_; }

    float process() noexcept {
        const auto* w = wf_;
        if (w->type == rompler::bank3::WaveType::Cycle) {
            // ---- v2 cycle path, verbatim math (16.16 over 600) ----------
            const uint32_t idx = phase32_ >> 16;
            float out;
            if (interp_ == Interp::DropSample) {
                out = (float)w->samples[idx] * kNorm;
            } else {
                const uint32_t frac = phase32_ & 0xFFFFu;
                const uint32_t nxt  = (idx + 1 == 600u) ? 0u : idx + 1;
                const int32_t a = w->samples[idx];
                const int32_t b = w->samples[nxt];
                const int32_t v = a + (int32_t)(((int64_t)(b - a) * frac) >> 16);
                out = (float)v * kNorm;
            }
            phase32_ += inc32_;
            if (phase32_ >= kCycleWrap) phase32_ -= kCycleWrap;
            return out;
        }

        // ---- Loop / OneShot: 32.32 phase over the buffer ----------------
        if (finished_) return 0.0f;
        const uint32_t len = w->length;
        uint32_t idx = (uint32_t)(phase64_ >> 32);
        if (idx >= len) {                       // safety (shouldn't happen)
            finished_ = (w->type == rompler::bank3::WaveType::OneShot);
            idx = finished_ ? len - 1 : w->loopStart;
        }
        float out;
        if (interp_ == Interp::DropSample) {
            out = (float)w->samples[idx] * kNorm;
        } else {
            const uint32_t frac16 = (uint32_t)(phase64_ >> 16) & 0xFFFFu;   // 16-bit frac, ASIC form
            uint32_t nxt = idx + 1;
            if (nxt >= len)
                nxt = (w->type == rompler::bank3::WaveType::Loop) ? w->loopStart : len - 1;
            const int32_t a = w->samples[idx];
            const int32_t b = w->samples[nxt];
            const int32_t v = a + (int32_t)(((int64_t)(b - a) * frac16) >> 16);
            out = (float)v * kNorm;
        }
        phase64_ += inc64_;
        const uint64_t end = (uint64_t)len << 32;
        if (phase64_ >= end) {
            if (w->type == rompler::bank3::WaveType::Loop) {
                phase64_ -= end - ((uint64_t)w->loopStart << 32);
            } else {
                finished_ = true;
            }
        }
        return out;
    }

private:
    static constexpr uint32_t kCycleWrap = 600u << 16;
    static constexpr float    kNorm      = 1.0f / 32768.0f;

    void updateIncrement() noexcept {
        if (wf_->type == rompler::bank3::WaveType::Cycle) {
            const double step = freq_ * 600.0 / sampleRate_;      // v2 law
            inc32_ = (uint32_t)(step * 65536.0 + 0.5);
        } else {
            const double root  = wf_->rootHz > 0.0f ? (double)wf_->rootHz : 220.0;
            const double ratio = (freq_ / root) * (44100.0 / sampleRate_);
            inc64_ = (uint64_t)(ratio * 4294967296.0 + 0.5);
        }
    }

    const rompler::bank3::Waveform* wf_ = &rompler::bank3::kWaveforms[0];
    double   sampleRate_ = 44100.0;
    double   freq_       = 440.0;
    uint32_t phase32_    = 0;
    uint32_t inc32_      = 0;
    uint64_t phase64_    = 0;
    uint64_t inc64_      = 0;
    Interp   interp_     = Interp::Linear;
    bool     finished_   = false;
};

} // namespace dreamer
