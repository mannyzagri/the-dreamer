// DreamVoice v3 -- The Dreamer's 4-tone voice engine.
// v2 (FEATURES.md CORE) extended per DSP_BUILD.md (2026-07-18, which WINS
// over FEATURES.md where they conflict):
//
//   * bank v3 playback via PcmOsc3 (Cycle/Loop/OneShot, START as 0..1
//     fraction, START RANDOM per-tone flag)
//   * per-tone NOISE source (level + color one-pole, 12-bit quantized,
//     mixed PRE-shaper; AUX dest 4 = NOISE, matrix dest NOISE)
//   * tone chain (section 5): osc (+start/random) -> +noise -> waveshaper
//     -> re-quantize 12-bit (ALWAYS) -> ToneSvf TVF -> TVA -> vector gain
//     -> pan -> stereo tone sum
//   * per-voice humanization drift (section 3): S&H random walk ~0.2 Hz,
//     global DRIFT depth 0..1 -> 0..+/-3 cents, one-pole slewed, applied
//     coherently to all tones of the voice
//   * vector ORBIT SHAPE (section 6): SAW/TRI/SIN/SQR/S+H shaping of the
//     raw phase ramp; ORBIT RATE 0.02..8 Hz; optional per-voice free-run
//     (random phase offset per noteOn)
//   * mod matrix dest list grows: PITCH/CUT1/CUT2/MORPH/SHAPE/VECPHS/PAN/
//     NOISE; src list unchanged (GLFO/VECPHS/AUX/VELO/WHEEL)
//
// Tone params are now native v3 fields (start 0..1 etc.) -- the verified
// rompler::PartialParams struct no longer fits the v3 start semantics; the
// verified ADSR (rompler::EnvelopeAdsr) is still reused. Carry-overs from
// v1/v2 (proven): oldest-note steal via global stamp, kill/killAll (kill
// freezes envelope levels by design), sustain, bend, 16-sample control-rate
// stepping (era-honest, do NOT smooth).
//
// C++17, header-only, JUCE-free, real-time safe after prepare().

#pragma once
#include <cmath>
#include <cstdint>
#include "dsp/bank/Mode2Voice.h"        // rompler::EnvelopeAdsr (verified)
#include "dsp/glue/PcmOsc3.h"
#include "dsp/glue/Waveshaper.h"
#include "dsp/glue/ToneSvf.h"
#include "dsp/ported/RhinoLfo.h"

namespace dreamer {

// ---------------------------------------------------------------- params
struct ToneParams {
    bool   enabled      = false;
    int    waveIndex    = 0;       // bank3 index (0..103)
    int    coarse       = 0;       // semitones (processor maps oct*12)
    double fineCents    = 0.0;
    double level        = 0.8;
    double velSens      = 0.5;
    double start01      = 0.0;     // 0..1 fraction (v3 semantics)
    bool   startRandom  = false;   // randomize start at note-on
    double pan          = 0.0;     // -1..+1

    double noise        = 0.0;     // 0..1 level
    double noiseColor   = 0.0;     // 0 white .. 1 dark

    double dir          = 0.0;     // vector angle 0..1
    double vecInt       = 0.0;     // vector intensity 0..1

    // DSP_BUILD s11/12/13 per-tone features (multi-tap voicing, loop mode,
    // hit varispeed). All default to the inert value so a bare init patch is
    // bit-identical to the pre-s11 single-tap forward-loop engine.
    int    voicing      = 0;       // 0 SINGLE, 1 OCT, 2 POWER, 3 DREAMY (s11)
    int    dreamySpread = 0;       // 0 ADD9, 1 MIN7, 2 SUS2 (s11, DREAMY only)
    int    loopMode     = 0;       // 0 FORWARD, 1 PINGPONG (s12, Loop waves)
    int    hitPlay      = 0;       // 0 NORMAL, 1 STRETCH (s13, OneShot waves)
    double hitStretch   = 0.5;     // 0..1 -> 0.25x..4x speed (log, 0.5=1.0x)
    double hitPitchTrim = 0.0;     // -24..+24 semitone re-tune (still varispeed)

    int    shapeMode    = 0;       // 0 OFF, 1..5 ShaperTables order
    double shapeDepth   = 0.0;

    int    tvfMode      = 0;       // LP24/LP12/BP/HP
    double cutoffHz     = 8000.0;
    double resonance    = 0.0;
    double tvfEnvAmount = 0.0;     // Hz at env peak
    double tvfKeyFollow = 0.5;
    double tvfA = 0.005, tvfD = 0.3, tvfS = 1.0, tvfR = 0.3;
    double tvaA = 0.005, tvaD = 0.3, tvaS = 1.0, tvaR = 0.3;

    double auxA = 0.005, auxD = 0.3, auxS = 1.0, auxR = 0.3;
    int    auxDest = 0;            // 0 PITCH, 1 START, 2 SHAPE, 3 PAN, 4 NOISE
    double auxAmt  = 0.0;          // -1..+1 (on-panel since GUI v7)

