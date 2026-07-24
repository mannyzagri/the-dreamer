#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"
#include "Td007Remap.h"
#include "BinaryData.h"

using namespace dreamer::params;

namespace {
// ---- normalized 0..1 -> unit maps (documented contract addenda) -----------
inline double cutHz(double v)   { return 60.0 * std::pow(200.0, v); }        // 60..12000
inline double envHz(double v)   { return v * 9600.0; }                       // unipolar (s9)
inline double adsrSec(double v) { return dreamer::lawv::envTimeSec(v); }     // D1: log 1ms..10s
inline double dlyMs(double v)   { return std::pow(1000.0, v); }              // 1..1000
inline double penvSec(double v) { return dreamer::lawv::penvTimeSec(v); }    // 0.02..10 (D1 display)
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
    pGEnvA = p(kGEnvA); pGEnvD = p(kGEnvD); pGEnvS = p(kGEnvS); pGEnvR = p(kGEnvR);
    pGCutoff = p(kGCutoff); pGRes = p(kGRes);
    pGOctave = p(kGOctave); pLimiterOn = p(kLimiterOn);
    // v15 GUI additions
    pFlt2Env = p(kFlt2Env); pLfoSync = p(kLfoSync);
    pLfo2Rate = p(kLfo2Rate); pLfo2Shape = p(kLfo2Shape); pLfo2Sync = p(kLfo2Sync);  // v16
    pModfxPFocus = p(kModfxPFocus); pDlyPFocus = p(kDlyPFocus); pRevPFocus = p(kRevPFocus);
    pModfxParam = p(kModfxParam); pDlyParam = p(kDlyParam); pRevParam = p(kRevParam);
    pLofiParam = p(kLofiParam); pLofiPFocus = p(kLofiPFocus);
    pTalkParam = p(kTalkParam); pTalkPFocus = p(kTalkPFocus);

    // D6: flat param index table + empty CC map.
    for (auto* rp : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(rp))
            paramByIndex.push_back(r);
    for (auto& c : ccToParam) c.store(-1, std::memory_order_relaxed);

    loadFactoryPresets();   // parse embedded bank once (message thread, ctor)
}

//==============================================================================
// Factory preset bank -- parsed once at construction from the embedded
// presets.json. Validation (PRESETS.md): every param id in every preset MUST
// exist in the APVTS; an unknown id is preset/param drift, so we fail LOUD in
// debug (jassert + DBG) and, in release, skip the unknown id while counting it.
void TheDreamerProcessor::loadFactoryPresets()
{
    const auto parsed = juce::JSON::parse(
        juce::String::fromUTF8(BinaryData::presets_json, BinaryData::presets_jsonSize));
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) { jassertfalse; return; }

    const auto arr = root->getProperty("presets");
    if (! arr.isArray()) { jassertfalse; return; }

    int unknownCount = 0;
    for (const auto& pv : *arr.getArray())
    {
        auto* po = pv.getDynamicObject();
        if (po == nullptr) { jassertfalse; continue; }

        Preset preset;
        preset.name     = po->getProperty("name").toString();
        preset.category = po->getProperty("category").toString();

        const auto params = po->getProperty("params");
        if (auto* paramObj = params.getDynamicObject())
        {
            preset.values.reserve((size_t)paramObj->getProperties().size());
            for (const auto& kv : paramObj->getProperties())
            {
                const juce::String id = kv.name.toString();
                if (apvts.getParameter(id) == nullptr)
                {
                    ++unknownCount;
                    DBG("[presets] UNKNOWN param id '" << id << "' in preset '"
                        << preset.name << "' -- skipped");
                    jassertfalse;   // preset/param drift bug: make it loud
                    continue;       // release: skip unknown id, keep the count
                }
                preset.values.emplace_back(id, kv.value);
            }
        }
        presets.push_back(std::move(preset));
    }

    DBG("[presets] loaded " << (int)presets.size() << " factory presets; "
        << unknownCount << " unknown param id(s)");
    jassert(unknownCount == 0);   // must be 0 -- see PRESETS.md validation
}

