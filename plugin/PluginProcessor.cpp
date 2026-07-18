#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

using namespace dreamer::params;

namespace {
// ---- normalized 0..1 -> unit maps (documented contract addenda) -----------
inline double cutHz(double v)   { return 60.0 * std::pow(200.0, v); }        // 60..12000
inline double envHz(double v)   { return v * 9600.0; }                       // unipolar (s9)
inline double adsrSec(double v) { return 0.001 + 8.0 * v * v * v; }          // 1ms..8s
inline double dlyMs(double v)   { return std::pow(1000.0, v); }              // 1..1000
inline double penvSec(double v) { return 0.02 * std::pow(500.0, v); }        // 0.02..10
}

//==============================================================================
TheDreamerProcessor::TheDreamerProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int t = 0; t < 4; ++t) cacheTonePtrs(pTone[t], t);

    auto p = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    pMaster = p(kMaster);
    pVecPhase = p(kVecPhase); pVecOrbitOn = p(kVecOrbitOn);
    pVecOrbitRate = p(kVecOrbitRate); pVecOrbitShape = p(kVecOrbitShape);
    pVecOrbitVoice = p(kVecOrbitVoice);
    pVecPenvOn = p(kVecPenvOn); pVecPenvStart = p(kVecPenvStart);
    pVecPenvEnd = p(kVecPenvEnd); pVecPenvTime = p(kVecPenvTime);
    pVecPenvLoop = p(kVecPenvLoop);
    pFltRoute = p(kFltRoute); pFltBal = p(kFltBal);
    pFlt1Type = p(kFlt1Type); pFlt1Cut = p(kFlt1Cut);
    pFlt1Res = p(kFlt1Res); pFlt1Env = p(kFlt1Env);
    pFlt2Type = p(kFlt2Type); pFlt2Cut = p(kFlt2Cut);
    pFlt2Res = p(kFlt2Res); pFlt2Morph = p(kFlt2Morph);
    pLfoRate = p(kLfoRate); pLfoShape = p(kLfoShape);
    for (int i = 0; i < 3; ++i) {
        const juce::String n(i + 1);
        pMtxSrc[i] = p("mtx" + n + "_src");
        pMtxDst[i] = p("mtx" + n + "_dst");
        pMtxAmt[i] = p("mtx" + n + "_amt");
    }
    pModfxType = p(kModfxType); pModfxRate = p(kModfxRate);
    pModfxDepth = p(kModfxDepth); pModfxMix = p(kModfxMix); pModfxOn = p(kModfxOn);
    pDlyMode = p(kDlyMode); pDlyTime = p(kDlyTime);
    pDlyFb = p(kDlyFb); pDlyMix = p(kDlyMix); pDlyOn = p(kDlyOn); pDlySync = p(kDlySync);
    pRevType = p(kRevType); pRevSize = p(kRevSize);
    pRevDamp = p(kRevDamp); pRevMix = p(kRevMix); pRevOn = p(kRevOn);
    for (int i = 0; i < 4; ++i) {
        const juce::String n(i);
        pModfxP[i] = p("modfx_p" + n);
        pDlyP[i]   = p("dly_p" + n);
        pRevP[i]   = p("rev_p" + n);
    }
    pLofiOn = p(kLofiOn); pLofiBits = p(kLofiBits); pLofiSrate = p(kLofiSrate);
    pLofiCompand = p(kLofiCompand); pLofiAlias = p(kLofiAlias);
    pWidthOn = p(kWidthOn); pWidth = p(kWidth); pWidthHaas = p(kWidthHaas);
    pWidthBassMono = p(kWidthBassMono);
    pTalkOn = p(kTalkOn); pTalkVa = p(kTalkVa); pTalkVb = p(kTalkVb);
    pTalkMorph = p(kTalkMorph); pTalkSens = p(kTalkSens);
    pFxPrePost = p(kFxPrePost);
    pDrift = p(kDrift); pInterp = p(kInterp); pEngine = p(kEngine);
}