    // v7: TWO per-tone LFOs, each with its own rate (free Hz or tempo-synced
    // to one of 12 divisions), depth and destination. Supersedes the single
    // global-LFO tap of DSP_BUILD s3 (v7 handoff is newer).
    struct ToneLfo {
        double rate01 = 0.5;       // free: ->Lfo::rateHzFromParam(v*100);
                                   // sync: quantized to division index round(v*11)
        double depth  = 0.0;
        bool   sync   = false;
        int    dest   = 0;         // 0 PITCH, 1 CUTOFF, 2 SHAPE, 3 LEVEL
        int    shape  = 1;         // v11: panel TRI/SIN/SAW/SQR/S+H; default SIN
                                   //  -> Lfo::sine (bit-neutral vs pre-v11 fixed sine)
    };
    ToneLfo lfo1, lfo2;
};

struct VectorParams {
    double phase        = 0.0;     // manual PHASE, 0..1 turns
    bool   orbitOn      = false;
    double orbitRate01  = 0.3;     // 0..1 -> 0.02..8 Hz (exp map)
    int    orbitShape   = 0;       // 0 SAW, 1 TRI, 2 SIN, 3 SQR, 4 S+H
    bool   orbitPerVoice = false;  // per-voice free-run (non-panel)
    bool   penvOn       = false;
    bool   penvLoop     = false;
    double penvStart    = 0.0;
    double penvEnd      = 0.5;
    double penvTime     = 1.0;     // seconds
};

struct MatrixSlot { int src = 0; int dest = 0; double amt = 0.0; };

struct DreamPatch {
    ToneParams   tone[4];
    int          interp = 1;       // 0 DropSample, 1 Linear
    VectorParams vec;
    int          glfoShape01 = 0;  // panel order TRI/SIN/SAW/SQR/S+H
    double       glfoRate01  = 50.0;   // 0..100 -> Lfo::rateHzFromParam
    MatrixSlot   slot[3];
    double       drift = 0.0;      // global humanize depth 0..1
};

namespace mtx {
    enum Src  : int { srcNone = 0, srcGlfo, srcVecPhs, srcAux, srcVelo, srcWheel };
    enum Dest : int { dstNone = 0, dstPitch, dstCut1, dstCut2, dstMorph,
                      dstShape, dstVecPhs, dstPan, dstNoise, dstFxParam };
    // dstFxParam (=9) is the "FX PARAM" matrix dest from DSP_BUILD s9 (which
    // WINS): s9 lists FX PARAM -- NOT hit_stretch -- as the new dest. It is
    // RESERVED/inert here (GUI parity only, like dstMorph) until the GUI gives
    // it a slot/focus target; wiring one fixed FX param is the s9-flagged trap.
}

struct ModContext {
    float bendSemis      = 0.0f;
    float driftSemis     = 0.0f;   // per-voice humanize (filled by DreamVoice)
    float glfoValue      = 0.0f;   // global LFO (matrix source)
    float bpm            = 120.0f; // host tempo (tone-LFO sync divisions)
    float phi            = 0.0f;   // global phase part, turns
    float mtxPitchSemis  = 0.0f;
    float mtxShapeAdd    = 0.0f;
    float mtxPanAdd      = 0.0f;
    float mtxNoiseAdd    = 0.0f;
};

// 12 tempo divisions (GUI v7 order): 4/1 2/1 1/1 1/2 1/2T 1/4 1/4T 1/8 1/8T
// 1/16 1/16T 1/32 -- one LFO cycle per division. Public for tests/GUI parity.
inline double toneLfoDivisionBeats(int idx) noexcept {
    static constexpr double beats[12] = {
        16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.0, 2.0 / 3.0,
        0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    if (idx < 0) idx = 0; if (idx > 11) idx = 11;
    return beats[idx];
}
inline double toneLfoRateHz(const ToneParams::ToneLfo& p, double bpm) noexcept {
    if (!p.sync)
        return (double)Lfo::rateHzFromParam((float)(p.rate01 * 100.0));
    const int idx = (int)(p.rate01 * 11.0 + 0.5);
    return (bpm / 60.0) / toneLfoDivisionBeats(idx);
}

// Panel LFO-shape order (TRI/SIN/SAW/SQR/S+H, shared by per-tone LFOs and the
// global LFO) -> Lfo::Shape enum {sine=0,tri=1,ramp=2,square=3,sh=4}. One map,
// used by both the voice and updateLive() so display and DSP always agree.
inline int panelLfoShapeToLfo(int s) noexcept {
    static constexpr int m[5] = { 1, 0, 2, 3, 4 };
    if (s < 0) s = 0; if (s > 4) s = 4;
    return m[s];
}

// ---------------------------------------------------------------- voicing s11
// Multi-tap oscillator interval table (generated at playback, nothing baked).
// Fills semis[0..ntaps-1] with the ET interval of each tap and returns the tap
// count (1..4). Public for tests/GUI parity.
//   SINGLE -> {0}                 OCT    -> {0,+12}
//   POWER  -> {0,+7,+12}          DREAMY -> root + dreamy_spread:
//     ADD9 {0,+7,+12,+14}  MIN7 {0,+3,+7,+10}  SUS2 {0,+2,+7,+12}
inline int voicingIntervals(int voicing, int dreamySpread, int semis[4]) noexcept {
    switch (voicing) {
    case 1: semis[0]=0; semis[1]=12; semis[2]=0; semis[3]=0; return 2;   // OCT
    case 2: semis[0]=0; semis[1]=7;  semis[2]=12; semis[3]=0; return 3;  // POWER
    case 3: {                                                            // DREAMY
        static constexpr int spread[3][4] = {
            { 0, 7, 12, 14 },   // ADD9
            { 0, 3, 7, 10 },    // MIN7
            { 0, 2, 7, 12 } };  // SUS2
        int s = dreamySpread < 0 ? 0 : (dreamySpread > 2 ? 2 : dreamySpread);
        for (int i = 0; i < 4; ++i) semis[i] = spread[s][i];
        return 4;
    }
    default: semis[0]=0; semis[1]=0; semis[2]=0; semis[3]=0; return 1;   // SINGLE
    }
}
// equal-power sum gain 1/sqrt(ntaps). n==1 returns exactly 1.0f so a SINGLE
// voicing tone stays bit-identical to the pre-s11 single-osc path.
inline float voicingTapGain(int nTaps) noexcept {
    switch (nTaps) {
    case 2:  return 0.70710678118654752f;   // 1/sqrt(2)
    case 3:  return 0.57735026918962576f;   // 1/sqrt(3)
    case 4:  return 0.5f;                    // 1/sqrt(4)
    default: return 1.0f;                    // SINGLE
    }
}

// deterministic per-instance RNG (glue idiom)
struct GlueRng {
    uint32_t s = 1u;
    void seed(uint32_t v) noexcept { s = v | 1u; }
    float bipolar() noexcept {                      // [-1, +1)
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)((int32_t)s / 2147483648.0);
    }
    float unipolar() noexcept { return bipolar() * 0.5f + 0.5f; }
};

// ---------------------------------------------------------------- tone
class Tone {
public:
    // 12-bit re-quantize (low nibble zero), truncate toward zero -- the
    // explicit chain stage from DSP_BUILD s5 (also used by the shaper).
    static float requant12(float x) noexcept {
        int32_t q = (int32_t)(x * 32768.0f);
        if (q > 32767) q = 32767; else if (q < -32768) q = -32768;
        q = (q / 16) * 16;
        return (float)q * (1.0f / 32768.0f);
    }