//==============================================================================
// applyPreset -- one atomic, host-visible state change on the message thread.
//
// Encoding contract (verified against presets.json): the JSON stores
//   * AudioParameterChoice -> integer choice INDEX (may exceed 1),
//   * AudioParameterBool    -> true/false,
//   * every Float AND Int param -> its NORMALIZED 0..1 value (already in the
//     0..1 domain even for bipolar/ranged params such as pan, fine, oct,
//     hit_pitchtrim -- they are NOT stored in real units here).
// So we dispatch on the PARAM TYPE, not on an assumed range: choices are
// normalized through the choice range, bools map to 0/1, and everything else
// is passed straight through as the normalized value. setValueNotifyingHost
// updates the host, the undo/automation view, AND the WebView relays in one
// step, so the whole panel reflects the patch automatically.
//
// NaN/Inf: values are JSON literals already in range; convertTo0to1 and the
// direct 0..1 pass-through cannot manufacture NaN/Inf, so no sanitising needed.
// Shared preset decoder (factory + user). Encoding contract: bool -> true/false;
// choice -> integer index; Float/Int -> the normalized 0..1 value straight
// through. Message thread only (setValueNotifyingHost).
void TheDreamerProcessor::applyParamMap(
    const std::vector<std::pair<juce::String, juce::var>>& values)
{
    // TD-007 remap, preset-JSON path (this decoder serves factory AND user
    // presets): a stored flt1_type/flt2_type index > 3 can only come from a
    // pre-TD-007 user-preset file (the factory bank is migrated, all <= 3 ->
    // no-op there), so the unconditional remap is safe -- same reasoning and
    // same table as setStateInformation (plugin/Td007Remap.h). The neutralize
    // cut/res overrides are applied AFTER the loop so a stored cut/res entry
    // later in the map cannot undo them. Message thread only.
    bool neutralize[2] = { false, false };

    for (const auto& [id, value] : values)
    {
        auto* param = apvts.getParameter(id);
        if (param == nullptr) continue;     // unknown id -> skip (belt-and-braces)

        if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr)
            param->setValueNotifyingHost((bool)value ? 1.0f : 0.0f);
        else if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(param))
        {
            int idx = (int)value;
            const int slot = (id == kFlt1Type) ? 0 : (id == kFlt2Type) ? 1 : -1;
            if (slot >= 0)
            {
                bool tr = false;
                idx = dreamer::td007::remapGlobalFilterType(idx, tr);
                neutralize[slot] = neutralize[slot] || tr;
            }
            param->setValueNotifyingHost(cp->convertTo0to1((float)idx));
        }
        else
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (float)(double)value));
    }

    static constexpr const char* remapCutIds[2] = { kFlt1Cut, kFlt2Cut };
    static constexpr const char* remapResIds[2] = { kFlt1Res, kFlt2Res };
    for (int s = 0; s < 2; ++s)
        if (neutralize[s])
        {
            if (auto* p = apvts.getParameter(remapCutIds[s])) p->setValueNotifyingHost(1.0f);
            if (auto* p = apvts.getParameter(remapResIds[s])) p->setValueNotifyingHost(0.0f);
        }
}

void TheDreamerProcessor::applyPreset(int index)
{
    if (index < 0 || index >= (int)presets.size())
        return;
    applyParamMap(presets[(size_t)index].values);
    currentProgram = index + 1;             // D4: host program 0 is INIT
    loadedUserPresetName.clear();           // TD-010: factory load overrides user name
}

// D4 INIT patch. Reset every parameter to its APVTS default, then apply the
// INIT-specific delta (tone A = Basic Saw cycle). The layout defaults already
// encode the rest of the spec: tone A on / B-D off, serial routing, filter 1
// LP fully open + res 0, filter 2 neutral, all FX off, no orbit/p-env, master
// 0.78, limiter on. Message thread only (setValueNotifyingHost).
void TheDreamerProcessor::resetToInit()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            rp->setValueNotifyingHost(rp->getDefaultValue());

    if (auto* w = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(tid("wave", 0))))
    {
        int sawIdx = 0;
        for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i)
            if (juce::String(rompler::bank3::kWaveforms[(size_t)i].category) == "Basic"
                && juce::String(rompler::bank3::kWaveforms[(size_t)i].name) == "Saw")
            { sawIdx = i; break; }
        w->setValueNotifyingHost(w->convertTo0to1((float)sawIdx));
    }
    currentProgram = 0;
    loadedUserPresetName.clear();           // TD-010: INIT overrides user name
}

const juce::String TheDreamerProcessor::getProgramName(int index)
{
    if (index <= 0) return "INIT";                       // D4 program 0
    const int f = index - 1;
    if (f >= (int)presets.size()) return {};
    const auto& p = presets[(size_t)f];
    return p.category.isNotEmpty() ? (p.category + ": " + p.name) : p.name;
}

// D13 panic (message thread): request an audio-thread flush. Never touch synth
// or FX state from here -- that would race processBlock.
void TheDreamerProcessor::panic() noexcept { panicRequested.store(true, std::memory_order_relaxed); }

// D13 flush (audio thread): hard-off all voices and zero the FX tails so no
// delay/reverb energy survives. Also the NaN-recovery sink (GAIN_STAGING 7d).
void TheDreamerProcessor::flushFx() noexcept
{
    synth.killAll();
    delay.reset();                          // long tails -- the D13 priority
    reverb.reset();
    reverbWasActive = false;
    revCache.type = -1;
    for (auto& c : reverbWetL) c = 0.0f;
    for (auto& c : reverbWetR) c = 0.0f;
    ensemble.reset(); dimension.reset(); rotary.reset(); barberpole.reset();
    chorus.reset();   flanger.reset();      phaser.reset();
    for (int ch = 0; ch < 2; ++ch) dcBlock[ch].reset();
    // TD-001: a flush must clear EVERY stateful stage, or poisoned state
    // survives the recovery. Previously missed: the global filters (their
    // recursions kept ringing through D13/panic) and the POST stages.
    for (int ch = 0; ch < 2; ++ch) { f1[ch].reset(); f2[ch].reset(); }
    lofi.reset();
    stereoWidth.reset();
    talkbox.reset();
    scopeBuf.fill(0.0f);                    // no stale garbage into the GUI FFT
}

// D3: snapshot the last n final-output samples (in order) as an Array<var> for
// the WebView analyzer. n clamped to the ring size. Message thread (editor timer).
juce::var TheDreamerProcessor::getScopeData(int n) const
{
    if (n < 1) n = 1; else if (n > kScopeSize) n = kScopeSize;
    const uint32_t w = scopeWrite.load(std::memory_order_acquire);
    juce::Array<juce::var> out;
    out.ensureStorageAllocated(n);
    for (int i = n; i > 0; --i)
    {
        const uint32_t idx = (w - (uint32_t)i) & (uint32_t)(kScopeSize - 1);
        out.add((double)scopeBuf[(size_t)idx]);
    }
    return out;
}

juce::String TheDreamerProcessor::presetName(int index) const
{
    return (index >= 0 && index < (int)presets.size()) ? presets[(size_t)index].name
                                                       : juce::String();
}

