#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

using namespace dreamer::params;

//==============================================================================
TheDreamerProcessor::TheDreamerProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    const char* prefixes[4] = { "a_", "b_", "c_", "d_" };
    for (int t = 0; t < 4; ++t) cacheTonePtrs(pTone[t], prefixes[t]);

    auto p = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    pInterp = p(kInterp); pEngine = p(kEngine);
    pVolume = p(kVolume); pOutput = p(kOutput);
    pVecPhase = p(kVecPhase); pOrbitOn = p(kOrbitOn); pOrbitRate = p(kOrbitRate);
    pPenvOn = p(kPenvOn); pPenvLoop = p(kPenvLoop);
    pPenvStart = p(kPenvStart); pPenvEnd = p(kPenvEnd); pPenvTime = p(kPenvTime);
    pGlfoShape = p(kGlfoShape); pGlfoRate = p(kGlfoRate);
    for (int i = 0; i < 3; ++i) {
        const juce::String n(i + 1);
        pMtxSrc[i]  = p("mod" + n + "_src");
        pMtxDest[i] = p("mod" + n + "_dest");
        pMtxAmt[i]  = p("mod" + n + "_amt");
    }
    pF1Type = p(kF1Type); pF1Cutoff = p(kF1Cutoff); pF1Reso = p(kF1Reso); pF1Env = p(kF1Env);
    pF2Type = p(kF2Type); pF2Cutoff = p(kF2Cutoff); pF2Reso = p(kF2Reso); pF2Morph = p(kF2Morph);
    pFiltRouting = p(kFiltRouting);
    pModfxOn = p(kModfxOn); pModfxType = p(kModfxType);
    pModfxRate = p(kModfxRate); pModfxDepth = p(kModfxDepth); pModfxMix = p(kModfxMix);
    pDelayOn = p(kDelayOn); pDelayMode = p(kDelayMode);
    pDelayTime = p(kDelayTime); pDelayFb = p(kDelayFb); pDelayMix = p(kDelayMix);
    pRevOn = p(kRevOn); pRevType = p(kRevType);
    pRevSize = p(kRevSize); pRevDamp = p(kRevDamp); pRevMix = p(kRevMix);
}

void TheDreamerProcessor::cacheTonePtrs(TonePtrs& dst, const char* prefix)
{
    auto p = [&](const char* suffix) {
        return apvts.getRawParameterValue(pid(prefix, suffix));
    };
    dst.on = p("on"); dst.wave = p("wave"); dst.coarse = p("coarse");
    dst.fine = p("fine"); dst.level = p("level"); dst.velsens = p("velsens");
    dst.start = p("start"); dst.pan = p("pan");
    dst.dir = p("dir"); dst.vint = p("int");
    dst.shapeType = p("shape_type"); dst.shapeDepth = p("shape_depth");
    dst.tvfType = p("tvf_type"); dst.cutoff = p("cutoff"); dst.reso = p("reso");
    dst.envAmt = p("env_amt"); dst.keyfollow = p("keyfollow");
    dst.tvfA = p("tvf_att"); dst.tvfD = p("tvf_dec"); dst.tvfS = p("tvf_sus"); dst.tvfR = p("tvf_rel");
    dst.tvaA = p("tva_att"); dst.tvaD = p("tva_dec"); dst.tvaS = p("tva_sus"); dst.tvaR = p("tva_rel");
    dst.auxA = p("aux_att"); dst.auxD = p("aux_dec"); dst.auxS = p("aux_sus"); dst.auxR = p("aux_rel");
    dst.auxDest = p("aux_dest"); dst.auxAmt = p("aux_amt");
    dst.lfoDepth = p("lfo_depth"); dst.lfoDest = p("lfo_dest");
}

//==============================================================================
void TheDreamerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.prepare(sampleRate);
    synth.killAll();
    lastOversample = 0;

    for (int ch = 0; ch < 2; ++ch) {
        f1[ch].setSampleRate(sampleRate);
        f2[ch].setSampleRate(sampleRate);
        dcBlock[ch].prepare(sampleRate);
    }
    gfCtrl = 0;

    delay.prepare(sampleRate);
    chorus.prepare(sampleRate, 6.0f);
    flanger.prepare(sampleRate, 3.0f);
    phaser.prepare(sampleRate);
    reverb.setSampleRate(sampleRate);
    reverb.reset();
    reverbWasActive = false;
    revCache = {};

    reverbWetL.assign((size_t)juce::jmax(1, samplesPerBlock), 0.0f);
    reverbWetR.assign((size_t)juce::jmax(1, samplesPerBlock), 0.0f);

    volumeSmoothed.reset(sampleRate, 0.02);
    outputSmoothed.reset(sampleRate, 0.02);
    volumeSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(pVolume->load()));
    outputSmoothed.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(pOutput->load()));
}

