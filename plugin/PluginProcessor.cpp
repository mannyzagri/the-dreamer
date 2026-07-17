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
    cachePartialPtrs(pA, "a_");
    cachePartialPtrs(pB, "b_");

    auto p = [this](const char* id) { return apvts.getRawParameterValue(id); };
    pInterp = p(kInterp);   pEngine = p(kEngine);
    pVolume = p(kVolume);   pOutput = p(kOutput);
    pCondition = p(kCondition); pStability = p(kStability); pTempo = p(kTempo);
    pDistType = p(kDistType); pDistDrive = p(kDistDrive);
    pDistColor = p(kDistColor); pDistMix = p(kDistMix);
    pDelayType = p(kDelayType); pDelayMode = p(kDelayMode);
    pDelayMs = p(kDelayMs); pDelayFb = p(kDelayFb); pDelayMix = p(kDelayMix);
    pDelayHpSlope = p(kDelayHpSlope); pDelayHpFreq = p(kDelayHpFreq);
    pDelayLpSlope = p(kDelayLpSlope); pDelayLpFreq = p(kDelayLpFreq);
    pChorusOn = p(kChorusOn); pChorusDepth = p(kChorusDepth); pChorusRate = p(kChorusRate);
    pFlangerOn = p(kFlangerOn); pFlangerDepth = p(kFlangerDepth); pFlangerRate = p(kFlangerRate);
    pPhaserOn = p(kPhaserOn); pPhaserDepth = p(kPhaserDepth); pPhaserRate = p(kPhaserRate);
    pRevType = p(kRevType); pRevSize = p(kRevSize); pRevDamp = p(kRevDamp); pRevMix = p(kRevMix);
    pReverbHpSlope = p(kReverbHpSlope); pReverbHpFreq = p(kReverbHpFreq);
    pReverbLpSlope = p(kReverbLpSlope); pReverbLpFreq = p(kReverbLpFreq);
    pCompOn = p(kCompOn); pCompLim = p(kCompLim); pCompThresh = p(kCompThresh);
    pCompRatio = p(kCompRatio); pCompGain = p(kCompGain);
    pClipOn = p(kClipOn);
}

void TheDreamerProcessor::cachePartialPtrs(PartialPtrs& dst, const char* prefix)
{
    auto p = [&](const char* suffix) {
        return apvts.getRawParameterValue(pid(prefix, suffix));
    };
    dst.on = p("on"); dst.wave = p("wave"); dst.coarse = p("coarse");
    dst.fine = p("fine"); dst.level = p("level"); dst.velsens = p("velsens");
    dst.start = p("start"); dst.filterType = p("filter_type");
    dst.cutoff = p("cutoff"); dst.reso = p("reso");
    dst.envAmt = p("env_amt"); dst.keyfollow = p("keyfollow");
    dst.tvfA = p("tvf_att"); dst.tvfD = p("tvf_dec");
    dst.tvfS = p("tvf_sus"); dst.tvfR = p("tvf_rel");
    dst.tvaA = p("tva_att"); dst.tvaD = p("tva_dec");
    dst.tvaS = p("tva_sus"); dst.tvaR = p("tva_rel");
    for (int l = 0; l < 2; ++l) {
        const juce::String lp = "lfo" + juce::String(l + 1) + "_";
        auto q = [&](const char* suffix) {
            return apvts.getRawParameterValue(pid(prefix, (lp + suffix).toRawUTF8()));
        };
        dst.lfo[l].shape = q("shape"); dst.lfo[l].rate = q("rate");
        dst.lfo[l].depth = q("depth"); dst.lfo[l].dest = q("dest");
        dst.lfo[l].keytrig = q("keytrig");
    }
}

