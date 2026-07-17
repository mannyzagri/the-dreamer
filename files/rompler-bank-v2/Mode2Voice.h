// Mode2Voice -- JD-990-style dual-partial ROMpler voice topology
//
// Structure (per voice):
//
//   Partial A: PcmOscillator -> TVF -> TVA ---\
//                                              (+) -> voice out -> [Rubber-Rhino FX]
//   Partial B: PcmOscillator -> TVF -> TVA ---/
//
// Per partial: waveform select, coarse (semitones) + fine (cents) detune,
// level, velocity sensitivity, sample-start offset (the E-MU trick),
// TVF with cutoff/resonance/env amount/key follow, independent TVF and
// TVA envelopes.
//
// Filter seam: the partial owns a FilterSlot*. The built-in Tvf2Pole is a
// plain Chamberlin SVF -- a placeholder with the right control surface.
// To use the Rubber-Rhino filters, adapt them to FilterSlot (three methods)
// and inject via Partial::setFilter(). Everything else stays untouched.
//
// Envelopes are ADSR here; the JD-990's TVF/TVA envelopes were 4-stage
// with sustain -- ADSR is the honest subset and the extension point is
// isolated in EnvelopeAdsr.
//
// C++17, header-only, no dependencies beyond PcmOscillator/RomplerBank.

#pragma once
#include <cmath>
#include <cstdint>
#include "PcmOscillator.h"

namespace rompler {

// ---------------------------------------------------------------- envelope
class EnvelopeAdsr {
public:
    void setSampleRate(double sr) noexcept { sr_ = sr; }
    // times in seconds, sustain 0..1
    void set(double a, double d, double s, double r) noexcept {
        att_ = rate(a); dec_ = rate(d); sus_ = s; rel_ = rate(r);
    }
    void noteOn() noexcept  { stage_ = Stage::Attack; }
    void noteOff() noexcept { if (stage_ != Stage::Idle) stage_ = Stage::Release; }
    bool isActive() const noexcept { return stage_ != Stage::Idle; }

    float process() noexcept {
        switch (stage_) {
        case Stage::Attack:
            lvl_ += att_;
            if (lvl_ >= 1.0) { lvl_ = 1.0; stage_ = Stage::Decay; }
            break;
        case Stage::Decay:
            lvl_ += (sus_ - lvl_) * dec_;
            if (lvl_ - sus_ < 1e-4) { lvl_ = sus_; stage_ = Stage::Sustain; }
            break;
        case Stage::Sustain: break;
        case Stage::Release:
            lvl_ -= lvl_ * rel_;
            if (lvl_ < 1e-4) { lvl_ = 0.0; stage_ = Stage::Idle; }
            break;
        case Stage::Idle: lvl_ = 0.0; break;
        }
        return static_cast<float>(lvl_);
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    double rate(double seconds) const noexcept {
        return seconds <= 0.0 ? 1.0 : 1.0 / (seconds * sr_);
    }
    double sr_ = 44100.0, att_ = 1.0, dec_ = 0.01, sus_ = 1.0, rel_ = 0.01;
    double lvl_ = 0.0;
    Stage stage_ = Stage::Idle;
};

// ---------------------------------------------------------------- filter seam
struct FilterSlot {
    virtual ~FilterSlot() = default;
    virtual void setSampleRate(double sr) = 0;
    virtual void setCutoffRes(double hz, double res01) = 0;  // res 0..1
    virtual float process(float in) = 0;
    virtual void reset() = 0;
};

// Built-in placeholder TVF: Chamberlin SVF lowpass. Swap for Rubber-Rhino.
class Tvf2Pole final : public FilterSlot {
public:
    void setSampleRate(double sr) override { sr_ = sr; }
    void setCutoffRes(double hz, double res01) override {
        hz = std::min(hz, sr_ * 0.22);   // Chamberlin stability margin
        f_ = 2.0 * std::sin(M_PI * hz / sr_);
        q_ = 1.0 - 0.95 * std::min(1.0, std::max(0.0, res01));
    }
    float process(float in) override {
        // two half-steps for a bit more headroom
        for (int i = 0; i < 2; ++i) {
            low_  += f_ * 0.5 * band_;
            const double high = in - low_ - q_ * band_;
            band_ += f_ * 0.5 * high;
        }
        return static_cast<float>(low_);
    }
    void reset() override { low_ = band_ = 0.0; }
private:
    double sr_ = 44100.0, f_ = 0.5, q_ = 1.0, low_ = 0.0, band_ = 0.0;
};

// ---------------------------------------------------------------- partial
struct PartialParams {
    int    waveIndex     = 0;      // into bank::kWaveforms
    int    coarse        = 0;      // semitones
    double fineCents     = 0.0;
    double level         = 1.0;    // 0..1
    double velSens       = 0.5;    // 0..1, amount velocity scales level
    double startOffset   = 0.0;    // 0..599 samples, E-MU trick
    bool   enabled       = true;

    // TVF
    double cutoffHz      = 8000.0;
    double resonance     = 0.0;    // 0..1
    double tvfEnvAmount  = 0.0;    // Hz added at env peak (can be negative)
    double tvfKeyFollow  = 0.5;    // 0..1, cutoff tracks note
    double tvfA = 0.005, tvfD = 0.3, tvfS = 1.0, tvfR = 0.3;