void TheDreamerProcessor::cacheTonePtrs(TonePtrs& dst, int t)
{
    auto p = [&](const char* base) { return apvts.getRawParameterValue(tid(base, t)); };
    dst.wave = p("wave"); dst.on = p("on"); dst.level = p("level");
    dst.oct = p("oct"); dst.fine = p("fine");
    dst.start = p("start"); dst.startRandom = p("start_random");
    dst.velo = p("velo"); dst.pan = p("pan");
    dst.shape = p("shape"); dst.shapeDepth = p("shape_depth");
    dst.noise = p("noise"); dst.noiseColor = p("noise_color");
    dst.dir = p("dir"); dst.vint = p("vint");
    for (int lf = 0; lf < 2; ++lf) {
        const juce::String lb = "lfo" + juce::String(lf + 1) + "_";
        auto q = [&](const char* s) {
            return apvts.getRawParameterValue(tid((lb + s).toRawUTF8(), t));
        };
        dst.lfo[lf].rate = q("rate"); dst.lfo[lf].depth = q("depth");
        dst.lfo[lf].sync = q("sync"); dst.lfo[lf].dest = q("dest");
        dst.lfo[lf].shape = q("shape");
    }
    dst.auxDest = p("aux_dest"); dst.auxAmt = p("aux_amt");
    dst.tvfType = p("tvf_type"); dst.tvfCut = p("tvf_cut");
    dst.tvfRes = p("tvf_res"); dst.tvfEnv = p("tvf_env"); dst.tvfKf = p("tvf_kf");
    dst.tvfA = p("tvf_a"); dst.tvfD = p("tvf_d"); dst.tvfS = p("tvf_s"); dst.tvfR = p("tvf_r");
    dst.tvaA = p("tva_a"); dst.tvaD = p("tva_d"); dst.tvaS = p("tva_s"); dst.tvaR = p("tva_r");
    dst.auxA = p("aux_a"); dst.auxD = p("aux_d"); dst.auxS = p("aux_s"); dst.auxR = p("aux_r");
}

//==============================================================================
void TheDreamerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.prepare(sampleRate);
    synth.killAll();
    lastOversample = 0;
    guiFifo.reset();                // discard any GUI MIDI queued before prepare

    for (int ch = 0; ch < 2; ++ch) {
        f1[ch].setSampleRate(sampleRate);
        f2[ch].setSampleRate(sampleRate);
        dcBlock[ch].prepare(sampleRate);
    }
    gfCtrl = 0;

    ensemble.prepare(sampleRate);
    dimension.prepare(sampleRate);
    rotary.prepare(sampleRate);
    barberpole.prepare(sampleRate);
    delay.prepare(sampleRate);
    chorus.prepare(sampleRate, 6.0f);
    flanger.prepare(sampleRate, 3.0f);
    phaser.prepare(sampleRate);
    reverb.setSampleRate(sampleRate);
    reverb.reset();
    lofi.prepare(sampleRate);
    stereoWidth.prepare(sampleRate);
    talkbox.prepare(sampleRate);
    reverbWasActive = false;
    revCache = {};

    reverbWetL.assign((size_t)juce::jmax(1, samplesPerBlock), 0.0f);
    reverbWetR.assign((size_t)juce::jmax(1, samplesPerBlock), 0.0f);

    // per-block one-pole for FX extra params (~20 ms). Recomputed exactly
    // once per block from the actual numSamples in processBlock.
    fxSmoothCoef_ = 1.0f;
    for (int s = 0; s < 3; ++s) for (int i = 0; i < 4; ++i) fxP_[s][i] = 0.5f;

    meterL.store(0.0f); meterR.store(0.0f);

    masterSmoothed.reset(sampleRate, 0.02);
    masterSmoothed.setCurrentAndTargetValue(pMaster->load());
}