juce::String TheDreamerProcessor::presetCategory(int index) const
{
    return (index >= 0 && index < (int)presets.size()) ? presets[(size_t)index].category
                                                       : juce::String();
}

juce::var TheDreamerProcessor::getPresetList() const
{
    juce::Array<juce::var> list;
    for (const auto& p : presets)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("name", p.name);
        o->setProperty("category", p.category);
        o->setProperty("bank", "FACTORY");   // D14: GUI distinguishes FACTORY/USER
        list.add(juce::var(o));
    }
    return juce::var(list);
}

// Bank-authoritative wave list so the GUI shows REAL loop/hit names (PAD_01,
// AIRY_06, MORPH_PADAIR, HIT_CHIFF, ...) instead of "Loop NNN" placeholders.
juce::var TheDreamerProcessor::getWaveList() const
{
    using namespace rompler::bank3;
    juce::Array<juce::var> list;
    for (int i = 0; i < kNumWaveforms; ++i)
    {
        const auto& w = kWaveforms[(size_t)i];
        const char* tag = w.type == WaveType::Loop    ? "ENS"
                        : w.type == WaveType::OneShot  ? "SHOT" : "";
        auto* o = new juce::DynamicObject();
        o->setProperty("category", juce::String(w.category));
        o->setProperty("name",     juce::String(w.name));
        o->setProperty("tag",      juce::String(tag));
        list.add(juce::var(o));
    }
    return juce::var(list);
}