    // TVA
    double tvaA = 0.005, tvaD = 0.3, tvaS = 1.0, tvaR = 0.3;
};

class Partial {
public:
    Partial() { setFilter(&builtin_); }

    void setSampleRate(double sr) noexcept {
        sr_ = sr;
        osc_.setSampleRate(sr);
        tvfEnv_.setSampleRate(sr);
        tvaEnv_.setSampleRate(sr);
        filter_->setSampleRate(sr);
    }
    void setFilter(FilterSlot* f) noexcept { filter_ = f ? f : &builtin_; }

    void noteOn(const PartialParams& p, int midiNote, float velocity01) noexcept {
        params_ = p;
        note_ = midiNote;
        osc_.setWaveform(p.waveIndex);
        const double semis = midiNote - 69 + p.coarse + p.fineCents / 100.0;
        osc_.setFrequency(440.0 * std::pow(2.0, semis / 12.0));
        osc_.reset(p.startOffset);
        velGain_ = static_cast<float>((1.0 - p.velSens) + p.velSens * velocity01);
        tvfEnv_.set(p.tvfA, p.tvfD, p.tvfS, p.tvfR);
        tvaEnv_.set(p.tvaA, p.tvaD, p.tvaS, p.tvaR);
        tvfEnv_.noteOn();
        tvaEnv_.noteOn();
        filter_->reset();
        ctrlCount_ = 0;
    }
    void noteOff() noexcept { tvfEnv_.noteOff(); tvaEnv_.noteOff(); }
    bool isActive() const noexcept { return params_.enabled && tvaEnv_.isActive(); }

    float process() noexcept {
        if (!isActive()) { (void)tvfEnv_.process(); return 0.0f; }
        const float tvf = tvfEnv_.process();
        const float tva = tvaEnv_.process();
        // control-rate cutoff update (every 16 samples), like the hardware
        if (ctrlCount_-- <= 0) {
            ctrlCount_ = 15;
            const double keyHz = params_.cutoffHz *
                std::pow(2.0, params_.tvfKeyFollow * (note_ - 60) / 12.0);
            double hz = keyHz + params_.tvfEnvAmount * tvf;
            hz = std::min(std::max(hz, 20.0), 18000.0);
            filter_->setCutoffRes(hz, params_.resonance);
        }
        const float s = filter_->process(osc_.process());
        return s * tva * velGain_ * static_cast<float>(params_.level);
    }

private:
    PcmOscillator osc_;
    EnvelopeAdsr  tvfEnv_, tvaEnv_;
    Tvf2Pole      builtin_;
    FilterSlot*   filter_ = nullptr;
    PartialParams params_;
    double sr_ = 44100.0;
    float  velGain_ = 1.0f;
    int    note_ = 60, ctrlCount_ = 0;
};

// ---------------------------------------------------------------- voice
struct Mode2Patch {
    PartialParams a, b;
};

class Mode2Voice {
public:
    void setSampleRate(double sr) noexcept { a_.setSampleRate(sr); b_.setSampleRate(sr); }
    void setFilters(FilterSlot* fa, FilterSlot* fb) noexcept { a_.setFilter(fa); b_.setFilter(fb); }

    void noteOn(const Mode2Patch& patch, int midiNote, float velocity01) noexcept {
        note_ = midiNote; ++serial_;
        a_.noteOn(patch.a, midiNote, velocity01);
        b_.noteOn(patch.b, midiNote, velocity01);
    }
    void noteOff() noexcept { a_.noteOff(); b_.noteOff(); }
    bool isActive() const noexcept { return a_.isActive() || b_.isActive(); }
    int  note() const noexcept { return note_; }
    uint64_t serial() const noexcept { return serial_; }

    float process() noexcept { return a_.process() + b_.process(); }

private:
    Partial a_, b_;
    int note_ = -1;
    uint64_t serial_ = 0;
};

// ---------------------------------------------------------------- poly manager
template <int MaxVoices = 8>   // JD-990: 24; Orbit: 32. 8 is dev default.
class Mode2Synth {
public:
    void setSampleRate(double sr) noexcept {
        for (auto& v : voices_) v.setSampleRate(sr);
    }
    Mode2Patch& patch() noexcept { return patch_; }

    void noteOn(int midiNote, float velocity01) noexcept {
        Mode2Voice* target = nullptr;
        for (auto& v : voices_) if (!v.isActive()) { target = &v; break; }
        if (!target) {                      // steal oldest
            uint64_t best = UINT64_MAX;
            for (auto& v : voices_)
                if (v.serial() < best) { best = v.serial(); target = &v; }
        }
        stamp_++;
        target->noteOn(patch_, midiNote, velocity01);
    }
    void noteOff(int midiNote) noexcept {
        for (auto& v : voices_)
            if (v.isActive() && v.note() == midiNote) v.noteOff();
    }
    float process() noexcept {
        float s = 0.0f;
        for (auto& v : voices_) s += v.process();
        return s;
    }

private:
    Mode2Voice voices_[MaxVoices];
    Mode2Patch patch_;
    uint64_t stamp_ = 0;
};

} // namespace rompler