//==============================================================================
void TheDreamerProcessor::buildPatch(dreamer::DreamPatch& patch) const
{
    for (int t = 0; t < 4; ++t) {
        auto& d = patch.tone[t];
        const auto& s = pTone[t];
        d.enabled      = s.on->load() > 0.5f;
        d.waveIndex    = (int)s.wave->load();
        d.coarse       = (int)s.oct->load() * 12;          // oct -> semitones
        d.fineCents    = s.fine->load();
        d.level        = s.level->load();
        d.velSens      = s.velo->load();
        d.start01      = s.start->load();
        d.startRandom  = s.startRandom->load() > 0.5f;
        d.pan          = s.pan->load();
        d.noise        = s.noise->load();
        d.noiseColor   = s.noiseColor->load();
        d.dir          = s.dir->load();
        d.vecInt       = s.vint->load();
        d.shapeMode    = (int)s.shape->load();
        d.shapeDepth   = s.shapeDepth->load();
        d.tvfMode      = (int)s.tvfType->load();
        d.cutoffHz     = cutHz(s.tvfCut->load());
        d.resonance    = s.tvfRes->load();
        d.tvfEnvAmount = envHz(s.tvfEnv->load());
        d.tvfKeyFollow = s.tvfKf->load();
        d.tvfA = adsrSec(s.tvfA->load()); d.tvfD = adsrSec(s.tvfD->load());
        d.tvfS = s.tvfS->load();          d.tvfR = adsrSec(s.tvfR->load());
        d.tvaA = adsrSec(s.tvaA->load()); d.tvaD = adsrSec(s.tvaD->load());
        d.tvaS = s.tvaS->load();          d.tvaR = adsrSec(s.tvaR->load());
        d.auxA = adsrSec(s.auxA->load()); d.auxD = adsrSec(s.auxD->load());
        d.auxS = s.auxS->load();          d.auxR = adsrSec(s.auxR->load());
        d.auxDest = (int)s.auxDest->load();
        d.auxAmt  = s.auxAmt->load();
        auto fillLfo = [](dreamer::ToneParams::ToneLfo& o, const LfoPtrs& q) {
            o.rate01 = q.rate->load();
            o.depth  = q.depth->load();
            o.sync   = q.sync->load() > 0.5f;
            o.dest   = (int)q.dest->load();
            o.shape  = (int)q.shape->load();
        };
        fillLfo(d.lfo1, s.lfo[0]);
        fillLfo(d.lfo2, s.lfo[1]);
    }
    patch.interp = (int)pInterp->load();

    patch.vec.phase        = pVecPhase->load();
    patch.vec.orbitOn      = pVecOrbitOn->load() > 0.5f;
    patch.vec.orbitRate01  = pVecOrbitRate->load();
    patch.vec.orbitShape   = (int)pVecOrbitShape->load();
    patch.vec.orbitPerVoice = pVecOrbitVoice->load() > 0.5f;
    patch.vec.penvOn       = pVecPenvOn->load() > 0.5f;
    patch.vec.penvLoop     = pVecPenvLoop->load() > 0.5f;
    patch.vec.penvStart    = pVecPenvStart->load();
    patch.vec.penvEnd      = pVecPenvEnd->load();
    patch.vec.penvTime     = penvSec(pVecPenvTime->load());

    patch.glfoShape01 = (int)pLfoShape->load();
    patch.glfoRate01  = pLfoRate->load() * 100.0;          // 0..1 -> Lfo map

    for (int i = 0; i < 3; ++i) {
        patch.slot[i].src  = (int)pMtxSrc[i]->load() + 1;  // engine enums have none=0
        patch.slot[i].dest = (int)pMtxDst[i]->load() + 1;
        patch.slot[i].amt  = pMtxAmt[i]->load();
    }
    patch.drift = pDrift->load();
}