//==============================================================================
void TheDreamerProcessor::buildPatch(dreamer::DreamPatch& patch) const
{
    for (int t = 0; t < 4; ++t) {
        auto& d = patch.tone[t];
        const auto& s = pTone[t];
        d.enabled      = s.on->load() > 0.5f;
        d.waveIndex    = (int)s.wave->load();
        d.coarse       = (int)s.coarse->load();
        d.fineCents    = s.fine->load();
        d.level        = s.level->load();
        d.velSens      = s.velsens->load();
        d.startOffset  = s.start->load();
        d.pan          = s.pan->load();
        d.dir          = s.dir->load();
        d.vecInt       = s.vint->load();
        d.shapeMode    = (int)s.shapeType->load();
        d.shapeDepth   = s.shapeDepth->load();
        d.tvfMode      = (int)s.tvfType->load();
        d.cutoffHz     = s.cutoff->load();
        d.resonance    = s.reso->load();
        d.tvfEnvAmount = s.envAmt->load();
        d.tvfKeyFollow = s.keyfollow->load();
        d.tvfA = s.tvfA->load() * 0.001; d.tvfD = s.tvfD->load() * 0.001;
        d.tvfS = s.tvfS->load();         d.tvfR = s.tvfR->load() * 0.001;
        d.tvaA = s.tvaA->load() * 0.001; d.tvaD = s.tvaD->load() * 0.001;
        d.tvaS = s.tvaS->load();         d.tvaR = s.tvaR->load() * 0.001;
        d.auxA = s.auxA->load() * 0.001; d.auxD = s.auxD->load() * 0.001;
        d.auxS = s.auxS->load();         d.auxR = s.auxR->load() * 0.001;
        d.auxDest = (int)s.auxDest->load();
        d.auxAmt  = s.auxAmt->load();
        d.lfoDepth = s.lfoDepth->load();
        d.lfoDest  = (int)s.lfoDest->load();
    }
    patch.interp = (int)pInterp->load();

    patch.vec.phase       = pVecPhase->load();
    patch.vec.orbitOn     = pOrbitOn->load() > 0.5f;
    patch.vec.orbitRate01 = pOrbitRate->load();
    patch.vec.penvOn      = pPenvOn->load() > 0.5f;
    patch.vec.penvLoop    = pPenvLoop->load() > 0.5f;
    patch.vec.penvStart   = pPenvStart->load();
    patch.vec.penvEnd     = pPenvEnd->load();
    patch.vec.penvTime    = pPenvTime->load();

    patch.glfoShape01 = (int)pGlfoShape->load();
    patch.glfoRate01  = pGlfoRate->load();

    for (int i = 0; i < 3; ++i) {
        // param choices have no "-": engine enums are param index + 1;
        // a slot is inert at amt = 0
        patch.slot[i].src  = (int)pMtxSrc[i]->load() + 1;
        patch.slot[i].dest = (int)pMtxDest[i]->load() + 1;
        patch.slot[i].amt  = pMtxAmt[i]->load();
    }
}