void TheDreamerProcessor::cacheTonePtrs(TonePtrs& dst, int t)
{
    auto p = [&](const char* base) { return apvts.getRawParameterValue(tid(base, t)); };
    dst.wave = p("wave"); dst.on = p("on"); dst.level = p("level");
    dst.oct = p("oct"); dst.semi = p("semi"); dst.fine = p("fine");
    dst.start = p("start"); dst.startRandom = p("start_random");
    dst.velo = p("velo"); dst.pan = p("pan");
    dst.shape = p("shape"); dst.shapeDepth = p("shape_depth");
    dst.noise = p("noise"); dst.noiseColor = p("noise_color");
    dst.dir = p("dir"); dst.vint = p("vint");
    dst.detuneVoices = p("detune_voices"); dst.detuneAmount = p("detune_amount");
    dst.voicing = p("voicing"); dst.dreamySpread = p("dreamy_spread");
    dst.loopMode = p("loop_mode"); dst.hitPlay = p("hit_play");
    dst.hitStretch = p("hit_stretch"); dst.hitPitchTrim = p("hit_pitchtrim");
    dst.loopRate = p("loop_rate"); dst.loopRateSync = p("loop_rate_sync");
    dst.loopRateBeats = p("loop_rate_beats"); dst.loopVarispeed = p("loop_varispeed");
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
        // TD-001: GlobalFilter::setSampleRate does NOT clear sub-filter state
        // (ToneSvf/FilterExtra/ZPlane just store sr_) -- without this, filter
        // recursion state survived host stop/start and SR changes.
        f1[ch].reset();
        f2[ch].reset();
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
    outputStage.prepare(0.5f);        // GAIN_STAGING s5: gentle soft-clip drive
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
    // D5 global offsets (bipolar, normalized-space; added to each tone's knob
    // value before its unit map, then clamped). D8 global octave folds into
    // every tone's coarse. All default 0 -> a static patch is bit-identical.
    const float gEnvA = pGEnvA->load(), gEnvD = pGEnvD->load(),
                gEnvS = pGEnvS->load(), gEnvR = pGEnvR->load();
    const float gCut  = pGCutoff->load(), gRes = pGRes->load();
    const int   gOct  = (int)pGOctave->load();
    auto off01 = [](float base, float offs) {                 // clamp(base+offs, 0..1)
        return juce::jlimit(0.0f, 1.0f, base + offs);
    };
    for (int t = 0; t < 4; ++t) {
        auto& d = patch.tone[t];
        const auto& s = pTone[t];
        d.enabled      = s.on->load() > 0.5f;
        d.waveIndex    = (int)s.wave->load();
        // D7 semi + D8 global octave: pitch = (oct + g_octave)*12 + semi (+ fine + mod)
        d.coarse       = ((int)s.oct->load() + gOct) * 12 + (int)s.semi->load();
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
        d.voicing      = (int)s.voicing->load();        // s11
        d.dreamySpread = (int)s.dreamySpread->load();   // s11
        d.detuneVoices = (int)s.detuneVoices->load() + 1;   // D9 choice idx 0..3 -> 1..4
        d.detuneAmount = s.detuneAmount->load();            // D9
        d.loopMode     = (int)s.loopMode->load();       // s12
        d.hitPlay      = (int)s.hitPlay->load();        // s13
        d.hitStretch   = s.hitStretch->load();          // s13 (0..1)
        d.hitPitchTrim = s.hitPitchTrim->load();        // s13 (Int -24..+24)
        d.loopRate      = s.loopRate->load();           // D15
        d.loopRateSync  = s.loopRateSync->load() > 0.5f;
        d.loopRateBeats = (int)s.loopRateBeats->load();
        d.loopVarispeed = s.loopVarispeed->load() > 0.5f;
        d.shapeMode    = (int)s.shape->load();
        d.shapeDepth   = s.shapeDepth->load();
        d.tvfMode      = (int)s.tvfType->load();
        // D5 g_cutoff / g_res offset every tone's TVF (normalized, then mapped).
        d.cutoffHz     = cutHz(off01(s.tvfCut->load(), gCut));
        d.resonance    = off01(s.tvfRes->load(), gRes);
        d.tvfEnvAmount = envHz(s.tvfEnv->load());
        d.tvfKeyFollow = s.tvfKf->load();               // D10 bipolar -1..+1
        d.tvfA = adsrSec(s.tvfA->load()); d.tvfD = adsrSec(s.tvfD->load());
        d.tvfS = s.tvfS->load();          d.tvfR = adsrSec(s.tvfR->load());
        // D5 g_env_* offset the TVA envelope of every tone (normalized-space).
        d.tvaA = adsrSec(off01(s.tvaA->load(), gEnvA)); d.tvaD = adsrSec(off01(s.tvaD->load(), gEnvD));
        d.tvaS = off01(s.tvaS->load(), gEnvS);          d.tvaR = adsrSec(off01(s.tvaR->load(), gEnvR));
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
    patch.glfoSync    = pLfoSync->load() > 0.5f;           // v15 global LFO sync
    patch.glfo2Shape01 = (int)pLfo2Shape->load();          // v16 GLOBAL LFO 2
    patch.glfo2Rate01  = pLfo2Rate->load() * 100.0;
    patch.glfo2Sync    = pLfo2Sync->load() > 0.5f;

    for (int i = 0; i < 3; ++i) {
        patch.slot[i].src  = (int)pMtxSrc[i]->load() + 1;  // engine enums have none=0
        patch.slot[i].dest = (int)pMtxDst[i]->load() + 1;
        patch.slot[i].amt  = pMtxAmt[i]->load();
    }
    patch.drift = pDrift->load();
    patch.morph = pFlt2Morph->load();   // TD-007: shared tone morph base
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

    // D13: consume a pending panic (GUI button) on the audio thread.
    if (panicRequested.exchange(false, std::memory_order_relaxed))
        flushFx();

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
    synth.setOversample(os);   // TD-007: tone Rhino family (gated inside)

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
        else if (msg.isController()) {
            const int   cc = juce::jlimit(0, 127, msg.getControllerNumber());
            const float v  = msg.getControllerValue() / 127.0f;
            if (cc == 1) synth.setWheel(v);          // CC1 stays the wheel mod source
            // D6 MIDI learn: if armed, bind this CC to the target param; then
            // route any mapped CC to its param. setValueNotifyingHost moves the
            // host + GUI too (the accepted MIDI-learn idiom; not per-sample).
            const int lt = midiLearnTarget.load(std::memory_order_relaxed);
            if (lt >= 0) {
                ccToParam[cc].store(lt, std::memory_order_relaxed);
                midiLearnTarget.store(-1, std::memory_order_relaxed);
            }
            const int pi = ccToParam[cc].load(std::memory_order_relaxed);
            if (pi >= 0 && pi < (int)paramByIndex.size())
                paramByIndex[(size_t)pi]->setValueNotifyingHost(v);
        }
    }

    const int   routing = pFltRoute->load() > 0.5f ? 1 : 0;   // Bool: house > 0.5f read (0=SER, 1=PAR)
    const int   f1t     = (int)pFlt1Type->load();
    const int   f2t     = (int)pFlt2Type->load();
    const float f1Env   = pFlt1Env->load();
    const float f2Env   = pFlt2Env->load();   // v15: F2 env (symmetric with F1)
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
    // v15 focus-shadow: the single PARAMS knob (<slot>_param) drives the
    // focus-selected extra; the other three keep their p_i. One-pole smoothing
    // absorbs the value swap when the focus changes (no click).
    std::atomic<float>* const* pP[3]   = { pModfxP, pDlyP, pRevP };
    std::atomic<float>*        pParam[3] = { pModfxParam, pDlyParam, pRevParam };
    std::atomic<float>*        pFocus[3] = { pModfxPFocus, pDlyPFocus, pRevPFocus };
    for (int s = 0; s < 3; ++s) {
        const int focus = juce::jlimit(0, 3, (int)pFocus[s]->load());
        for (int i = 0; i < 4; ++i) {
            const float target = (i == focus) ? pParam[s]->load() : pP[s][i]->load();
            fxP_[s][i] += fxSmoothCoef_ * (target - fxP_[s][i]);
        }
    }

    const bool  lofiOn   = pLofiOn->load() > 0.5f;
    const bool  lofiPre  = pFxPrePost->load() > 0.5f;        // Pre = 1
    // LO-FI reads its RAW named params directly: the v15 GUI does its own focus
    // routing (the PARAMS knob rebinds to lofi_bits/srate/compand/alias per
    // lofi_pfocus), so the DSP must NOT shadow via lofi_param. (lofi_param +
    // lofi_pfocus stay as params for GUI binding but are engine-inert here.)
    const float lofiBits  = pLofiBits->load();
    const float lofiSrate = pLofiSrate->load();
    const float lofiComp  = pLofiCompand->load();
    const bool  lofiAlias = pLofiAlias->load() > 0.5f;
    const bool  widthOn   = pWidthOn->load() > 0.5f;
    const float widthAmt  = pWidth->load(), widthHaas = pWidthHaas->load();
    const bool  widthBM   = pWidthBassMono->load() > 0.5f;
    const bool  talkOn    = pTalkOn->load() > 0.5f;
    // TALK keeps the focus-shadow: the v15 GUI's TALK PARAMS knob binds the
    // proxy talk_param (unlike LO-FI's raw routing), so the focused vowel/morph/
    // sens sub-param reads talk_param; the rest read their own params.
    const int   talkFocus = juce::jlimit(0, 3, (int)pTalkPFocus->load());
    const float talkKnob  = pTalkParam->load();
    const float talkVa    = talkFocus == 0 ? talkKnob : pTalkVa->load();
    const float talkVb    = talkFocus == 1 ? talkKnob : pTalkVb->load();
    const float talkMorph = talkFocus == 2 ? talkKnob : pTalkMorph->load();
    const float talkSens  = talkFocus == 3 ? talkKnob : pTalkSens->load();

    masterSmoothed.setTargetValue(pMaster->load());

    // Modest pre-FX headroom. The PRINCIPLED per-voice summing fix now lives in
    // DreamVoice (GAIN_STAGING s1: 1/sqrt(nEnabledTones) tone-sum trim), so this
    // is no longer the anti-clip crutch it once was -- just headroom that leaves
    // room for polyphony ahead of the output soft-clip/ceiling (s5).
    // 2.7.4 loudness pass (measured, user-approved): 0.5 -> 0.7 (+2.9 dB).
    // Measured before the change: INIT chord -16.8 dBFS RMS / -6.0 peak; the
    // factory bank sat 8-25 dB below INIT. The s5 tanh soft-clip + -0.1 dBFS
    // ceiling absorb the raised peaks. Ear-tunable single constant.
    constexpr float kVoiceHeadroom = 0.7f;

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

#if DREAMER_TD001_TRACE
    // TD-001 diagnostic tracer (compiled out in release). Per-stage block peaks
    // + a ONE-SHOT stderr report on the first block where any stage exceeds
    // the trip level (or goes non-finite) -- names the first runaway stage.
    float trStg[8] = {};                 // 0 synth 1 filt 2 modfx 3 dly 4 rev 5 width 6 talk 7 final
    static std::atomic<int>      trFired { 0 };
    static std::atomic<uint64_t> trBlocks { 0 };
    #define TR_TAP(slot) trStg[slot] = std::fmax(trStg[slot], std::fmax(std::fabs(l), std::fabs(r)))
#else
    #define TR_TAP(slot) ((void)0)
#endif

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
        TR_TAP(0);

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
                      * std::pow(2.0, (double)synth.matrixCut2Oct()
                                    + (double)f2Env * 2.0 * (double)synth.auxMax());
            h1 = juce::jlimit(20.0, 18000.0, h1);
            h2 = juce::jlimit(20.0, 18000.0, h2);
            for (int ch = 0; ch < 2; ++ch) {
                f1[ch].setCutoffRes(h1, pFlt1Res->load());
                f2[ch].setCutoffRes(h2, pFlt2Res->load());
                // TD-007: flt2_morph no longer feeds GlobalFilter -- the globals
                // are choice-4 now (DreamPlane unreachable there); the morph
                // drives every tone's ZPlane via buildPatch/DreamSynth instead.
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
        TR_TAP(1);

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
        TR_TAP(2);

        float wetL, wetR;
        delay.processSampleWet(0.5f * (l + r), dfb, wetL, wetR);
        // GAIN_STAGING s3: linear dry/wet, NOT additive. Old `l += wetL*dwet`
        // stacked full dry + wet -> instant clip as mix rose. dmix==0 -> pure
        // dry (bypass preserved); dmix==1 -> pure wet. Feedback is capped at
        // 0.9 (< 0.95) above via dfb, so high fb can't build to clipping.
        l = l * (1.0f - dwet) + wetL * dwet;
        r = r * (1.0f - dwet) + wetR * dwet;
        TR_TAP(3);

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

        // GAIN_STAGING s3: proper equal-power/linear dry/wet. The old form
        // (dry*(1-0.4*rmix) + wet*0.8*rmix) summed to ~1.4x at mix=1 and never
        // fully removed the dry -> the "reverb clips" smoking gun. New law:
        //   dry*(1-rmix) + wet*(kReverbWetNorm*rmix)
        // fully crossfades (dry gone at mix=1) and normalizes the wet so a
        // max-size hall from a unity input stays <= unity. Bypass continuity is
        // PRESERVED: the render is pure-wet (dryLevel=0) so at rmix->0 the dry
        // coeff -> 1.0 (= bypass), no click/step (the property the old comment
        // guarded). kReverbWetNorm gives the wet path a little headroom too.
        constexpr float kReverbWetNorm = 0.7f;   // wet leg gain at full mix (-3 dB)
        for (int n = 0; n < numSamples; ++n) {
            const float dg = 1.0f - rmix;
            const float wg = kReverbWetNorm * rmix;
            left[n] = left[n] * dg + reverbWetL[(size_t)n] * wg;
            if (right != nullptr)
                right[n] = right[n] * dg + reverbWetR[(size_t)n] * wg;
        }
        reverbWasActive = true;
    } else if (reverbWasActive) {
        reverb.reset();
        reverbWasActive = false;
        revCache.type = -1;
    }
#if DREAMER_TD001_TRACE
    for (int n = 0; n < numSamples; ++n) {
        const float l = left[n], r = right != nullptr ? right[n] : left[n];
        trStg[4] = std::fmax(trStg[4], std::fmax(std::fabs(l), std::fabs(r)));
    }
#endif

    // ---- POST stages (LO-FI if POST, WIDTH fixed POST, TALK) -> MASTER -----
    // WIDTH is fixed POST-filter (architect F4: pre-delay width only reaches
    // the dry + reverb, so POST widens the whole image predictably). TALK
    // sits after the POST slot (F-review recommended order).
    const bool limiterOn = pLimiterOn->load() > 0.5f;   // D12
    float peakL = 0.0f, peakR = 0.0f;
    bool  nanSeen = false;                               // TD-001: fmax ABSORBS NaN
    float grMin = 1.0f;                                  // D12: worst gain ratio
    uint32_t sw = scopeWrite.load(std::memory_order_relaxed);   // D3 ring index
    for (int n = 0; n < numSamples; ++n) {
        float l = left[n];
        float r = right != nullptr ? right[n] : left[n];

        if (lofiOn && !lofiPre)
            lofi.process(l, r, lofiBits, lofiSrate, lofiComp, lofiAlias);
        if (widthOn)
            stereoWidth.process(l, r, widthAmt, widthHaas, widthBM);
        TR_TAP(5);
        if (talkOn)
            talkbox.process(l, r, talkVa, talkVb, talkMorph, talkSens, 1.0f);
        TR_TAP(6);

        const float g = masterSmoothed.getNextValue();
        l *= g; r *= g;
        // D12: the GAIN_STAGING s5 soft-clip + -0.1 dBFS ceiling is now a
        // switchable option (default ON). While on, track the worst-case gain
        // reduction so the GUI can light an activity LED. OFF = clean pass-through
        // (no character, no safety ceiling -- the user's choice, documented).
        if (limiterOn) {
            const float preMax = std::fmax(std::fabs(l), std::fabs(r));
            outputStage.process(l, r);
            const float postMax = std::fmax(std::fabs(l), std::fabs(r));
            if (preMax > 1.0e-6f) {
                const float ratio = postMax / preMax;
                if (ratio < grMin) grMin = ratio;
            }
        }
        TR_TAP(7);
        left[n] = l;
        if (right != nullptr) right[n] = r;
        // D3: tap the FINAL output (post-master/limiter) into the scope ring.
        scopeBuf[(size_t)(sw & (uint32_t)(kScopeSize - 1))] = 0.5f * (l + r);
        ++sw;
        peakL = std::fmax(peakL, std::fabs(l));
        peakR = std::fmax(peakR, std::fabs(r));
        nanSeen = nanSeen || (l != l) || (r != r);   // NaN != NaN; fmax would swallow it
    }
    scopeWrite.store(sw, std::memory_order_release);        // D3 publish
    // D13 NaN-recovery (GAIN_STAGING 7d): a non-finite sample means the chain
    // blew up -- clear the audio out, flush voices+FX, and blank the scope so
    // nothing propagates downstream or into the GUI FFT. TD-001 fix: the old
    // check tested isfinite(peak) only, but C++ fmax RETURNS THE NON-NAN
    // OPERAND, so a pure-NaN sample was absorbed and D13 only ever fired on
    // +/-Inf. nanSeen restores "NaN is NaN". (Finite overloads are a DIFFERENT
    // failure class and deliberately do NOT trigger this path.)
    if (nanSeen || ! std::isfinite(peakL) || ! std::isfinite(peakR)) {
        buffer.clear();
        flushFx();
        scopeBuf.fill(0.0f);
        peakL = peakR = 0.0f;
    }
#if DREAMER_TD001_TRACE
    ++trBlocks;
    {
        bool trip = false, nf = false;
        for (int s = 0; s < 8; ++s) {
            if (!std::isfinite(trStg[s])) { nf = true; trip = true; }
            if (trStg[s] > 8.0f) trip = true;
        }
        if (trip && trFired.fetch_add(1) < 5) {
            static const char* nm[8] = { "synth", "filters", "modfx", "delay",
                                         "reverb", "width", "talk", "final" };
            static const char* mf[7] = { "CHORUS", "FLANGER", "PHASER", "ENSEMBLE",
                                         "DIMENSION", "ROTARY", "BARBERPOLE" };
            int first = -1;
            for (int s = 0; s < 8; ++s)
                if (!std::isfinite(trStg[s]) || trStg[s] > 8.0f) { first = s; break; }
            std::fprintf(stderr, "\n[TD001-TRACE] FAULT at block %llu (t=%.2fs) "
                         "first-hot=%s%s\n  stage peaks:",
                         (unsigned long long)trBlocks.load(),
                         (double)trBlocks.load() * numSamples / getSampleRate(),
                         first >= 0 ? nm[first] : "?", nf ? " (non-finite)" : "");
            for (int s = 0; s < 8; ++s)
                std::fprintf(stderr, " %s=%.4g", nm[s], (double)trStg[s]);
            const int mt = juce::jlimit(0, 6, modfxType);
            std::fprintf(stderr, "\n  modfx: on=%d type=%s rate=%.3f depth=%.3f mix=%.3f "
                         "p=[%.3f %.3f %.3f %.3f] limiter=%d vintage=%d\n",
                         (int)modfxOn, mf[mt], mfRate01, mfDepth, mfMix,
                         fxP_[0][0], fxP_[0][1], fxP_[0][2], fxP_[0][3],
                         (int)limiterOn, (int)vintage);
            std::fflush(stderr);
        }
    }
#endif
#undef TR_TAP
    meterL.store(peakL, std::memory_order_relaxed);
    meterR.store(peakR, std::memory_order_relaxed);
    // D12: gain reduction in dB (>=0); 0 when off or not limiting.
    limiterGR.store((limiterOn && grMin < 1.0f) ? -20.0f * std::log10(grMin) : 0.0f,
                    std::memory_order_relaxed);
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
// ---- D6 MIDI learn (message thread) ----------------------------------------
int TheDreamerProcessor::paramIndexForId(const juce::String& id) const
{
    for (int i = 0; i < (int)paramByIndex.size(); ++i)
        if (paramByIndex[(size_t)i]->paramID == id) return i;
    return -1;
}
void TheDreamerProcessor::midiLearnStart(const juce::String& id)
{ midiLearnTarget.store(paramIndexForId(id), std::memory_order_relaxed); }
void TheDreamerProcessor::midiLearnCancel()
{ midiLearnTarget.store(-1, std::memory_order_relaxed); }
void TheDreamerProcessor::midiLearnClearParam(const juce::String& id)
{
    const int idx = paramIndexForId(id);
    if (idx < 0) return;
    for (auto& c : ccToParam)
        if (c.load(std::memory_order_relaxed) == idx) c.store(-1, std::memory_order_relaxed);
}
int TheDreamerProcessor::midiLearnCcForParam(const juce::String& id) const
{
    const int idx = paramIndexForId(id);
    if (idx < 0) return -1;
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[cc].load(std::memory_order_relaxed) == idx) return cc;
    return -1;
}

