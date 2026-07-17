// DreamVoice v2 -- The Dreamer's 4-tone voice engine (FEATURES.md CORE set,
// design handoff 2026-07-18; supersedes the 2-partial v1 engine, git history
// keeps it).
//
// Per voice: 4 Tones (JD-990 model), each:
//   PcmOscillator -> Waveshaper (01/W LUT, section 2) -> ToneSvf TVF
//   (LP24/LP12/BP/HP) -> TVA, with TVF/TVA/AUX ADSR envelopes, a per-tone
//   tap of the single GLOBAL LFO (section 3), per-tone PAN (equal-power into
//   the stereo voice bus), and a Dream Vector position DIR/INT (section 4).
// Tone sum (stereo) -> global filters + FX in the processor.
//
// Dream Vector v4 (per-tone directional morphing):
//   g    = max(0, cos(2pi*(phi - dir)))^2          control rate, per 16
//   gain = level * ((1 - int) + int * g)           one-pole smoothed
//   phi  = manual PHASE + ORBIT ramp + per-voice P-ENV, summed, wrapped.
//
// Mod matrix (section 7): 3 slots SRC -> DEST, bipolar AMT, control rate.
//   SRC:  - / G-LFO / VEC PHS / AUX (voice-max) / VELO / WHEEL
//   DEST: - / PITCH / CUT 1 / CUT 2 / MORPH (reserved, no-op in v1) /
//         SHAPE / VEC PHS / PAN
//   VEC PHS -> VEC PHS self-routing is clamped (slot skipped), per spec.
//
// Control-rate stepping (16 samples) is era-honest hardware behavior -- the
// verified bank cadence -- do NOT smooth it (the vector gain's one-pole is
// part of the spec'd law, not zipper hiding).
//
// Carry-overs from v1 (proven in test_voice): oldest-note steal via global
// stamp (fixes the bank's unassigned-stamp defect), kill/killAll hard stop
// (kill freezes envelope levels by design -- stolen-voice semantics),
// sustain pedal, pitch bend on the control-rate frequency update.
//
// C++17, header-only, JUCE-free, real-time safe after prepare().

#pragma once
#include <cmath>
#include <cstdint>
#include "dsp/bank/Mode2Voice.h"        // rompler::EnvelopeAdsr + PcmOscillator/bank
#include "dsp/glue/Waveshaper.h"
#include "dsp/glue/ToneSvf.h"
#include "dsp/ported/RhinoLfo.h"        // dreamer::Lfo (the global LFO)

namespace dreamer {

// ---------------------------------------------------------------- params
struct ToneParams {
    bool   enabled      = false;
    int    waveIndex    = 0;
    int    coarse       = 0;       // semitones
    double fineCents    = 0.0;
    double level        = 0.8;     // 0..1
    double velSens      = 0.5;     // 0..1
    double startOffset  = 0.0;     // 0..599 (E-MU trick)
    double pan          = 0.0;     // -1..+1

    // Dream Vector position
    double dir          = 0.0;     // 0..1 = 0..360 degrees
    double vecInt       = 0.0;     // 0..1 intensity

    // waveshaper
    int    shapeMode    = 0;       // 0 OFF, 1..5 = ShaperTables order
    double shapeDepth   = 0.0;     // 0..1

    // TVF
    int    tvfMode      = 0;       // ToneSvf::Mode order LP24/LP12/BP/HP
    double cutoffHz     = 8000.0;
    double resonance    = 0.0;
    double tvfEnvAmount = 0.0;     // Hz at env peak (bipolar)
    double tvfKeyFollow = 0.5;
    double tvfA = 0.005, tvfD = 0.3, tvfS = 1.0, tvfR = 0.3;

    // TVA
    double tvaA = 0.005, tvaD = 0.3, tvaS = 1.0, tvaR = 0.3;

    // AUX env (section 3): one ADSR, one destination
    double auxA = 0.005, auxD = 0.3, auxS = 1.0, auxR = 0.3;
    int    auxDest = 0;            // 0 PITCH, 1 START, 2 SHAPE, 3 PAN
    double auxAmt  = 0.0;          // -1..+1 (PITCH scales to +/-24 st)

