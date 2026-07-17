// DreamVoice -- The Dreamer's voice: an adapted copy of the verified
// rompler::Partial / Mode2Voice / Mode2Synth (dsp/bank/Mode2Voice.h, which
// stays untouched as reference), extended per the 2026-07-17 scope decisions:
//
//   * per-partial RhinoFilterSlot (ported Rubber-Rhino 24 dB filter, 7 types)
//   * 2 LFOs per partial (ported dreamer::Lfo), dest Pitch / Cutoff / Level,
//     key-trigger or free-run, applied at the voice's existing 16-sample
//     control rate -- the stepping is era-honest hardware behavior, do NOT
//     smooth it
//   * control-rate pitch update: base pitch + pitch bend + Instability tune
//     drift + pitch LFO ride one setFrequency call (noteOn no longer sets
//     frequency directly; the first control tick at sample 0 does, the same
//     cadence the original used for cutoff)
//   * live parameter updates on held notes (updateLive): continuous fields
//     (cutoff/res/env amt/keyfollow/level/coarse/fine/filter type/LFOs/
//     enabled) follow the knobs; waveIndex, startOffset, velSens and the
//     ADSR times stay note-on-latched (the verified snapshot design)
//   * fixed 24 voices, oldest-NOTE stealing: the verified Mode2Synth's
//     stamp_ is incremented but never assigned to a voice (its per-voice
//     serial_ counts reuses, not age) -- DreamSynth passes a global stamp
//     into noteOn, which is the fix
//   * sustain pedal (CC64) and all-notes-off handled at the synth level
//
// Depth laws (control-rate, bipolar LFO value v in [-1,+1], depth d in 0..1):
//   Pitch : += v * d*d * 12 semitones   (square-law: fine vibrato low, +/-1 oct max)
//   Cutoff: *= 2^(v * d * 2)            (+/-2 octaves)
//   Level : *= 1 - d*0.5*(1 - v)        (tremolo, gain swings [1-d, 1])
//
// C++17, header-only, JUCE-free, real-time safe after prepare().

#pragma once
#include <cmath>
#include <cstdint>
#include "dsp/bank/Mode2Voice.h"        // rompler::{EnvelopeAdsr,FilterSlot,PartialParams,PcmOscillator}
#include "dsp/glue/RhinoFilterSlot.h"   // dreamer::RhinoFilterSlot
#include "dsp/ported/RhinoLfo.h"        // dreamer::Lfo

namespace dreamer {

// ---------------------------------------------------------------- params
struct LfoParams {
    int   shape   = 0;      // dreamer::Lfo::Shape order: sine/tri/ramp/square/sh
    float rate01  = 50.0f;  // 0..100 -> Lfo::rateHzFromParam (0.05..30 Hz)
    float depth   = 0.0f;   // 0..1, per-dest law above
    int   dest    = 0;      // 0 = Pitch, 1 = Cutoff, 2 = Level
    bool  keyTrig = true;   // true: reset phase at noteOn; false: free-run
};

struct DreamPartialParams {
    rompler::PartialParams base;   // verbatim reuse of the verified struct
    LfoParams lfo[2];
    int filterType = 0;            // dreamer::RubberFilter::Type, 0..6
};

struct DreamPatch {
    DreamPartialParams a, b;
    int interp = 1;                // 0 = DropSample, 1 = Linear
};

// ---------------------------------------------------------------- partial
// Adapted from rompler::Partial (dsp/bank/Mode2Voice.h); the verified logic
// is kept line-for-line, with the LFO/pitch insertions at the existing
// 16-sample control block.
class DreamPartial {
public:
    DreamPartial() { filter_ = &slot_; }

    void prepare(double sr, uint32_t seedBase) noexcept {
        sr_ = sr;
        osc_.setSampleRate(sr);
        tvfEnv_.setSampleRate(sr);
        tvaEnv_.setSampleRate(sr);
        slot_.setSampleRate(sr);
        for (int i = 0; i < 2; ++i)
            lfo_[i].prepare(sr, (0x9E3779B9u * (seedBase * 2u + (uint32_t)i)) | 1u);
    }

    // test seam (mirrors rompler::Partial::setFilter)
    void setFilter(rompler::FilterSlot* f) noexcept { filter_ = f ? f : &slot_; }

    void setInterpMode(int m) noexcept {
        osc_.setInterp(m == 0 ? rompler::PcmOscillator::Interp::DropSample
                              : rompler::PcmOscillator::Interp::Linear);
    }
    void setOversample(int f) noexcept { slot_.setOversampleFactor(f); }