    void prepare(double sr, uint32_t seedBase) noexcept {
        sr_ = sr;
        for (auto& o : osc_) o.setSampleRate(sr);      // s11: up to 4 taps
        tvfEnv_.setSampleRate(sr);
        tvaEnv_.setSampleRate(sr);
        auxEnv_.setSampleRate(sr);
        svf_.setSampleRate(sr);
        rng_.seed(0x9E3779B9u * (seedBase + 1u));
        lfo_[0].prepare(sr, (0xA5A5A5A5u ^ (seedBase * 2u)) | 1u);
        lfo_[1].prepare(sr, (0x5A5A5A5Au ^ (seedBase * 2u + 1u)) | 1u);
    }

    void setInterpMode(int m) noexcept {
        const auto im = (m == 0 ? PcmOsc3::Interp::DropSample
                                : PcmOsc3::Interp::Linear);
        for (auto& o : osc_) o.setInterp(im);          // s11: all taps
    }

    void noteOn(const ToneParams& p, int midiNote, float velocity01) noexcept {
        params_ = p;
        note_ = midiNote;
        // s11/12/13: cache wave type + baked root for the varispeed / loop-mode
        // decision, and configure all 4 taps over the SAME buffer.
        {
            const auto& wf = rompler::bank3::kWaveforms[
                (p.waveIndex < 0 || p.waveIndex >= rompler::bank3::kNumWaveforms)
                    ? 0 : (size_t)p.waveIndex];
            waveType_ = wf.type;
            rootHz_   = wf.rootHz > 0.0f ? (double)wf.rootHz : 220.0;
        }
        nTaps_   = voicingIntervals(p.voicing, p.dreamySpread, intervalSemis_);
        tapGain_ = voicingTapGain(nTaps_);
        const bool pingpong = (p.loopMode == 1);
        for (auto& o : osc_) { o.setWaveform(p.waveIndex); o.setLoopMode(pingpong); }
        baseSemis_ = midiNote - 69 + p.coarse + p.fineCents / 100.0;
        double start = p.start01;
        if (p.startRandom) start = (double)rng_.unipolar();       // section 2
        if (p.auxDest == 1)                                       // AUX->START
            start += p.auxAmt * (double)lastAux_;
        if (start < 0.0) start = 0.0; else if (start > 1.0) start = 1.0;
        for (auto& o : osc_) o.reset(start);           // taps inherit START/RANDOM
        velGain_ = (float)((1.0 - p.velSens) + p.velSens * velocity01);
        tvfEnv_.set(p.tvfA, p.tvfD, p.tvfS, p.tvfR);
        tvaEnv_.set(p.tvaA, p.tvaD, p.tvaS, p.tvaR);
        auxEnv_.set(p.auxA, p.auxD, p.auxS, p.auxR);
        tvfEnv_.noteOn();
        tvaEnv_.noteOn();
        auxEnv_.noteOn();
        svf_.setMode(p.tvfMode);
        svf_.reset();
        noiseLp_ = 0.0f;
        vecGain_ = 0.0f;
        levelMul_ = 1.0f;
        shapeEff_ = (float)p.shapeDepth;
        noiseEff_ = (float)p.noise;
        ctrlCount_ = 0;
    }