    // global-LFO tap
    double lfoDepth = 0.0;         // 0..1
    int    lfoDest  = 0;           // 0 PITCH, 1 CUTOFF, 2 SHAPE, 3 LEVEL
};

struct VectorParams {
    double phase      = 0.0;       // manual PHASE knob, 0..1 turns
    bool   orbitOn    = false;
    double orbitRate01 = 20.0;     // 0..100 -> Lfo::rateHzFromParam Hz
    bool   penvOn     = false;
    bool   penvLoop   = false;
    double penvStart  = 0.0;       // 0..1 turns
    double penvEnd    = 0.5;
    double penvTime   = 1.0;       // seconds
};

struct MatrixSlot { int src = 0; int dest = 0; double amt = 0.0; };  // amt -1..+1

struct DreamPatch {
    ToneParams   tone[4];
    int          interp = 1;       // 0 DropSample, 1 Linear
    VectorParams vec;
    int          glfoShape01 = 0;  // panel order TRI/SIN/SAW/SQR/S+H
    double       glfoRate01  = 50.0;
    MatrixSlot   slot[3];
};

// matrix source/dest indices (panel choice order)
namespace mtx {
    enum Src  : int { srcNone = 0, srcGlfo, srcVecPhs, srcAux, srcVelo, srcWheel };
    enum Dest : int { dstNone = 0, dstPitch, dstCut1, dstCut2, dstMorph,
                      dstShape, dstVecPhs, dstPan };
}

// control-rate modulation bundle handed down from the synth
struct ModContext {
    float bendSemis      = 0.0f;
    float driftTuneSemis = 0.0f;
    float driftCutOct    = 0.0f;
    float glfoValue      = 0.0f;   // bipolar
    float phi            = 0.0f;   // global phase part (manual+orbit+matrix), turns
    float mtxPitchSemis  = 0.0f;
    float mtxShapeAdd    = 0.0f;
    float mtxPanAdd      = 0.0f;
};

// ---------------------------------------------------------------- tone
class Tone {
public:
    void prepare(double sr) noexcept {
        sr_ = sr;
        osc_.setSampleRate(sr);
        tvfEnv_.setSampleRate(sr);
        tvaEnv_.setSampleRate(sr);
        auxEnv_.setSampleRate(sr);
        svf_.setSampleRate(sr);
    }

    void setInterpMode(int m) noexcept {
        osc_.setInterp(m == 0 ? rompler::PcmOscillator::Interp::DropSample
                              : rompler::PcmOscillator::Interp::Linear);
    }

    void noteOn(const ToneParams& p, int midiNote, float velocity01) noexcept {
        params_ = p;
        note_ = midiNote;
        osc_.setWaveform(p.waveIndex);
        baseSemis_ = midiNote - 69 + p.coarse + p.fineCents / 100.0;
        // AUX->START: re-strike offset, sampled at note-on from the aux env's
        // residual level (0 on a fresh voice; evolving on steals/retriggers)
        double start = p.startOffset;
        if (p.auxDest == 1)
            start += p.auxAmt * 599.0 * (double)lastAux_;
        start = std::min(std::max(start, 0.0), 599.0);
        osc_.reset(start);
        velGain_ = (float)((1.0 - p.velSens) + p.velSens * velocity01);
        tvfEnv_.set(p.tvfA, p.tvfD, p.tvfS, p.tvfR);
        tvaEnv_.set(p.tvaA, p.tvaD, p.tvaS, p.tvaR);
        auxEnv_.set(p.auxA, p.auxD, p.auxS, p.auxR);
        tvfEnv_.noteOn();
        tvaEnv_.noteOn();
        auxEnv_.noteOn();
        svf_.setMode(p.tvfMode);
        svf_.reset();
        vecGain_ = 0.0f;                 // vector gain fades in via its one-pole
        levelMul_ = 1.0f;
        shapeEff_ = (float)p.shapeDepth;
        ctrlCount_ = 0;
    }

    void noteOff() noexcept { tvfEnv_.noteOff(); tvaEnv_.noteOff(); auxEnv_.noteOff(); }
    void kill() noexcept { noteOff(); params_.enabled = false; }
    bool isActive() const noexcept { return params_.enabled && tvaEnv_.isActive(); }
    float auxLevel() const noexcept { return lastAux_; }