    void noteOn(const DreamPartialParams& p, int midiNote, float velocity01) noexcept {
        params_ = p;
        note_ = midiNote;
        osc_.setWaveform(p.base.waveIndex);
        baseSemis_ = midiNote - 69 + p.base.coarse + p.base.fineCents / 100.0;
        // no osc_.setFrequency here: ctrlCount_ = 0 makes the first process()
        // call run the control block at sample 0, which sets it (same cadence
        // the verified code used for the cutoff)
        osc_.reset(p.base.startOffset);
        velGain_ = static_cast<float>((1.0 - p.base.velSens) + p.base.velSens * velocity01);
        tvfEnv_.set(p.base.tvfA, p.base.tvfD, p.base.tvfS, p.base.tvfR);
        tvaEnv_.set(p.base.tvaA, p.base.tvaD, p.base.tvaS, p.base.tvaR);
        tvfEnv_.noteOn();
        tvaEnv_.noteOn();
        slot_.setType(p.filterType);
        filter_->reset();
        for (int i = 0; i < 2; ++i) {
            lfo_[i].setShape(p.lfo[i].shape);
            lfo_[i].setRateHz(Lfo::rateHzFromParam(p.lfo[i].rate01));
            if (p.lfo[i].keyTrig) lfo_[i].reset();
        }
        lfoLevelGain_ = 1.0f;
        ctrlCount_ = 0;
    }

    void noteOff() noexcept { tvfEnv_.noteOff(); tvaEnv_.noteOff(); }
    // Hard stop: disabling the partial silences it instantly (isActive gates
    // on enabled); the next noteOn re-copies the patch and re-enables.
    void kill() noexcept { noteOff(); params_.base.enabled = false; }
    bool isActive() const noexcept { return params_.base.enabled && tvaEnv_.isActive(); }

    // Live (per-block) update of the continuous fields; waveIndex, startOffset,
    // velSens and ADSR times stay latched from noteOn (documented deviation).
    void updateLive(const DreamPartialParams& p) noexcept {
        params_.base.enabled      = p.base.enabled;
        params_.base.level        = p.base.level;
        params_.base.cutoffHz     = p.base.cutoffHz;
        params_.base.resonance    = p.base.resonance;
        params_.base.tvfEnvAmount = p.base.tvfEnvAmount;
        params_.base.tvfKeyFollow = p.base.tvfKeyFollow;
        params_.base.coarse       = p.base.coarse;
        params_.base.fineCents    = p.base.fineCents;
        baseSemis_ = note_ - 69 + p.base.coarse + p.base.fineCents / 100.0;
        if (p.filterType != params_.filterType) {
            params_.filterType = p.filterType;
            slot_.setType(p.filterType);
        }
        for (int i = 0; i < 2; ++i) {
            params_.lfo[i] = p.lfo[i];
            lfo_[i].setShape(p.lfo[i].shape);
            lfo_[i].setRateHz(Lfo::rateHzFromParam(p.lfo[i].rate01));
        }
    }

    float process(float bendSemis, float driftTuneSemis, float driftCutOct) noexcept {
        lfo_[0].tick();                       // per-sample tick (Lfo contract);
        lfo_[1].tick();                       // ticking while idle keeps free-run honest
        if (!isActive()) { (void)tvfEnv_.process(); return 0.0f; }
        const float tvf = tvfEnv_.process();
        const float tva = tvaEnv_.process();
        // control-rate update (every 16 samples), like the hardware
        if (ctrlCount_-- <= 0) {
            ctrlCount_ = 15;
            double lfoPitchSemis = 0.0, lfoCutOct = 0.0;
            lfoLevelGain_ = 1.0f;
            for (int i = 0; i < 2; ++i) {
                const float v = lfo_[i].value();
                const float d = params_.lfo[i].depth;
                switch (params_.lfo[i].dest) {
                case 0:  lfoPitchSemis += (double)v * d * d * 12.0; break;
                case 1:  lfoCutOct     += (double)v * d * 2.0;      break;
                default: lfoLevelGain_ *= 1.0f - d * 0.5f * (1.0f - v); break;
                }
            }
            osc_.setFrequency(440.0 * std::pow(2.0,
                (baseSemis_ + bendSemis + driftTuneSemis + lfoPitchSemis) / 12.0));
            const double keyHz = params_.base.cutoffHz *
                std::pow(2.0, params_.base.tvfKeyFollow * (note_ - 60) / 12.0);
            double hz = keyHz + params_.base.tvfEnvAmount * tvf;
            hz *= std::pow(2.0, lfoCutOct + (double)driftCutOct);
            hz = std::min(std::max(hz, 20.0), 18000.0);
            filter_->setCutoffRes(hz, params_.base.resonance);
        }
        const float s = filter_->process(osc_.process());
        return s * tva * velGain_ * static_cast<float>(params_.base.level) * lfoLevelGain_;
    }

private:
    rompler::PcmOscillator osc_;
    rompler::EnvelopeAdsr  tvfEnv_, tvaEnv_;
    RhinoFilterSlot        slot_;
    rompler::FilterSlot*   filter_ = nullptr;
    Lfo                    lfo_[2];
    DreamPartialParams     params_;
    double sr_ = 44100.0;
    double baseSemis_ = 0.0;
    float  velGain_ = 1.0f;
    float  lfoLevelGain_ = 1.0f;
    int    note_ = 60, ctrlCount_ = 0;
};

// ---------------------------------------------------------------- voice
class DreamVoice {
public:
    void prepare(double sr, uint32_t voiceIdx) noexcept {
        a_.prepare(sr, voiceIdx * 2u);
        b_.prepare(sr, voiceIdx * 2u + 1u);
    }