    void noteOff() noexcept { tvfEnv_.noteOff(); tvaEnv_.noteOff(); auxEnv_.noteOff(); }
    void kill() noexcept { noteOff(); params_.enabled = false; }
    bool isActive() const noexcept { return params_.enabled && tvaEnv_.isActive(); }
    float auxLevel() const noexcept { return lastAux_; }

    void updateLive(const ToneParams& p) noexcept {
        params_.enabled      = p.enabled;
        params_.level        = p.level;
        params_.pan          = p.pan;
        params_.dir          = p.dir;
        params_.vecInt       = p.vecInt;
        params_.shapeMode    = p.shapeMode;
        params_.shapeDepth   = p.shapeDepth;
        params_.noise        = p.noise;
        params_.noiseColor   = p.noiseColor;
        params_.cutoffHz     = p.cutoffHz;
        params_.resonance    = p.resonance;
        params_.tvfEnvAmount = p.tvfEnvAmount;
        params_.tvfKeyFollow = p.tvfKeyFollow;
        params_.coarse       = p.coarse;
        params_.fineCents    = p.fineCents;
        params_.auxDest      = p.auxDest;
        params_.auxAmt       = p.auxAmt;
        params_.lfo1         = p.lfo1;
        params_.lfo2         = p.lfo2;
        params_.startRandom  = p.startRandom;
        // s11/13: live voicing + hit params (read each control block); s12 loop
        // mode is stateful in the oscillator, so push it on change.
        params_.voicing      = p.voicing;
        params_.dreamySpread = p.dreamySpread;
        params_.hitPlay      = p.hitPlay;
        params_.hitStretch   = p.hitStretch;
        params_.hitPitchTrim = p.hitPitchTrim;
        if (p.loopMode != params_.loopMode) {
            params_.loopMode = p.loopMode;
            const bool pingpong = (p.loopMode == 1);
            for (auto& o : osc_) o.setLoopMode(pingpong);
        }
        baseSemis_ = note_ - 69 + p.coarse + p.fineCents / 100.0;
        if (p.tvfMode != params_.tvfMode) {
            params_.tvfMode = p.tvfMode;
            svf_.setMode(p.tvfMode);
        }
        // D2 (felt bug): envelopes must read their A/D/S/R CONTINUOUSLY, not
        // latch them at note-on. Push the live rates into all three envelopes
        // every control block. EnvelopeAdsr::set() only recomputes per-sample
        // increments (pure arithmetic, RT-safe) -- it does NOT restart a stage,
        // so a currently-releasing voice picks up a shorter release immediately,
        // and attack/decay edits affect voices in those stages. (set() with the
        // same values is idempotent, so a static patch stays bit-identical.)
        params_.tvfA = p.tvfA; params_.tvfD = p.tvfD; params_.tvfS = p.tvfS; params_.tvfR = p.tvfR;
        params_.tvaA = p.tvaA; params_.tvaD = p.tvaD; params_.tvaS = p.tvaS; params_.tvaR = p.tvaR;
        params_.auxA = p.auxA; params_.auxD = p.auxD; params_.auxS = p.auxS; params_.auxR = p.auxR;
        tvfEnv_.set(p.tvfA, p.tvfD, p.tvfS, p.tvfR);
        tvaEnv_.set(p.tvaA, p.tvaD, p.tvaS, p.tvaR);
        auxEnv_.set(p.auxA, p.auxD, p.auxS, p.auxR);
    }