//==============================================================================
void TheDreamerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.prepare(sampleRate);
    synth.killAll();
    lastOversample = 0;   // force re-apply below

    dist.prepare(sampleRate);
    dcBlock.prepare(sampleRate);
    delay.prepare(sampleRate);
    chorus.prepare(sampleRate, 6.0f);
    flanger.prepare(sampleRate, 3.0f);
    phaser.prepare(sampleRate);
    comp.prepare(sampleRate);
    reverb.setSampleRate(sampleRate);
    reverb.reset();
    springReverb.prepare(sampleRate);
    springReverb.reset();
    reverbWasActive = false;
    revCache = {};

    // deterministic character, not re-randomised per instance (donor seed)
    instability.prepare(sampleRate, 0x1955B52Fu);

    delayHP.prepare(sampleRate, dreamer::ReturnFilter::Mode::highpass);
    delayLP.prepare(sampleRate, dreamer::ReturnFilter::Mode::lowpass);
    reverbHP.prepare(sampleRate, dreamer::ReturnFilter::Mode::highpass);
    reverbLP.prepare(sampleRate, dreamer::ReturnFilter::Mode::lowpass);

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
    auto fill = [](dreamer::DreamPartialParams& d, const PartialPtrs& s) {
        d.base.enabled      = s.on->load() > 0.5f;
        d.base.waveIndex    = (int)s.wave->load();
        d.base.coarse       = (int)s.coarse->load();
        d.base.fineCents    = s.fine->load();
        d.base.level        = s.level->load();
        d.base.velSens      = s.velsens->load();
        d.base.startOffset  = s.start->load();
        d.base.cutoffHz     = s.cutoff->load();
        d.base.resonance    = s.reso->load();
        d.base.tvfEnvAmount = s.envAmt->load();
        d.base.tvfKeyFollow = s.keyfollow->load();
        d.base.tvfA = s.tvfA->load() * 0.001; d.base.tvfD = s.tvfD->load() * 0.001;
        d.base.tvfS = s.tvfS->load();         d.base.tvfR = s.tvfR->load() * 0.001;
        d.base.tvaA = s.tvaA->load() * 0.001; d.base.tvaD = s.tvaD->load() * 0.001;
        d.base.tvaS = s.tvaS->load();         d.base.tvaR = s.tvaR->load() * 0.001;
        d.filterType = (int)s.filterType->load();
        for (int l = 0; l < 2; ++l) {
            d.lfo[l].shape   = (int)s.lfo[l].shape->load();
            d.lfo[l].rate01  = s.lfo[l].rate->load();
            d.lfo[l].depth   = s.lfo[l].depth->load();
            d.lfo[l].dest    = (int)s.lfo[l].dest->load();
            d.lfo[l].keyTrig = s.lfo[l].keytrig->load() > 0.5f;
        }
    };
    fill(patch.a, pA);
    fill(patch.b, pB);
    patch.interp = (int)pInterp->load();
}