    // continuous fields go live on held notes; waveIndex/start/velSens/env
    // times stay note-on-latched (v1 convention, documented)
    void updateLive(const ToneParams& p) noexcept {
        params_.enabled      = p.enabled;
        params_.level        = p.level;
        params_.pan          = p.pan;
        params_.dir          = p.dir;
        params_.vecInt       = p.vecInt;
        params_.shapeMode    = p.shapeMode;
        params_.shapeDepth   = p.shapeDepth;
        params_.cutoffHz     = p.cutoffHz;
        params_.resonance    = p.resonance;
        params_.tvfEnvAmount = p.tvfEnvAmount;
        params_.tvfKeyFollow = p.tvfKeyFollow;
        params_.coarse       = p.coarse;
        params_.fineCents    = p.fineCents;
        params_.auxDest      = p.auxDest;
        params_.auxAmt       = p.auxAmt;
        params_.lfoDepth     = p.lfoDepth;
        params_.lfoDest      = p.lfoDest;
        baseSemis_ = note_ - 69 + p.coarse + p.fineCents / 100.0;
        if (p.tvfMode != params_.tvfMode) {
            params_.tvfMode = p.tvfMode;
            svf_.setMode(p.tvfMode);
        }
    }

    void process(float& l, float& r, const ModContext& m) noexcept {
        if (!isActive()) { (void)tvfEnv_.process(); lastAux_ = auxEnv_.process(); return; }
        const float tvf = tvfEnv_.process();
        const float tva = tvaEnv_.process();
        lastAux_ = auxEnv_.process();

        if (ctrlCount_-- <= 0) {                        // control rate, per 16
            ctrlCount_ = 15;
            double pitchSemis = baseSemis_ + m.bendSemis + m.driftTuneSemis
                              + m.mtxPitchSemis;
            double cutOct     = m.driftCutOct;
            double shapeEff   = params_.shapeDepth + m.mtxShapeAdd;
            double panEff     = params_.pan + m.mtxPanAdd;
            levelMul_ = 1.0f;

            // global-LFO tap (per-tone depth/dest)
            {
                const float v = m.glfoValue;
                const float d = (float)params_.lfoDepth;
                switch (params_.lfoDest) {
                case 0:  pitchSemis += (double)v * d * d * 12.0; break;
                case 1:  cutOct     += (double)v * d * 2.0;      break;
                case 2:  shapeEff   += (double)v * d;            break;
                default: levelMul_  *= 1.0f - d * 0.5f * (1.0f - v); break;
                }
            }
            // AUX env (PITCH/SHAPE/PAN live; START is note-on-only)
            {
                const double a = (double)lastAux_ * params_.auxAmt;
                switch (params_.auxDest) {
                case 0:  pitchSemis += a * 24.0; break;
                case 2:  shapeEff   += a;        break;
                case 3:  panEff     += a;        break;
                default: break;
                }
            }

            osc_.setFrequency(440.0 * std::pow(2.0, pitchSemis / 12.0));

            const double keyHz = params_.cutoffHz *
                std::pow(2.0, params_.tvfKeyFollow * (note_ - 60) / 12.0);
            double hz = keyHz + params_.tvfEnvAmount * tvf;
            hz *= std::pow(2.0, cutOct);
            hz = std::min(std::max(hz, 20.0), 18000.0);
            svf_.setCutoffRes(hz, params_.resonance);

            // Dream Vector gain law (v4), one-pole smoothed at control rate
            {
                const double d  = m.phi - params_.dir;
                const double c  = std::cos(2.0 * 3.14159265358979323846 * d);
                const double g  = c > 0.0 ? c * c : 0.0;
                const double tg = params_.level *
                    ((1.0 - params_.vecInt) + params_.vecInt * g);
                vecGain_ += kVecSmooth * ((float)tg - vecGain_);
            }
            // equal-power pan
            {
                if (panEff > 1.0) panEff = 1.0; else if (panEff < -1.0) panEff = -1.0;
                const double a = (panEff + 1.0) * 0.25 * 3.14159265358979323846;
                panL_ = (float)std::cos(a);
                panR_ = (float)std::sin(a);
            }
            if (shapeEff < 0.0) shapeEff = 0.0; else if (shapeEff > 1.0) shapeEff = 1.0;
            shapeEff_ = (float)shapeEff;
        }

        float x = osc_.process();
        x = Waveshaper::process(x, params_.shapeMode, shapeEff_);
        x = svf_.process(x);
        const float s = x * tva * velGain_ * vecGain_ * levelMul_;
        l += s * panL_;
        r += s * panR_;
    }

private:
    static constexpr float kVecSmooth = 0.3f;   // one-pole at control rate