//==============================================================================
void TheDreamerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    buildPatch(synth.patch());
    synth.updateLive();

    // host tempo for the tone-LFO + delay sync divisions (fallback 120)
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit(20.0, 999.0, *b);
    synth.setBpm((float)bpm);

    const bool vintage = pEngine->load() < 0.5f;
    const int  os      = vintage ? 1 : 2;
    if (os != lastOversample) {
        lastOversample = os;
        for (int ch = 0; ch < 2; ++ch) { f1[ch].setOversample(os); f2[ch].setOversample(os); }
    }

    numEvents = 0;
    drainGuiMidi();                 // GUI keyboard/wheels first: offset-0 events,
                                    // keeping events[] monotonically sorted
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        const int  ofs = meta.samplePosition;
        if (msg.isNoteOn())
            pushEvent({ ofs, NoteEvent::Type::on, msg.getNoteNumber(),
                        msg.getVelocity() / 127.0f });
        else if (msg.isNoteOff())
            pushEvent({ ofs, NoteEvent::Type::off, msg.getNoteNumber(), 0.0f });
        else if (msg.isPitchWheel())
            pushEvent({ ofs, NoteEvent::Type::bend, 0,
                        (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f });
        else if (msg.isSustainPedalOn())
            pushEvent({ ofs, NoteEvent::Type::sustain, 0, 1.0f });
        else if (msg.isSustainPedalOff())
            pushEvent({ ofs, NoteEvent::Type::sustain, 0, 0.0f });
        else if (msg.isAllNotesOff())
            pushEvent({ ofs, NoteEvent::Type::allNotesOff, 0, 0.0f });
        else if (msg.isAllSoundOff())
            pushEvent({ ofs, NoteEvent::Type::allSoundOff, 0, 0.0f });
        else if (msg.isControllerOfType(1))
            synth.setWheel(msg.getControllerValue() / 127.0f);
    }

    const int   routing = (int)pFltRoute->load();
    const int   f1t     = (int)pFlt1Type->load();
    const int   f2t     = (int)pFlt2Type->load();
    const float f1Env   = pFlt1Env->load();
    for (int ch = 0; ch < 2; ++ch) { f1[ch].setType(f1t); f2[ch].setType(f2t); }

    const bool  modfxOn   = pModfxOn->load() > 0.5f;
    const int   modfxType = (int)pModfxType->load();
    const float mfRate01  = pModfxRate->load();
    const float mfDepth   = pModfxDepth->load();
    const float mfMix     = pModfxMix->load();
    const float chorusRateHz   = 0.05f + mfRate01 * 2.95f;   // donor knob laws
    const float chorusRangeMs  = 0.5f + mfDepth * 5.5f;
    const float flangerRateHz  = 0.02f + mfRate01 * 1.98f;
    const float flangerRangeMs = 0.1f + mfDepth * 2.9f;
    const float flangerFb      = 0.3f + mfDepth * 0.5f;
    const float phaserRateHz   = 0.02f + mfRate01 * 1.98f;
    const float ensembleRateHz = 0.2f + mfRate01 * 5.8f;     // string-machine range

    const float barberRateHz   = 0.02f + mfRate01 * 1.98f;   // barberpole sweep

    const bool dlyOn = pDlyOn->load() > 0.5f;
    static constexpr int delayTypeMap[3] = { 2, 0, 1 };      // DIG/TAPE/PONG -> donor enum
    delay.setType((dreamer::StereoDelay::Type)delayTypeMap[(int)pDlyMode->load()]);
    if (pDlySync->load() > 0.5f) {
        // v9: tempo-sync the delay time to the same 12-division table the tone
        // LFOs use (dreamer::toneLfoDivisionBeats), off the host BPM.
        const int idx = (int)(pDlyTime->load() * 11.0f + 0.5f);
        const double beats = dreamer::toneLfoDivisionBeats(idx);
        const double ms = beats * 60000.0 / (bpm > 1.0 ? bpm : 120.0);
        delay.setTimeFree((float)juce::jlimit(1.0, 4000.0, ms));
    } else {
        delay.setTimeFree((float)dlyMs(pDlyTime->load()));
    }
    const float dfb  = pDlyFb->load() * 0.9f;                // 0..1 -> 0..0.9 (donor cap)
    const float dwet = dlyOn ? pDlyMix->load() : 0.0f;

    // ---- FX v1.5 per-block reads (FEATURES.md 11) --------------------------
    // Smooth the continuous PARAMS extras once per block (~20 ms one-pole).
    // Discrete extras (Dim MODE, Barber STAGES/DIR, Rotary SPEED) snap and
    // de-click inside their own effect. dly/rev extras are reserved (the
    // ported StereoDelay/juce::Reverb can't take extra knobs under rule 1).
    fxSmoothCoef_ = 1.0f - std::exp(-(float)numSamples / (0.02f * (float)getSampleRate()));
    std::atomic<float>* const* pP[3] = { pModfxP, pDlyP, pRevP };
    for (int s = 0; s < 3; ++s)
        for (int i = 0; i < 4; ++i)
            fxP_[s][i] += fxSmoothCoef_ * (pP[s][i]->load() - fxP_[s][i]);

    const bool  lofiOn   = pLofiOn->load() > 0.5f;
    const bool  lofiPre  = pFxPrePost->load() > 0.5f;        // Pre = 1
    const float lofiBits = pLofiBits->load(), lofiSrate = pLofiSrate->load();
    const float lofiComp = pLofiCompand->load();
    const bool  lofiAlias = pLofiAlias->load() > 0.5f;
    const bool  widthOn   = pWidthOn->load() > 0.5f;
    const float widthAmt  = pWidth->load(), widthHaas = pWidthHaas->load();
    const bool  widthBM   = pWidthBassMono->load() > 0.5f;
    const bool  talkOn    = pTalkOn->load() > 0.5f;
    const float talkVa = pTalkVa->load(), talkVb = pTalkVb->load();
    const float talkMorph = pTalkMorph->load(), talkSens = pTalkSens->load();

    masterSmoothed.setTargetValue(pMaster->load());

    constexpr float kVoiceHeadroom = 0.5f;    // fixed pre-FX headroom (master is post-FX)

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    int nextEvent = 0;
    for (int n = 0; n < numSamples; ++n) {
        while (nextEvent < numEvents && events[nextEvent].offset <= n) {
            const auto& e = events[nextEvent++];
            switch (e.type) {
            case NoteEvent::Type::on:          synth.noteOn(e.note, e.value); break;
            case NoteEvent::Type::off:         synth.noteOff(e.note);         break;
            case NoteEvent::Type::bend:        synth.setPitchBend(e.value);   break;
            case NoteEvent::Type::sustain:     synth.setSustain(e.value > 0.5f); break;
            case NoteEvent::Type::allNotesOff: synth.allNotesOff();           break;
            case NoteEvent::Type::allSoundOff: synth.killAll();               break;
            }
        }

        float l = 0.0f, r = 0.0f;
        synth.process(l, r);

        // LO-FI PRE-filter: crush INTO the resonant filter sweep (the
        // musically valuable placement; architect F4)
        if (lofiOn && lofiPre)
            lofi.process(l, r, lofiBits, lofiSrate, lofiComp, lofiAlias);

        if (gfCtrl-- <= 0) {
            gfCtrl = 15;
            double h1 = cutHz(pFlt1Cut->load())
                      * std::pow(2.0, (double)synth.matrixCut1Oct()
                                    + (double)f1Env * 2.0 * (double)synth.auxMax());
            double h2 = cutHz(pFlt2Cut->load())
                      * std::pow(2.0, (double)synth.matrixCut2Oct());
            h1 = juce::jlimit(20.0, 18000.0, h1);
            h2 = juce::jlimit(20.0, 18000.0, h2);
            for (int ch = 0; ch < 2; ++ch) {
                f1[ch].setCutoffRes(h1, pFlt1Res->load());
                f2[ch].setCutoffRes(h2, pFlt2Res->load());
            }
        }
        if (routing == 0) {                                  // SER (BAL inert)
            l = f2[0].process(f1[0].process(l));
            r = f2[1].process(f1[1].process(r));
        } else {                                             // PAR: BAL 1<->2
            // normalized weights: bal 0 = equal (pre-v7 behavior), +/-1 = solo
            const float bal = pFltBal->load();
            const float w1  = bal > 0.0f ? 1.0f - bal : 1.0f;
            const float w2  = bal < 0.0f ? 1.0f + bal : 1.0f;
            const float nrm = 0.5f * (w1 + w2) > 0.0f ? 1.0f / (w1 + w2) : 0.5f;
            l = (w1 * f1[0].process(l) + w2 * f2[0].process(l)) * nrm;
            r = (w1 * f1[1].process(r) + w2 * f2[1].process(r)) * nrm;
        }

        l = dcBlock[0].processSample(l);
        r = dcBlock[1].processSample(r);
        if (vintage) {
            l = dreamer::Truncate16::processSample(l);
            r = dreamer::Truncate16::processSample(r);
        }
        l *= kVoiceHeadroom;
        r *= kVoiceHeadroom;

        if (modfxOn) {
            const float* p = fxP_[0];        // modfx PARAMS extras p0..p3
            switch (modfxType) {
            case 0:  chorus.process(l, r, 4.0f, chorusRangeMs, chorusRateHz, 0.0f, mfMix); break;
            case 1:  flanger.process(l, r, 0.5f, flangerRangeMs, flangerRateHz, flangerFb, mfMix); break;
            case 2:  phaser.process(l, r, mfDepth, phaserRateHz, mfMix); break;
            case 3:  ensemble.process(l, r, ensembleRateHz, mfDepth, mfMix, p[0], p[1]); break;   // SPREAD, TONE
            case 4:  dimension.process(l, r, p[0], p[1], mfMix); break;                            // MODE, TONE
            case 5:  rotary.process(l, r, p[0], p[1], p[2], p[3], mfMix); break;                   // SPEED/ACCEL/BAL/WIDTH
            default: barberpole.process(l, r, barberRateHz, p[0], p[1], p[2], mfMix); break;       // DIR/STAGES/FB
            }
        }

        float wetL, wetR;
        delay.processSampleWet(0.5f * (l + r), dfb, wetL, wetR);
        l += wetL * dwet;
        r += wetR * dwet;

        left[n] = l;
        if (right != nullptr) right[n] = r;
    }

    // ---- reverb (per-block, donor voicing + cache) -------------------------
    const bool  revOn = pRevOn->load() > 0.5f;
    const float rmix  = revOn ? pRevMix->load() : 0.0f;
    if (rmix > 0.001f) {
        const float size = pRevSize->load();
        const float damp = pRevDamp->load();
        static constexpr int revTypeMap[3] = { 0, 2, 1 };    // ROOM/HALL/PLATE -> donor cases
        const int revType = revTypeMap[(int)pRevType->load()];

        if (revType != revCache.type || size != revCache.size || damp != revCache.damp) {
            revCache = { revType, size, damp };
            juce::Reverb::Parameters rp;
            switch (revType) {
                case 0:  rp.roomSize = 0.25f + 0.45f * size;
                         rp.damping  = 0.35f + 0.55f * damp;
                         rp.width    = 0.7f;  break;
                case 1:  rp.roomSize = 0.45f + 0.35f * size;
                         rp.damping  = 0.10f + 0.50f * damp;
                         rp.width    = 1.0f;  break;
                default: rp.roomSize = 0.65f + 0.35f * size;
                         rp.damping  = 0.20f + 0.60f * damp;
                         rp.width    = 1.0f;  break;
            }
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.freezeMode = 0.0f;
            reverb.setParameters(rp);
        }
        for (int n = 0; n < numSamples; ++n) {
            reverbWetL[(size_t)n] = left[n];
            reverbWetR[(size_t)n] = right != nullptr ? right[n] : left[n];
        }
        if (right != nullptr)
            reverb.processStereo(reverbWetL.data(), reverbWetR.data(), numSamples);
        else
            reverb.processMono(reverbWetL.data(), numSamples);

        // Dry gain must be CONTINUOUS with the bypass path (dry x1.0 when
        // reverb is off): the donor's kDryScale=2.0 doubled the dry level the
        // instant mix crossed the 1% threshold -> a click + a huge ROOM/HALL
        // level jump. With pure-wet render (dryLevel=0) the manual mix needs
        // no 2x compensation: at rmix->0 dry->1.0 (= bypass), so no step.
        for (int n = 0; n < numSamples; ++n) {
            left[n] = left[n] * (1.0f - 0.4f * rmix) + reverbWetL[(size_t)n] * (0.8f * rmix);
            if (right != nullptr)
                right[n] = right[n] * (1.0f - 0.4f * rmix) + reverbWetR[(size_t)n] * (0.8f * rmix);
        }
        reverbWasActive = true;
    } else if (reverbWasActive) {
        reverb.reset();
        reverbWasActive = false;
        revCache.type = -1;
    }

    // ---- POST stages (LO-FI if POST, WIDTH fixed POST, TALK) -> MASTER -----
    // WIDTH is fixed POST-filter (architect F4: pre-delay width only reaches
    // the dry + reverb, so POST widens the whole image predictably). TALK
    // sits after the POST slot (F-review recommended order).
    float peakL = 0.0f, peakR = 0.0f;
    for (int n = 0; n < numSamples; ++n) {
        float l = left[n];
        float r = right != nullptr ? right[n] : left[n];

        if (lofiOn && !lofiPre)
            lofi.process(l, r, lofiBits, lofiSrate, lofiComp, lofiAlias);
        if (widthOn)
            stereoWidth.process(l, r, widthAmt, widthHaas, widthBM);
        if (talkOn)
            talkbox.process(l, r, talkVa, talkVb, talkMorph, talkSens, 1.0f);

        const float g = masterSmoothed.getNextValue();
        l *= g; r *= g;
        left[n] = l;
        if (right != nullptr) right[n] = r;
        peakL = std::fmax(peakL, std::fabs(l));
        peakR = std::fmax(peakR, std::fabs(r));
    }
    meterL.store(peakL, std::memory_order_relaxed);
    meterR.store(peakR, std::memory_order_relaxed);
}