// ---- D14 user presets (message thread; factory bank stays read-only) -------
juce::File TheDreamerProcessor::userPresetDir() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("The Dreamer").getChildFile("Presets");
}
// Current APVTS -> a JSON param map in the same encoding as the factory bank.
juce::DynamicObject::Ptr TheDreamerProcessor::captureParamMap() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (auto* rp : paramByIndex)
    {
        const juce::String id = rp->paramID;
        if (auto* b = dynamic_cast<juce::AudioParameterBool*>(rp))   obj->setProperty(id, b->get());
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(rp)) obj->setProperty(id, c->getIndex());
        else obj->setProperty(id, (double)rp->getValue());          // normalized 0..1
    }
    return obj;
}
bool TheDreamerProcessor::saveUserPreset(const juce::String& name)
{
    const juce::String safe = juce::File::createLegalFileName(name).trim();
    if (safe.isEmpty()) return false;
    auto dir = userPresetDir();
    if (! dir.isDirectory()) dir.createDirectory();
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("name", name);
    root->setProperty("category", "USER");
    root->setProperty("bank", "USER");
    root->setProperty("params", juce::var(captureParamMap().get()));
    return dir.getChildFile(safe + ".json").replaceWithText(juce::JSON::toString(juce::var(root.get())));
}
bool TheDreamerProcessor::loadUserPreset(const juce::String& name)
{
    auto f = userPresetDir().getChildFile(juce::File::createLegalFileName(name) + ".json");
    if (! f.existsAsFile()) return false;
    auto parsed = juce::JSON::parse(f);
    auto* o = parsed.getDynamicObject();
    if (o == nullptr) return false;
    auto* po = o->getProperty("params").getDynamicObject();
    if (po == nullptr) return false;
    std::vector<std::pair<juce::String, juce::var>> values;
    for (auto& prop : po->getProperties())
        values.emplace_back(prop.name.toString(), prop.value);
    applyParamMap(values);
    // TD-010: remember the loaded user patch by its stored display name (falls
    // back to the requested name for files without one). currentProgram keeps
    // its existing convention (unchanged by a user load); the name wins in the
    // editor's header display.
    const juce::String stored = o->getProperty("name").toString();
    loadedUserPresetName = stored.isNotEmpty() ? stored : name;
    return true;
}
bool TheDreamerProcessor::renameUserPreset(const juce::String& oldName, const juce::String& newName)
{
    auto dir = userPresetDir();
    auto a = dir.getChildFile(juce::File::createLegalFileName(oldName) + ".json");
    const juce::String safeNew = juce::File::createLegalFileName(newName).trim();
    if (! a.existsAsFile() || safeNew.isEmpty()) return false;
    auto parsed = juce::JSON::parse(a);
    if (auto* o = parsed.getDynamicObject()) o->setProperty("name", newName);
    auto b = dir.getChildFile(safeNew + ".json");
    if (! b.replaceWithText(juce::JSON::toString(parsed))) return false;
    if (b != a) a.deleteFile();
    return true;
}
bool TheDreamerProcessor::deleteUserPreset(const juce::String& name)
{
    return userPresetDir().getChildFile(juce::File::createLegalFileName(name) + ".json").deleteFile();
}
juce::var TheDreamerProcessor::getUserPresetList() const
{
    juce::Array<juce::var> list;
    auto dir = userPresetDir();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.json"))
        {
            auto parsed = juce::JSON::parse(f);
            auto* o = parsed.getDynamicObject();
            auto* e = new juce::DynamicObject();
            e->setProperty("name", o != nullptr ? o->getProperty("name")
                                                : juce::var(f.getFileNameWithoutExtension()));
            e->setProperty("category", o != nullptr ? o->getProperty("category") : juce::var("USER"));
            e->setProperty("bank", "USER");
            list.add(juce::var(e));
        }
    return juce::var(list);
}