    void process(float& l, float& r, const ModContext& m) noexcept {
        lfo_[0].tick();                               // per-sample (Lfo contract);
        lfo_[1].tick();                               // idle ticking keeps free-run honest
        if (!isActive()) { (void)tvfEnv_.process(); lastAux_ = auxEnv_.process(); return; }
        const float tvf = tvfEnv_.process();
        const float tva = tvaEnv_.process();
        lastAux_ = auxEnv_.process();

        if (ctrlCount_-- <= 0) {                      // control rate, per 16
            ctrlCount_ = 15;
            double pitchSemis = baseSemis_ + m.bendSemis + m.driftSemis
                              + m.mtxPitchSemis;
            double cutOct   = 0.0;
            double shapeEff = params_.shapeDepth + m.mtxShapeAdd;
            double noiseEff = params_.noise + m.mtxNoiseAdd;
            double panEff   = params_.pan + m.mtxPanAdd;
            levelMul_ = 1.0f;

            // v7: two per-tone LFOs (rate free/synced, per-dest depth laws)
            for (int i = 0; i < 2; ++i) {
                const auto& lp = i == 0 ? params_.lfo1 : params_.lfo2;
                lfo_[i].setRateHz((float)toneLfoRateHz(lp, (double)m.bpm));
                lfo_[i].setShape(panelLfoShapeToLfo(lp.shape));
                const float v = lfo_[i].value();
                const float d = (float)lp.depth;
                if (d <= 0.0f) continue;
                switch (lp.dest) {
                case 0:  pitchSemis += (double)v * d * d * 12.0; break;
                case 1:  cutOct     += (double)v * d * 2.0;      break;
                case 2:  shapeEff   += (double)v * d;            break;
                default: levelMul_  *= 1.0f - d * 0.5f * (1.0f - v); break;
                }
            }
            {   // AUX env (START is note-on-only)
                const double a = (double)lastAux_ * params_.auxAmt;
                switch (params_.auxDest) {
                case 0:  pitchSemis += a * 24.0; break;
                case 2:  shapeEff   += a;        break;
                case 3:  panEff     += a;        break;
                case 4:  noiseEff   += a;        break;
                default: break;
                }
            }

            // ---- s11/s13 multi-tap oscillator control ----------------------
            // Recompute the tap table live (voicing/spread are host params).
            nTaps_   = voicingIntervals(params_.voicing, params_.dreamySpread,
                                        intervalSemis_);
            tapGain_ = voicingTapGain(nTaps_);
            // s13 HIT STRETCH (OneShot only): varispeed replaces note tracking.
            // hit_stretch 0..1 -> speed 0.25x..4x (log, 0.5=1.0x); pitch FOLLOWS
            // speed; hit_pitchtrim re-tunes (still varispeed). setSpeedMul feeds
            // the Loop/OneShot phase increment; Cycle path is never touched.
            const bool stretch =
                (waveType_ == rompler::bank3::WaveType::OneShot) && (params_.hitPlay == 1);
            double baseFreq, speedMul;
            if (stretch) {
                double hs = params_.hitStretch;
                if (hs < 0.0) hs = 0.0; else if (hs > 1.0) hs = 1.0;
                const double speed = 0.25 * std::pow(2.0, 4.0 * hs);        // 0.25..4x
                const double trim  = std::pow(2.0, params_.hitPitchTrim / 12.0);
                baseFreq = rootHz_;                    // native pitch, note-detached
                speedMul = speed * trim;
            } else {
                baseFreq = 440.0 * std::pow(2.0, pitchSemis / 12.0);        // note-tracked
                speedMul = 1.0;
            }
            for (int k = 0; k < 4; ++k) {
                const double r = std::pow(2.0, intervalSemis_[k] / 12.0);   // ET detune
                osc_[k].setFrequency(baseFreq * r);
                osc_[k].setSpeedMul(speedMul);
            }

            const double keyHz = params_.cutoffHz *
                std::pow(2.0, params_.tvfKeyFollow * (note_ - 60) / 12.0);
            double hz = keyHz + params_.tvfEnvAmount * tvf;
            hz *= std::pow(2.0, cutOct);
            hz = std::min(std::max(hz, 20.0), 18000.0);
            svf_.setCutoffRes(hz, params_.resonance);

            {   // Dream Vector gain law, one-pole smoothed at control rate
                const double d  = m.phi - params_.dir;
                const double c  = std::cos(2.0 * 3.14159265358979323846 * d);
                const double g  = c > 0.0 ? c * c : 0.0;
                const double tg = params_.level *
                    ((1.0 - params_.vecInt) + params_.vecInt * g);
                vecGain_ += kVecSmooth * ((float)tg - vecGain_);
            }
            {   // equal-power pan
                if (panEff > 1.0) panEff = 1.0; else if (panEff < -1.0) panEff = -1.0;
                const double a = (panEff + 1.0) * 0.25 * 3.14159265358979323846;
                panL_ = (float)std::cos(a);
                panR_ = (float)std::sin(a);
            }
            if (shapeEff < 0.0) shapeEff = 0.0; else if (shapeEff > 1.0) shapeEff = 1.0;
            shapeEff_ = (float)shapeEff;
            if (noiseEff < 0.0) noiseEff = 0.0; else if (noiseEff > 1.0) noiseEff = 1.0;
            noiseEff_ = (float)noiseEff;
            noiseCoef_ = 1.0f - 0.98f * (float)params_.noiseColor;   // 0=white 1=dark
        }

        // ---- chain (DSP_BUILD s5) --------------------------------------
        // s11: voicing fans ONLY the oscillator -- sum up to 4 taps at
        // equal-power gain, then feed as ONE tone signal into the shared
        // shaper/TVF/TVA/vector/pan chain. nTaps_==1 -> x == osc_[0].process()
        // exactly (tapGain_ == 1.0f), so a SINGLE tone stays bit-identical.
        float x = 0.0f;
        for (int k = 0; k < nTaps_; ++k) x += osc_[k].process();
        x *= tapGain_;
        if (noiseEff_ > 0.0f) {
            noiseLp_ += noiseCoef_ * (rng_.bipolar() - noiseLp_);
            x += noiseEff_ * requant12(noiseLp_ * 0.9995f);   // 12-bit noise
        }
        x = Waveshaper::process(x, params_.shapeMode, shapeEff_);
        x = requant12(x);                                      // explicit stage
        x = svf_.process(x);
        const float s = x * tva * velGain_ * vecGain_ * levelMul_;
        l += s * panL_;
        r += s * panR_;
    }

private:
    static constexpr float kVecSmooth = 0.3f;