//==============================================================================
// GUI keyboard/wheels -> engine (lock-free SPSC; producer = editor native fns
// on the message thread, consumer = drainGuiMidi at the top of processBlock).
void TheDreamerProcessor::pushGui(GuiMidiEvent e) noexcept
{
    int s1, n1, s2, n2;
    guiFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0) { guiEvents[s1] = e; guiFifo.finishedWrite(1); }
    // n1 == 0 -> FIFO full: drop the event (never block, never grow)
}

void TheDreamerProcessor::guiNoteOn(int note, float vel01) noexcept
{
    pushGui({ (uint8_t)GuiMidi::on, (uint8_t)juce::jlimit(0, 127, note),
              juce::jlimit(0.0f, 1.0f, vel01) });
}
void TheDreamerProcessor::guiNoteOff(int note) noexcept
{
    pushGui({ (uint8_t)GuiMidi::off, (uint8_t)juce::jlimit(0, 127, note), 0.0f });
}
void TheDreamerProcessor::guiPitchBend(float bendNorm) noexcept
{
    // -1..+1 -> +/-2 semitones (same span as the host pitch-wheel path)
    pushGui({ (uint8_t)GuiMidi::bend, 0, juce::jlimit(-1.0f, 1.0f, bendNorm) * 2.0f });
}
void TheDreamerProcessor::guiModWheel(float w01) noexcept
{
    pushGui({ (uint8_t)GuiMidi::modWheel, 0, juce::jlimit(0.0f, 1.0f, w01) });
}
void TheDreamerProcessor::guiAllNotesOff() noexcept
{
    pushGui({ (uint8_t)GuiMidi::allNotesOff, 0, 0.0f });
}