//==============================================================================
void TheDreamerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    constexpr float kChorusBaseMs  = 4.0f;    // chorus centre delay (RR)
    constexpr float kFlangerBaseMs = 0.5f;    // flanger centre delay (RR)

    // ---- host BPM (delay sync); tempo param is the fallback ----------------
    double bpm = (double)pTempo->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit(20.0, 999.0, *b);

    // ---- patch + engine ----------------------------------------------------
    buildPatch(synth.patch());
    synth.updateLive();

    const bool vintage = pEngine->load() < 0.5f;
    const int  os      = vintage ? 1 : 2;
    if (os != lastOversample) {
        lastOversample = os;
        synth.setOversample(os);
        synth.killAll();      // hardware-honest: engine swap restarts voices
    }

    // ---- collect sample-accurate MIDI events -------------------------------
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
    }

    // ---- per-block FX settings (Rubber-Rhino mappings, verbatim) -----------
    dist.set((dreamer::Distortion::Mode)(int)pDistType->load(),
             pDistDrive->load(), pDistColor->load());
    const float distMix = pDistMix->load();

    delay.setType((dreamer::StereoDelay::Type)(int)pDelayType->load());
    if (pDelayMode->load() > 0.5f) {
        delay.setTimeFree(pDelayMs->load());
    } else {
        static const juce::NormalisableRange<float> msRange(1.0f, 1000.0f, 1.0f, 0.4f);
        const int zone = juce::jlimit(0, 7,
            (int)(msRange.convertTo0to1(pDelayMs->load()) * 8.0f));
        delay.setTimeSynced(zone, bpm);
    }
    const float fb  = pDelayFb->load();
    const float wet = pDelayMix->load();
    delayHP.setParams((int)pDelayHpSlope->load(), pDelayHpFreq->load());
    delayLP.setParams((int)pDelayLpSlope->load(), pDelayLpFreq->load());

    instability.setAmounts(pCondition->load(), pStability->load());

    const bool  chorusOn  = pChorusOn->load()  > 0.5f;
    const bool  flangerOn = pFlangerOn->load() > 0.5f;
    const bool  phaserOn  = pPhaserOn->load()  > 0.5f;

    const float chorusDepthN  = pChorusDepth->load();
    const float chorusRateHz  = 0.05f + pChorusRate->load() * 2.95f;    // 0.05..3 Hz
    const float chorusRangeMs = 0.5f + chorusDepthN * 5.5f;             // 0.5..6 ms
    const float chorusMix     = 0.3f + chorusDepthN * 0.4f;

    const float flangerDepthN  = pFlangerDepth->load();
    const float flangerRateHz  = 0.02f + pFlangerRate->load() * 1.98f;  // 0.02..2 Hz
    const float flangerRangeMs = 0.1f + flangerDepthN * 2.9f;           // 0.1..3 ms
    const float flangerFb      = 0.3f + flangerDepthN * 0.5f;           // 0.3..0.8
    const float flangerMix     = 0.3f + flangerDepthN * 0.4f;

    const float phaserDepthN = pPhaserDepth->load();
    const float phaserRateHz = 0.02f + pPhaserRate->load() * 1.98f;     // 0.02..2 Hz
    const float phaserMix    = 0.3f + phaserDepthN * 0.4f;

    volumeSmoothed.setTargetValue(juce::Decibels::decibelsToGain(pVolume->load()));
    outputSmoothed.setTargetValue(juce::Decibels::decibelsToGain(pOutput->load()));

    // ---- render: synth -> dist -> dcblock -> truncate -> volume -> delay ->
    //      mod-fx (Rubber-Rhino order) ---------------------------------------
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

        instability.tick();                        // exactly once per sample
        const auto drift = instability.drift();

        float x = synth.process(drift.tune * 0.20f,      // +/-20 cents * depth
                                drift.cutoff * 0.15f);   // +/-0.15 oct * depth
        x = dist.processSample(x, distMix);
        x = dcBlock.processSample(x);
        if (vintage)
            x = dreamer::Truncate16::processSample(x);
        x *= volumeSmoothed.getNextValue();              // master, pre-FX

        float wetL, wetR;
        delay.processSampleWet(x, fb, wetL, wetR);
        wetL = delayLP.processSample(0, delayHP.processSample(0, wetL));
        wetR = delayLP.processSample(1, delayHP.processSample(1, wetR));
        float l = x + wetL * wet;
        float r = x + wetR * wet;

        if (chorusOn)
            chorus.process(l, r, kChorusBaseMs, chorusRangeMs, chorusRateHz, 0.0f, chorusMix);
        if (flangerOn)
            flanger.process(l, r, kFlangerBaseMs, flangerRangeMs, flangerRateHz, flangerFb, flangerMix);
        if (phaserOn)
            phaser.process(l, r, phaserDepthN, phaserRateHz, phaserMix);

        left[n] = l;
        if (right != nullptr) right[n] = r;
    }

    // ---- reverb (per-block, Rubber-Rhino restructure mirrored) -------------
    const float rmix    = pRevMix->load();
    const int   revType = (int)pRevType->load();
    if (rmix > 0.001f) {
        const float size = pRevSize->load();
        const float damp = pRevDamp->load();

        if (revType == 3) {   // Spring
            springReverb.setParams(size, damp);
            reverbHP.setParams((int)pReverbHpSlope->load(), pReverbHpFreq->load());
            reverbLP.setParams((int)pReverbLpSlope->load(), pReverbLpFreq->load());
            for (int n = 0; n < numSamples; ++n) {
                float wl, wr;
                springReverb.processSample(left[n], wl, wr);
                reverbWetL[(size_t)n] = reverbLP.processSample(0, reverbHP.processSample(0, wl));
                reverbWetR[(size_t)n] = reverbLP.processSample(1, reverbHP.processSample(1, wr));
            }
            revCache.type = -1;   // force juce::Reverb coefficient refresh on switch-back
        } else {
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

            reverbHP.setParams((int)pReverbHpSlope->load(), pReverbHpFreq->load());
            reverbLP.setParams((int)pReverbLpSlope->load(), pReverbLpFreq->load());
            for (int n = 0; n < numSamples; ++n) {
                reverbWetL[(size_t)n] = reverbLP.processSample(0, reverbHP.processSample(0, reverbWetL[(size_t)n]));
                reverbWetR[(size_t)n] = reverbLP.processSample(1, reverbHP.processSample(1, reverbWetR[(size_t)n]));
            }
        }

        // dry gain 2*(1-0.4*rmix) matches juce::Reverb's internal dryScaleFactor
        // behavior the donor shipped with -- see the donor's NOTE comment.
        constexpr float kDryScale = 2.0f;
        for (int n = 0; n < numSamples; ++n) {
            left[n] = left[n] * (kDryScale * (1.0f - 0.4f * rmix)) + reverbWetL[(size_t)n] * (0.8f * rmix);
            if (right != nullptr)
                right[n] = right[n] * (kDryScale * (1.0f - 0.4f * rmix)) + reverbWetR[(size_t)n] * (0.8f * rmix);
        }
        reverbWasActive = true;
    } else if (reverbWasActive) {
        reverb.reset();          // drop the tail so re-enabling doesn't burst
        springReverb.reset();
        reverbHP.reset();
        reverbLP.reset();
        reverbWasActive = false;
        revCache.type = -1;
    }

    // ---- comp -> hiss -> output -> brickwall -------------------------------
    comp.setParams(pCompOn->load() > 0.5f, pCompLim->load() > 0.5f,
                   pCompThresh->load(), pCompRatio->load(), pCompGain->load());
    comp.resetGrMeter();
    const bool clipOn = pClipOn->load() > 0.5f;

    for (int n = 0; n < numSamples; ++n) {
        float l = left[n];
        float r = right != nullptr ? right[n] : l;

        comp.processSample(l, r);

        l += instability.hiss(0);   // exactly 0.0f at condition = 1 (pristine)
        r += instability.hiss(1);

        const float og = outputSmoothed.getNextValue();
        l *= og;
        r *= og;

        if (clipOn) {
            l = dreamer::BrickwallClip::processSample(l);
            r = dreamer::BrickwallClip::processSample(r);
        }

        left[n] = l;
        if (right != nullptr) right[n] = r;
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