    PcmOsc3                osc_[4];      // s11 multi-tap voicing (root + detuned)
    rompler::EnvelopeAdsr  tvfEnv_, tvaEnv_, auxEnv_;
    ToneSvf                svf_;
    Lfo                    lfo_[2];      // v7 per-tone LFO pair (v11: per-LFO shape)
    ToneParams             params_;
    GlueRng                rng_;
    double sr_ = 44100.0;
    double baseSemis_ = 0.0;
    float  velGain_ = 1.0f, vecGain_ = 0.0f, levelMul_ = 1.0f;
    float  shapeEff_ = 0.0f, noiseEff_ = 0.0f, noiseCoef_ = 1.0f, noiseLp_ = 0.0f;
    float  lastAux_ = 0.0f;
    float  panL_ = 0.70710678f, panR_ = 0.70710678f;
    int    note_ = 60, ctrlCount_ = 0;
    // s11/12/13 tap state
    int    intervalSemis_[4] = { 0, 0, 0, 0 };
    int    nTaps_    = 1;
    float  tapGain_  = 1.0f;
    rompler::bank3::WaveType waveType_ = rompler::bank3::WaveType::Cycle;
    double rootHz_   = 220.0;
};

// ---------------------------------------------------------------- voice
class DreamVoice {
public:
    void prepare(double sr, uint32_t voiceIdx) noexcept {
        sr_ = sr;
        for (uint32_t i = 0; i < 4; ++i) tones_[i].prepare(sr, voiceIdx * 4u + i);
        walkRng_.seed(0x51D5EEDu + voiceIdx * 0x9E3779B9u);
        orbitRng_.seed(0xB16B00B5u + voiceIdx * 0x9E3779B9u);
        walk_ = walkTarget_ = 0.0f;
        walkCount_ = 0;
        // GAIN_STAGING s1: tone-sum normalization one-pole (~10 ms) so the
        // 1/sqrt(nEnabledTones) scale glides on preset load / tone toggle.
        toneNormCoef_  = 1.0f - std::exp(-1.0f / (float)(sr * kToneNormTau));
        toneNormScale_ = 1.0f;
    }

    // GAIN_STAGING s1: equal-power tone-sum trim = 1/sqrt(nEnabledTones), so a
    // 4-tone patch sits near unity instead of ~3x. NEVER a per-voice/polyphony
    // scale (a chord must not duck). n==1 -> 1.0f so a single-tone voice stays
    // bit-identical to the pre-fix path.
    static float toneNormTarget(const DreamPatch& patch) noexcept {
        int n = 0;
        for (int i = 0; i < 4; ++i) if (patch.tone[i].enabled) ++n;
        static constexpr float k[5] = {
            1.0f, 1.0f, 0.70710678118654752f, 0.57735026918962576f, 0.5f };
        return k[n];
    }