    rompler::PcmOscillator osc_;
    rompler::EnvelopeAdsr  tvfEnv_, tvaEnv_, auxEnv_;
    ToneSvf                svf_;
    ToneParams             params_;
    double sr_ = 44100.0;
    double baseSemis_ = 0.0;
    float  velGain_ = 1.0f, vecGain_ = 0.0f, levelMul_ = 1.0f;
    float  shapeEff_ = 0.0f, lastAux_ = 0.0f;
    float  panL_ = 0.70710678f, panR_ = 0.70710678f;
    int    note_ = 60, ctrlCount_ = 0;
};

// ---------------------------------------------------------------- voice
class DreamVoice {
public:
    void prepare(double sr) noexcept {
        sr_ = sr;
        for (auto& t : tones_) t.prepare(sr);
    }

    void noteOn(const DreamPatch& patch, int midiNote, float velocity01,
                uint64_t stamp) noexcept {
        note_ = midiNote;
        serial_ = stamp;
        penvT_ = 0.0;
        for (int i = 0; i < 4; ++i) {
            tones_[i].setInterpMode(patch.interp);
            tones_[i].noteOn(patch.tone[i], midiNote, velocity01);
        }
    }
    void noteOff() noexcept { for (auto& t : tones_) t.noteOff(); }
    void kill() noexcept    { for (auto& t : tones_) t.kill(); }
    bool isActive() const noexcept {
        for (auto& t : tones_) if (t.isActive()) return true;
        return false;
    }
    int  note() const noexcept { return note_; }
    uint64_t serial() const noexcept { return serial_; }

    float auxMax() const noexcept {
        float m = 0.0f;
        for (auto& t : tones_) m = std::fmax(m, t.auxLevel());
        return m;
    }

    void updateLive(const DreamPatch& patch) noexcept {
        for (int i = 0; i < 4; ++i) {
            tones_[i].setInterpMode(patch.interp);
            tones_[i].updateLive(patch.tone[i]);
        }
    }

    // P-ENV value (turns): start -> end over time, optional ping-pong loop
    float penvValue(const VectorParams& v) const noexcept {
        if (!v.penvOn) return 0.0f;
        const double T = std::max(v.penvTime, 0.001);
        double u = penvT_ / T;
        if (v.penvLoop) {
            u = std::fmod(u, 2.0);
            if (u > 1.0) u = 2.0 - u;               // ping-pong
        } else if (u > 1.0) {
            u = 1.0;
        }
        return (float)(v.penvStart + (v.penvEnd - v.penvStart) * u);
    }

    void process(float& l, float& r, const VectorParams& vec, ModContext m) noexcept {
        m.phi += penvValue(vec);
        m.phi -= std::floor(m.phi);                 // wrap to [0,1)
        penvT_ += 1.0 / sr_;
        for (auto& t : tones_) t.process(l, r, m);
    }

private:
    Tone tones_[4];
    double sr_ = 44100.0, penvT_ = 0.0;
    int note_ = -1;
    uint64_t serial_ = 0;
};

// ---------------------------------------------------------------- synth
class DreamSynth {
public:
    static constexpr int kMaxVoices = 24;

    void prepare(double sr) noexcept {
        sr_ = sr;
        for (int i = 0; i < kMaxVoices; ++i) {
            voices_[i].prepare(sr);
            sustained_[i] = false;
        }
        glfo_.prepare(sr, 0x1955B52Fu);             // deterministic (donor idiom)
        orbitPhase_ = 0.0;
        ctrlCount_ = 0;
    }
    DreamPatch& patch() noexcept { return patch_; }

    void updateLive() noexcept {
        // panel order TRI/SIN/SAW/SQR/S+H -> Lfo::Shape {sine,tri,ramp,square,sh}
        static constexpr int shapeMap[5] = { 1, 0, 2, 3, 4 };
        int s = patch_.glfoShape01;
        if (s < 0) s = 0; if (s > 4) s = 4;
        glfo_.setShape(shapeMap[s]);
        glfo_.setRateHz(Lfo::rateHzFromParam((float)patch_.glfoRate01));
        orbitHz_ = Lfo::rateHzFromParam((float)patch_.vec.orbitRate01);
        for (auto& v : voices_) v.updateLive(patch_);
    }

    void setPitchBend(float semis) noexcept { bendSemis_ = semis; }
    void setWheel(float w01) noexcept { wheel_ = w01; }