//==============================================================================
void TheDreamerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // ---- patch + engine ----------------------------------------------------
    buildPatch(synth.patch());
    synth.updateLive();

    const bool vintage = pEngine->load() < 0.5f;
    const int  os      = vintage ? 1 : 2;
    if (os != lastOversample) {
        lastOversample = os;
        for (int ch = 0; ch < 2; ++ch) { f1[ch].setOversample(os); f2[ch].setOversample(os); }
    }

    // ---- MIDI --------------------------------------------------------------
    numEvents = 0;
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
        else if (msg.isControllerOfType(1))                       // mod wheel
            synth.setWheel(msg.getControllerValue() / 127.0f);    // matrix source
    }

    // ---- per-block settings ------------------------------------------------
    const int  routing  = (int)pFiltRouting->load();              // 0 SER, 1 PAR
    const int  f1Type   = (int)pF1Type->load();
    const int  f2Type   = (int)pF2Type->load();
    const float f1Env   = pF1Env->load();
    for (int ch = 0; ch < 2; ++ch) { f1[ch].setType(f1Type); f2[ch].setType(f2Type); }

    const bool  modfxOn   = pModfxOn->load() > 0.5f;
    const int   modfxType = (int)pModfxType->load();
    const float mfRate01  = pModfxRate->load();
    const float mfDepth   = pModfxDepth->load();
    const float mfMix     = pModfxMix->load();
    // Rubber-Rhino knob laws, mix from the panel's own MIX knob
    const float chorusRateHz  = 0.05f + mfRate01 * 2.95f;
    const float chorusRangeMs = 0.5f + mfDepth * 5.5f;
    const float flangerRateHz  = 0.02f + mfRate01 * 1.98f;
    const float flangerRangeMs = 0.1f + mfDepth * 2.9f;
    const float flangerFb      = 0.3f + mfDepth * 0.5f;
    const float phaserRateHz   = 0.02f + mfRate01 * 1.98f;

    const bool delayOn = pDelayOn->load() > 0.5f;
    // panel DIGITAL/TAPE/PONG -> StereoDelay Type {tape=0, ping=1, dig=2}
    static constexpr int delayTypeMap[3] = { 2, 0, 1 };
    delay.setType((dreamer::StereoDelay::Type)delayTypeMap[(int)pDelayMode->load()]);
    delay.setTimeFree(pDelayTime->load());
    const float dfb  = pDelayFb->load();
    const float dwet = delayOn ? pDelayMix->load() : 0.0f;

    volumeSmoothed.setTargetValue(juce::Decibels::decibelsToGain(pVolume->load()));
    outputSmoothed.setTargetValue(juce::Decibels::decibelsToGain(pOutput->load()));

    // ---- render ------------------------------------------------------------
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
        synth.process(0.0f, 0.0f, l, r);

        // global filters, control-rate cutoff updates (matrix + F1 ENV ride in)
        if (gfCtrl-- <= 0) {
            gfCtrl = 15;
            double h1 = (double)pF1Cutoff->load()
                      * std::pow(2.0, (double)synth.matrixCut1Oct()
                                    + (double)f1Env * 2.0 * (double)synth.auxMax());
            double h2 = (double)pF2Cutoff->load()
                      * std::pow(2.0, (double)synth.matrixCut2Oct());
            h1 = juce::jlimit(20.0, 18000.0, h1);
            h2 = juce::jlimit(20.0, 18000.0, h2);
            for (int ch = 0; ch < 2; ++ch) {
                f1[ch].setCutoffRes(h1, pF1Reso->load());
                f2[ch].setCutoffRes(h2, pF2Reso->load());
            }
        }
        if (routing == 0) {                                   // SER
            l = f2[0].process(f1[0].process(l));
            r = f2[1].process(f1[1].process(r));
        } else {                                              // PAR
            l = 0.5f * (f1[0].process(l) + f2[0].process(l));
            r = 0.5f * (f1[1].process(r) + f2[1].process(r));
        }

        l = dcBlock[0].processSample(l);
        r = dcBlock[1].processSample(r);
        if (vintage) {
            l = dreamer::Truncate16::processSample(l);
            r = dreamer::Truncate16::processSample(r);
        }
        const float vg = volumeSmoothed.getNextValue();       // master, pre-FX
        l *= vg;
        r *= vg;

        if (modfxOn) {
            switch (modfxType) {
            case 0:  chorus.process(l, r, 4.0f, chorusRangeMs, chorusRateHz, 0.0f, mfMix); break;
            case 1:  flanger.process(l, r, 0.5f, flangerRangeMs, flangerRateHz, flangerFb, mfMix); break;
            default: phaser.process(l, r, mfDepth, phaserRateHz, mfMix); break;
            }
        }

        // delay: mono-sum input, stereo wet (FEATURES section 8)
        float wetL, wetR;
        delay.processSampleWet(0.5f * (l + r), dfb, wetL, wetR);
        l += wetL * dwet;
        r += wetR * dwet;

        left[n] = l;
        if (right != nullptr) right[n] = r;
    }

    // ---- reverb (per-block; donor coefficient-cache pattern) ---------------
    const bool  revOn = pRevOn->load() > 0.5f;
    const float rmix  = revOn ? pRevMix->load() : 0.0f;
    if (rmix > 0.001f) {
        const float size = pRevSize->load();
        const float damp = pRevDamp->load();
        // panel ROOM/HALL/PLATE -> donor voicing cases {room=0, plate=1, hall=2}
        static constexpr int revTypeMap[3] = { 0, 2, 1 };
        const int revType = revTypeMap[(int)pRevType->load()];

        if (revType != revCache.type || size != revCache.size || damp != revCache.damp) {
            revCache = { revType, size, damp };
            juce::Reverb::Parameters rp;
            switch (revType) {
                case 0:  rp.roomSize = 0.25f + 0.45f * size;   // Room
                         rp.damping  = 0.35f + 0.55f * damp;
                         rp.width    = 0.7f;  break;
                case 1:  rp.roomSize = 0.45f + 0.35f * size;   // Plate
                         rp.damping  = 0.10f + 0.50f * damp;
                         rp.width    = 1.0f;  break;
                default: rp.roomSize = 0.65f + 0.35f * size;   // Hall
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

        // donor dry/wet law (kDryScale matches juce::Reverb's shipped behavior)
        constexpr float kDryScale = 2.0f;
        for (int n = 0; n < numSamples; ++n) {
            left[n] = left[n] * (kDryScale * (1.0f - 0.4f * rmix)) + reverbWetL[(size_t)n] * (0.8f * rmix);
            if (right != nullptr)
                right[n] = right[n] * (kDryScale * (1.0f - 0.4f * rmix)) + reverbWetR[(size_t)n] * (0.8f * rmix);
        }
        reverbWasActive = true;
    } else if (reverbWasActive) {
        reverb.reset();
        reverbWasActive = false;
        revCache.type = -1;
    }

    // ---- output gain -------------------------------------------------------
    for (int n = 0; n < numSamples; ++n) {
        const float og = outputSmoothed.getNextValue();
        left[n] *= og;
        if (right != nullptr) right[n] *= og;
    }
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