    void noteOn(const DreamPatch& patch, int midiNote, float velocity01,
                uint64_t stamp) noexcept {
        note_ = midiNote;
        serial_ = stamp;
        penvT_ = 0.0;
        voicePhiOffset_ = patch.vec.orbitPerVoice ? orbitRng_.unipolar() : 0.0f;
        // GAIN_STAGING s1: snap the tone-sum scale to the current enabled count on
        // a fresh note (no fade-in); the one-pole only glides live toggles.
        toneNormScale_ = toneNormTarget(patch);
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
    float driftSemisForTest() const noexcept { return driftSemis_; }

    void updateLive(const DreamPatch& patch) noexcept {
        for (int i = 0; i < 4; ++i) {
            tones_[i].setInterpMode(patch.interp);
            tones_[i].updateLive(patch.tone[i]);
        }
    }

    float penvValue(const VectorParams& v) const noexcept {
        if (!v.penvOn) return 0.0f;
        const double T = std::max(v.penvTime, 0.001);
        double u = penvT_ / T;
        if (v.penvLoop) {
            u = std::fmod(u, 2.0);
            if (u > 1.0) u = 2.0 - u;
        } else if (u > 1.0) {
            u = 1.0;
        }
        return (float)(v.penvStart + (v.penvEnd - v.penvStart) * u);
    }

    void process(float& l, float& r, const DreamPatch& patch, ModContext m) noexcept {
        // humanize drift (section 3): S&H random walk ~0.2 Hz, one-pole slew,
        // coherent for all 4 tones of this voice
        if (walkCount_-- <= 0) {
            walkCount_ = (int)(sr_ * 5.0);                 // ~0.2 Hz steps
            walkTarget_ += walkRng_.bipolar();
            if (walkTarget_ >  1.0f) walkTarget_ =  1.0f;
            if (walkTarget_ < -1.0f) walkTarget_ = -1.0f;
        }
        walk_ += 2.0e-5f * (walkTarget_ - walk_);          // slow slew
        driftSemis_ = walk_ * (float)patch.drift * 0.03f;  // +/-3 cents max
        m.driftSemis = driftSemis_;

        m.phi += penvValue(patch.vec) + voicePhiOffset_;
        m.phi -= std::floor(m.phi);
        penvT_ += 1.0 / sr_;

        // GAIN_STAGING s1: sum THIS voice's tones locally, apply the smoothed
        // 1/sqrt(nEnabledTones) trim, then add to the shared bus. For a single
        // enabled tone the target is 1.0 and the scale stays 1.0, so lv==tone
        // output and l += 1.0f*lv is bit-identical to the pre-fix direct sum.
        const float target = toneNormTarget(patch);
        toneNormScale_ += toneNormCoef_ * (target - toneNormScale_);
        float lv = 0.0f, rv = 0.0f;
        for (auto& t : tones_) t.process(lv, rv, m);
        l += toneNormScale_ * lv;
        r += toneNormScale_ * rv;
    }

private:
    static constexpr double kToneNormTau = 0.010;   // s, tone-sum scale one-pole

    Tone tones_[4];
    GlueRng walkRng_, orbitRng_;
    double sr_ = 44100.0, penvT_ = 0.0;
    float  walk_ = 0.0f, walkTarget_ = 0.0f, driftSemis_ = 0.0f;
    float  voicePhiOffset_ = 0.0f;
    float  toneNormScale_ = 1.0f, toneNormCoef_ = 1.0f;   // GAIN_STAGING s1
    int    walkCount_ = 0;
    int note_ = -1;
    uint64_t serial_ = 0;
};

// ---------------------------------------------------------------- synth
class DreamSynth {
public:
    static constexpr int kMaxVoices = 24;

    // ORBIT SHAPE (section 6): shape the raw 0..1 ramp into the phase
    // contribution. Public static for tests.
    static float orbitShapeFn(int shape, float raw, float shValue) noexcept {
        switch (shape) {
        case 1:  return 1.0f - std::fabs(2.0f * raw - 1.0f);            // TRI
        case 2:  return 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * raw); // SIN
        case 3:  return raw < 0.5f ? 0.0f : 0.5f;                       // SQR
        case 4:  return shValue;                                        // S+H (slewed)
        default: return raw;                                            // SAW
        }
    }
    static double orbitRateHz(double v01) noexcept {                    // 0.02..8 Hz
        if (v01 < 0.0) v01 = 0.0; else if (v01 > 1.0) v01 = 1.0;
        return 0.02 * std::pow(400.0, v01);
    }

    void prepare(double sr) noexcept {
        sr_ = sr;
        for (int i = 0; i < kMaxVoices; ++i) {
            voices_[i].prepare(sr, (uint32_t)i);
            sustained_[i] = false;
        }
        glfo_.prepare(sr, 0x1955B52Fu);
        shRng_.seed(0x5EEDBA5Eu);
        orbitPhase_ = 0.0;
        shHeld_ = shSlewed_ = 0.0f;
        ctrlCount_ = 0;
    }
    DreamPatch& patch() noexcept { return patch_; }

    void updateLive() noexcept {
        glfo_.setShape(panelLfoShapeToLfo(patch_.glfoShape01));
        glfo_.setRateHz(Lfo::rateHzFromParam((float)patch_.glfoRate01));
        orbitHz_ = orbitRateHz(patch_.vec.orbitRate01);
        for (auto& v : voices_) v.updateLive(patch_);
    }

    void setPitchBend(float semis) noexcept { bendSemis_ = semis; }
    void setWheel(float w01) noexcept { wheel_ = w01; }
    void setBpm(float bpm) noexcept { bpm_ = bpm > 1.0f ? bpm : 120.0f; }