    void noteOn(int midiNote, float velocity01) noexcept {
        lastVelocity_ = velocity01;
        DreamVoice* target = nullptr;
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices_[i].isActive()) { target = &voices_[i]; sustained_[i] = false; break; }
        if (!target) {                              // steal the OLDEST note
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
    void killAll() noexcept {
        sustainDown_ = false;
        for (int i = 0; i < kMaxVoices; ++i) { sustained_[i] = false; voices_[i].kill(); }
    }

    // matrix cutoff offsets for the processor's global filter slots (octaves)
    float matrixCut1Oct() const noexcept { return mtxCut1Oct_; }
    float matrixCut2Oct() const noexcept { return mtxCut2Oct_; }
    // voice-max AUX env level (control-rate) -- drives the GUI's F1 ENV knob
    float auxMax() const noexcept { return auxMax_; }

    void process(float driftTuneSemis, float driftCutOct, float& l, float& r) noexcept {
        glfo_.tick();
        if (patch_.vec.orbitOn) {
            orbitPhase_ += orbitHz_ / sr_;
            if (orbitPhase_ >= 1.0) orbitPhase_ -= 1.0;
        }

        if (ctrlCount_-- <= 0) {                    // synth-level control rate
            ctrlCount_ = 15;
            updateMatrix();
        }

        ModContext m;
        m.bendSemis      = bendSemis_;
        m.driftTuneSemis = driftTuneSemis;
        m.driftCutOct    = driftCutOct;
        m.glfoValue      = glfo_.value();
        m.phi            = basePhi_;
        m.mtxPitchSemis  = mtxPitchSemis_;
        m.mtxShapeAdd    = mtxShapeAdd_;
        m.mtxPanAdd      = mtxPanAdd_;

        for (auto& v : voices_) v.process(l, r, patch_.vec, m);
    }

    DreamVoice& voiceForTest(int i) noexcept { return voices_[i]; }
    float glfoValueForTest() const noexcept { return glfo_.value(); }

private:
    void updateMatrix() noexcept {
        // base phase (manual + orbit), before matrix VEC PHS pushes
        double phi = patch_.vec.phase + (patch_.vec.orbitOn ? orbitPhase_ : 0.0);

        float auxMax = 0.0f;
        for (auto& v : voices_)
            if (v.isActive()) auxMax = std::fmax(auxMax, v.auxMax());
        auxMax_ = auxMax;

        auto srcValue = [&](int src) -> float {
            switch (src) {
            case mtx::srcGlfo:   return glfo_.value();
            case mtx::srcVecPhs: return (float)(2.0 * (phi - std::floor(phi)) - 1.0);
            case mtx::srcAux:    return auxMax;
            case mtx::srcVelo:   return lastVelocity_;
            case mtx::srcWheel:  return wheel_;
            default:             return 0.0f;
            }
        };

        float pitch = 0, cut1 = 0, cut2 = 0, shape = 0, vecAdd = 0, pan = 0;
        for (const auto& s : patch_.slot) {
            if (s.src == mtx::srcNone || s.dest == mtx::dstNone) continue;
            if (s.src == mtx::srcVecPhs && s.dest == mtx::dstVecPhs) continue; // clamped
            const float v = srcValue(s.src) * (float)s.amt;
            switch (s.dest) {
            case mtx::dstPitch:  pitch  += v * 12.0f; break;   // +/-12 st at full
            case mtx::dstCut1:   cut1   += v * 2.0f;  break;   // +/-2 oct
            case mtx::dstCut2:   cut2   += v * 2.0f;  break;
            case mtx::dstShape:  shape  += v;         break;
            case mtx::dstVecPhs: vecAdd += v * 0.5f;  break;   // +/-half turn
            case mtx::dstPan:    pan    += v;         break;
            case mtx::dstMorph:  break;                        // reserved (V1.1/V2)
            default: break;
            }
        }

        mtxPitchSemis_ = pitch;
        mtxCut1Oct_    = cut1;
        mtxCut2Oct_    = cut2;
        mtxShapeAdd_   = shape;
        mtxPanAdd_     = pan;
        phi += vecAdd;
        basePhi_ = (float)(phi - std::floor(phi));
    }

    DreamVoice voices_[kMaxVoices];
    bool       sustained_[kMaxVoices] = {};
    DreamPatch patch_;
    Lfo        glfo_;
    double     sr_ = 44100.0;
    double     orbitPhase_ = 0.0, orbitHz_ = 0.5;
    uint64_t   stamp_ = 0;
    float      bendSemis_ = 0.0f, wheel_ = 0.0f, lastVelocity_ = 0.8f;
    float      basePhi_ = 0.0f;
    float      auxMax_ = 0.0f;
    float      mtxPitchSemis_ = 0, mtxCut1Oct_ = 0, mtxCut2Oct_ = 0,
               mtxShapeAdd_ = 0, mtxPanAdd_ = 0;
    bool       sustainDown_ = false;
    int        ctrlCount_ = 0;
};

} // namespace dreamer