    void noteOn(const DreamPatch& patch, int midiNote, float velocity01,
                uint64_t stamp) noexcept {
        note_ = midiNote;
        serial_ = stamp;                       // steal fix: global age stamp
        a_.setInterpMode(patch.interp);
        b_.setInterpMode(patch.interp);
        a_.noteOn(patch.a, midiNote, velocity01);
        b_.noteOn(patch.b, midiNote, velocity01);
    }
    void noteOff() noexcept { a_.noteOff(); b_.noteOff(); }
    void kill() noexcept    { a_.kill(); b_.kill(); }
    bool isActive() const noexcept { return a_.isActive() || b_.isActive(); }
    int  note() const noexcept { return note_; }
    uint64_t serial() const noexcept { return serial_; }

    void updateLive(const DreamPatch& patch) noexcept {
        a_.setInterpMode(patch.interp);
        b_.setInterpMode(patch.interp);
        a_.updateLive(patch.a);
        b_.updateLive(patch.b);
    }
    void setOversample(int f) noexcept { a_.setOversample(f); b_.setOversample(f); }
    void setFilters(rompler::FilterSlot* fa, rompler::FilterSlot* fb) noexcept {
        a_.setFilter(fa); b_.setFilter(fb);    // test seam
    }

    float process(float bendSemis, float driftTuneSemis, float driftCutOct) noexcept {
        return a_.process(bendSemis, driftTuneSemis, driftCutOct)
             + b_.process(bendSemis, driftTuneSemis, driftCutOct);
    }

private:
    DreamPartial a_, b_;
    int note_ = -1;
    uint64_t serial_ = 0;
};

// ---------------------------------------------------------------- poly manager
class DreamSynth {
public:
    static constexpr int kMaxVoices = 24;

    void prepare(double sr) noexcept {
        for (int i = 0; i < kMaxVoices; ++i) {
            voices_[i].prepare(sr, (uint32_t)i);
            sustained_[i] = false;
        }
    }
    DreamPatch& patch() noexcept { return patch_; }

    void updateLive() noexcept {
        for (auto& v : voices_) v.updateLive(patch_);
    }
    void setOversample(int f) noexcept {
        for (auto& v : voices_) v.setOversample(f);
    }
    void setPitchBend(float semis) noexcept { bendSemis_ = semis; }

    void noteOn(int midiNote, float velocity01) noexcept {
        DreamVoice* target = nullptr;
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices_[i].isActive()) { target = &voices_[i]; sustained_[i] = false; break; }
        if (!target) {                          // steal the OLDEST note (global stamp)
            uint64_t best = UINT64_MAX;
            int bestIdx = 0;
            for (int i = 0; i < kMaxVoices; ++i)
                if (voices_[i].serial() < best) { best = voices_[i].serial(); bestIdx = i; }
            target = &voices_[bestIdx];
            sustained_[bestIdx] = false;
        }
        target->noteOn(patch_, midiNote, velocity01, ++stamp_);
    }

    void noteOff(int midiNote) noexcept {
        for (int i = 0; i < kMaxVoices; ++i)
            if (voices_[i].isActive() && voices_[i].note() == midiNote) {
                if (sustainDown_) sustained_[i] = true;
                else              voices_[i].noteOff();
            }
    }

    void setSustain(bool down) noexcept {
        sustainDown_ = down;
        if (!down)
            for (int i = 0; i < kMaxVoices; ++i)
                if (sustained_[i]) { sustained_[i] = false; voices_[i].noteOff(); }
    }

    void allNotesOff() noexcept {
        sustainDown_ = false;
        for (int i = 0; i < kMaxVoices; ++i) { sustained_[i] = false; voices_[i].noteOff(); }
    }

    // Immediate silence (all-sound-off, engine switches, reset): no release tails.
    void killAll() noexcept {
        sustainDown_ = false;
        for (int i = 0; i < kMaxVoices; ++i) { sustained_[i] = false; voices_[i].kill(); }
    }

    float process(float driftTuneSemis, float driftCutOct) noexcept {
        float s = 0.0f;
        for (auto& v : voices_) s += v.process(bendSemis_, driftTuneSemis, driftCutOct);
        return s;
    }

    DreamVoice& voiceForTest(int i) noexcept { return voices_[i]; }

private:
    DreamVoice voices_[kMaxVoices];
    bool       sustained_[kMaxVoices] = {};
    DreamPatch patch_;
    uint64_t   stamp_ = 0;
    float      bendSemis_ = 0.0f;
    bool       sustainDown_ = false;
};

} // namespace dreamer