    void noteOn(int midiNote, float velocity01) noexcept {
        lastVelocity_ = velocity01;
        DreamVoice* target = nullptr;
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices_[i].isActive()) { target = &voices_[i]; sustained_[i] = false; break; }
        if (!target) {
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

    float matrixCut1Oct() const noexcept { return mtxCut1Oct_; }
    float matrixCut2Oct() const noexcept { return mtxCut2Oct_; }
    float auxMax() const noexcept { return auxMax_; }

    void process(float& l, float& r) noexcept {
        glfo_.tick();
        if (patch_.vec.orbitOn) {
            orbitPhase_ += orbitHz_ / sr_;
            if (orbitPhase_ >= 1.0) {
                orbitPhase_ -= 1.0;
                shHeld_ = shRng_.unipolar();               // S+H: new jump target
            }
            shSlewed_ += 0.002f * (shHeld_ - shSlewed_);   // slewed random jumps
        }

        if (ctrlCount_-- <= 0) {
            ctrlCount_ = 15;
            updateMatrix();
        }

        ModContext m;
        m.bendSemis     = bendSemis_;
        m.glfoValue     = glfo_.value();
        m.bpm           = bpm_;
        m.phi           = basePhi_;
        m.mtxPitchSemis = mtxPitchSemis_;
        m.mtxShapeAdd   = mtxShapeAdd_;
        m.mtxPanAdd     = mtxPanAdd_;
        m.mtxNoiseAdd   = mtxNoiseAdd_;

        for (auto& v : voices_) v.process(l, r, patch_, m);
    }

    DreamVoice& voiceForTest(int i) noexcept { return voices_[i]; }

private:
    void updateMatrix() noexcept {
        double phi = patch_.vec.phase;
        if (patch_.vec.orbitOn)
            phi += orbitShapeFn(patch_.vec.orbitShape, (float)orbitPhase_, shSlewed_);

        float am = 0.0f;
        for (auto& v : voices_)
            if (v.isActive()) am = std::fmax(am, v.auxMax());
        auxMax_ = am;

        auto srcValue = [&](int src) -> float {
            switch (src) {
            case mtx::srcGlfo:   return glfo_.value();
            case mtx::srcVecPhs: return (float)(2.0 * (phi - std::floor(phi)) - 1.0);
            case mtx::srcAux:    return auxMax_;
            case mtx::srcVelo:   return lastVelocity_;
            case mtx::srcWheel:  return wheel_;
            default:             return 0.0f;
            }
        };

        float pitch = 0, cut1 = 0, cut2 = 0, shape = 0, vecAdd = 0, pan = 0, noise = 0;
        for (const auto& s : patch_.slot) {
            if (s.src == mtx::srcNone || s.dest == mtx::dstNone) continue;
            if (s.src == mtx::srcVecPhs && s.dest == mtx::dstVecPhs) continue;
            const float v = srcValue(s.src) * (float)s.amt;
            switch (s.dest) {
            case mtx::dstPitch:  pitch  += v * 12.0f; break;
            case mtx::dstCut1:   cut1   += v * 2.0f;  break;
            case mtx::dstCut2:   cut2   += v * 2.0f;  break;
            case mtx::dstShape:  shape  += v;         break;
            case mtx::dstVecPhs: vecAdd += v * 0.5f;  break;
            case mtx::dstPan:    pan    += v;         break;
            case mtx::dstNoise:  noise  += v;         break;
            case mtx::dstMorph:  break;               // reserved (V1.1/V2)
            case mtx::dstFxParam: break;              // s9 FX PARAM: reserved/inert
                                                      //  (needs GUI slot/focus target)
            default: break;
            }
        }

        mtxPitchSemis_ = pitch;
        mtxCut1Oct_    = cut1;
        mtxCut2Oct_    = cut2;
        mtxShapeAdd_   = shape;
        mtxPanAdd_     = pan;
        mtxNoiseAdd_   = noise;
        phi += vecAdd;
        basePhi_ = (float)(phi - std::floor(phi));
    }

    DreamVoice voices_[kMaxVoices];
    bool       sustained_[kMaxVoices] = {};
    DreamPatch patch_;
    Lfo        glfo_;
    GlueRng    shRng_;
    double     sr_ = 44100.0;
    double     orbitPhase_ = 0.0, orbitHz_ = 0.5;
    uint64_t   stamp_ = 0;
    float      bendSemis_ = 0.0f, wheel_ = 0.0f, lastVelocity_ = 0.8f;
    float      bpm_ = 120.0f;
    float      basePhi_ = 0.0f, auxMax_ = 0.0f;
    float      shHeld_ = 0.0f, shSlewed_ = 0.0f;
    float      mtxPitchSemis_ = 0, mtxCut1Oct_ = 0, mtxCut2Oct_ = 0,
               mtxShapeAdd_ = 0, mtxPanAdd_ = 0, mtxNoiseAdd_ = 0;
    bool       sustainDown_ = false;
    int        ctrlCount_ = 0;
};

} // namespace dreamer