void TheDreamerProcessor::drainGuiMidi() noexcept
{
    int s1, n1, s2, n2;
    guiFifo.prepareToRead(guiFifo.getNumReady(), s1, n1, s2, n2);
    auto apply = [this](const GuiMidiEvent& e) {
        switch ((GuiMidi)e.type) {
        case GuiMidi::on:          pushEvent({ 0, NoteEvent::Type::on,  e.note, e.value }); break;
        case GuiMidi::off:         pushEvent({ 0, NoteEvent::Type::off, e.note, 0.0f });    break;
        case GuiMidi::bend:        pushEvent({ 0, NoteEvent::Type::bend, 0, e.value });     break;
        case GuiMidi::modWheel:    synth.setWheel(e.value);                                 break;
        case GuiMidi::allNotesOff: pushEvent({ 0, NoteEvent::Type::allNotesOff, 0, 0.0f }); break;
        }
    };
    for (int i = 0; i < n1; ++i) apply(guiEvents[s1 + i]);
    for (int i = 0; i < n2; ++i) apply(guiEvents[s2 + i]);
    guiFifo.finishedRead(n1 + n2);
}

//==============================================================================
void TheDreamerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void TheDreamerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* TheDreamerProcessor::createEditor()
{
    return new TheDreamerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheDreamerProcessor();
}