//==============================================================================
// State = a wrapper element holding the APVTS param tree + the D6 MIDI-learn
// map. Old (pre-D6) states whose root IS the APVTS tree still load (legacy
// branch), so existing projects are not broken.
static const juce::Identifier kStateRoot   { "DreamerState" };
static const juce::Identifier kMidiLearnTag { "MIDILEARN" };

void TheDreamerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement root(kStateRoot);
    // TD-010: persist the program index + any loaded-user-preset name as
    // attributes on the wrapper element (legacy states simply lack them).
    root.setAttribute("program", currentProgram);
    root.setAttribute("userPreset", loadedUserPresetName);
    if (auto params = apvts.copyState().createXml())
        root.addChildElement(params.release());       // owned by root now
    auto* ml = root.createNewChildElement(kMidiLearnTag);
    for (int cc = 0; cc < 128; ++cc)
    {
        const int idx = ccToParam[cc].load(std::memory_order_relaxed);
        if (idx >= 0 && idx < (int)paramByIndex.size())
        {
            auto* m = ml->createNewChildElement("MAP");
            m->setAttribute("cc", cc);
            m->setAttribute("param", paramByIndex[(size_t)idx]->paramID);
        }
    }
    copyXmlToBinary(root, destData);
}

void TheDreamerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) return;

    juce::XmlElement* paramsXml = nullptr;
    juce::XmlElement* mlXml     = nullptr;
    int          storedProgram = -1;                       // TD-010 (-1 = absent/legacy)
    juce::String storedUserPreset;
    if (xml->hasTagName(kStateRoot))                       // D6 wrapper format
    {
        paramsXml = xml->getChildByName(apvts.state.getType());
        mlXml     = xml->getChildByName(kMidiLearnTag);
        storedProgram    = xml->getIntAttribute("program", -1);
        storedUserPreset = xml->getStringAttribute("userPreset");
    }
    else if (xml->hasTagName(apvts.state.getType()))       // legacy pre-D6 state
    {
        paramsXml = xml.get();
    }

    // D6: rebuild the CC map (empty if none stored / legacy state).
    for (auto& c : ccToParam) c.store(-1, std::memory_order_relaxed);
    if (mlXml != nullptr)
        for (auto* m = mlXml->getChildByName("MAP"); m != nullptr;
             m = m->getNextElementWithTagName("MAP"))
        {
            const int cc  = m->getIntAttribute("cc", -1);
            const int idx = paramIndexForId(m->getStringAttribute("param"));
            if (cc >= 0 && cc < 128 && idx >= 0)
                ccToParam[cc].store(idx, std::memory_order_relaxed);
        }

    if (paramsXml != nullptr)
        {
            auto tree = juce::ValueTree::fromXml(*paramsXml);

            // --- TD-007 global-filter-type remap (pre-TD-007 saved states) ---
            // Any stored flt1_type/flt2_type index > 3 can ONLY come from a
            // pre-TD-007 state -- the remap is safe unconditionally, no
            // version tag needed. The mapping table itself lives in
            // plugin/Td007Remap.h (single source of truth, shared with
            // applyParamMap's preset-JSON path).
            // tvf_type old 0-3 is a subset of the new 0-13: nothing to do.
            {
                auto paramChild = [&tree](const juce::String& id) {
                    return tree.getChildWithProperty("id", id);
                };
                const char* typeIds[2] = { "flt1_type", "flt2_type" };
                const char* cutIds[2]  = { "flt1_cut",  "flt2_cut"  };
                const char* resIds[2]  = { "flt1_res",  "flt2_res"  };
                for (int s = 0; s < 2; ++s)
                {
                    auto tp = paramChild(typeIds[s]);
                    if (! tp.isValid()) continue;
                    const int old = juce::roundToInt((double)tp.getProperty("value", 0.0));
                    if (old <= 3) continue;                       // TD-007-native
                    bool transparent = false;
                    const int nu = dreamer::td007::remapGlobalFilterType(old, transparent);
                    tp.setProperty("value", (double)nu, nullptr);
                    if (transparent)
                    {
                        if (auto cp = paramChild(cutIds[s]); cp.isValid())
                            cp.setProperty("value", 1.0, nullptr);
                        if (auto rp = paramChild(resIds[s]); rp.isValid())
                            rp.setProperty("value", 0.0, nullptr);
                    }
                }
            }

            apvts.replaceState(tree);

            // --- pluginval strictness-8 "Plugin state restoration" fix --------
            // Snap every AudioParameterBool to a clean 0.0/1.0 after a state load.
            // Root cause: juce::AudioParameterBool stores the RAW normalized float
            // it was last set to (setValue: `value = newValue;` -- it does NOT
            // quantize; only get() applies `value >= 0.5`). So getValue() can
            // report e.g. 0.7983, and APVTS serialises that raw float into the
            // state tree -- a fractional bool round-trips through save/restore
            // verbatim. With the 47-program bank, applyPreset() (invoked via the
            // host's synthetic "Program" parameter during pluginval's randomise
            // pass) sets some bools to a clean 1.0 which pluginval records as the
            // "expected" value, while a raw fractional (0.7983) survives into the
            // restored tree -- so expected(1.0) != restored(0.7983) and the
            // "Plugin state restoration" test fails (tolerance 0.1). Re-setting
            // each bool to its own quantised boolean makes getValue() report the
            // clean 0/1 the host expects; setValueNotifyingHost also reconciles
            // the host / VST3 EditController cache. The boolean meaning is
            // unchanged (0.7983 -> true -> 1.0), so audio behaviour is identical.
            // Message-thread only (get/setStateInformation contract) -- rule 5
            // safe. Does not touch applyPreset or the GUI preset path.
            for (auto* p : getParameters())
                if (auto* b = dynamic_cast<juce::AudioParameterBool*>(p))
                    b->setValueNotifyingHost(b->get() ? 1.0f : 0.0f);
        }

    // TD-010: restore the program index + user-preset name AFTER the param
    // tree has landed (the params ARE the sound -- these are display/host
    // bookkeeping only, no applyPreset re-fire). Range-guarded; a legacy
    // state without the attributes leaves currentProgram as today and
    // clears any stale user name.
    if (storedProgram >= 0 && storedProgram < getNumPrograms())
        currentProgram = storedProgram;
    loadedUserPresetName = storedUserPreset;
}

juce::AudioProcessorEditor* TheDreamerProcessor::createEditor()
{
    return new TheDreamerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TheDreamerProcessor();
}
