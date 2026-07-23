// The Dreamer v3 -- 24-voice 4-tone vector ROMpler, VST3.
// Engine: dsp/glue/DreamVoice.h (bank v3 Cycle/Loop/OneShot, noise,
// humanize drift, Dream Vector w/ orbit shapes, mod matrix). Bus:
//   stereo tone sum -> GLOBAL FILTERS 1/2 (SER/PAR) -> dcblock ->
//   [vintage Truncate16] -> fixed pre-FX headroom -> MOD FX (chorus |
//   flanger | phaser | ENSEMBLE) -> DELAY (mono-sum in) -> REVERB ->
//   MASTER (post-FX, smoothed -- the mapped output parameter).
// Parameters: DSP_BUILD.md section 9 canonical table (see Params.h for the
// flagged additions). Normalized 0..1 -> unit maps live in this file.
#pragma once
#include <array>
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/glue/DreamVoice.h"
#include "dsp/glue/GlobalFilter.h"
#include "dsp/glue/Ensemble.h"
#include "dsp/glue/Dimension.h"
#include "dsp/glue/Rotary.h"
#include "dsp/glue/Barberpole.h"
#include "dsp/glue/LoFi.h"
#include "dsp/glue/StereoWidth.h"
#include "dsp/glue/Talkbox.h"
#include "dsp/glue/OutputStage.h"       // GAIN_STAGING s5: soft-clip + ceiling
#include "dsp/ported/fx/Effects.h"      // DCBlocker, Truncate16, StereoDelay
#include "dsp/ported/fx/ModFx.h"        // ModDelayFx, Phaser

class TheDreamerProcessor : public juce::AudioProcessor {
public:
    TheDreamerProcessor();

    const juce::String getName() const override { return "The Dreamer"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    double getTailLengthSeconds() const override { return 15.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Factory preset bank -- processor-owned (PRESETS.md). The standard program
    // interface exposes the whole bank to the host's own preset menu; every
    // program IS a factory preset. Presets are parsed once at construction.
    // D4: program 0 = INIT (synthetic); programs 1..N = factory presets. The
    // GUI's applyPreset()/getPresetList() stay 0-based over the FACTORY bank.
    int getNumPrograms() override { return (int)presets.size() + 1; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int i) override { if (i <= 0) resetToInit(); else applyPreset(i - 1); }
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}

    // Public preset API so ANY editor (the JS GUI, a future JUCE editor, or the
    // host program menu) can drive preset recall through the processor. The GUI
    // becomes a viewer: it asks for names/categories and calls applyPreset --
    // it never holds the preset data itself.
    void         applyPreset(int index);   // atomic APVTS swap (message thread)
    void         resetToInit();            // D4: INIT patch (basic saw, FX off)
    void         panic() noexcept;         // D13: all-notes-off + FX flush (msg thread)
    juce::var    getScopeData(int n = 2048) const;  // D3: final-output tap for GUI FFT
    int          presetCount() const noexcept { return (int)presets.size(); }

    // ---- D6 MIDI learn (message thread; GUI right-click drives it) ----------
    void midiLearnStart(const juce::String& paramId);   // arm: next CC binds paramId
    void midiLearnCancel();                              // disarm
    void midiLearnClearParam(const juce::String& paramId);  // drop paramId's CC map
    int  midiLearnCcForParam(const juce::String& paramId) const;  // -1 if unmapped
    bool midiLearnIsArmed() const noexcept { return midiLearnTarget.load() >= 0; }

    // ---- D14 user presets (message thread; factory bank stays read-only) ----
    bool      saveUserPreset(const juce::String& name);
    bool      renameUserPreset(const juce::String& oldName, const juce::String& newName);
    bool      deleteUserPreset(const juce::String& name);
    bool      loadUserPreset(const juce::String& name);
    juce::var getUserPresetList() const;   // Array<{name,category,bank:"USER"}>
    juce::String presetName(int index) const;
    juce::String presetCategory(int index) const;
    juce::var    getPresetList() const;     // Array<{name,category}> for editors

    // TD-010: name of the last successfully loaded USER preset ("" = none /
    // factory). Set by loadUserPreset, cleared by applyPreset/resetToInit
    // (so any factory or INIT load overrides it). Message thread only; the
    // editor reads it to display the real patch name, and get/setState
    // persist it so a DAW session reload shows the user patch, not a stale
    // factory index.
    const juce::String& getLoadedUserPresetName() const noexcept { return loadedUserPresetName; }

    // Bank-authoritative wave list (bank3 order): Array<{category,name,tag}>,
    // tag "" cycle / "ENS" loop / "SHOT" one-shot. The GUI wave overlay reads
    // this so loop/hit entries show the REAL bank names, not placeholders.
    juce::var    getWaveList() const;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // output metering feed for the editor's header L/R meters (peak per block)
    float getMeterL() const noexcept { return meterL.load(std::memory_order_relaxed); }
    float getMeterR() const noexcept { return meterR.load(std::memory_order_relaxed); }
    // D12: output-limiter gain reduction (dB, >=0) for the GUI activity LED.
    float getLimiterGR() const noexcept { return limiterGR.load(std::memory_order_relaxed); }

    // v11: on-screen keyboard + pitch/mod wheels -> engine. Called by the
    // editor's WebView native functions on the MESSAGE thread; each just
    // enqueues onto a lock-free SPSC FIFO drained at the top of processBlock
    // (never touch synth/DSP state from here -- that would race the audio
    // thread). bendNorm is bipolar -1..+1 (mapped to the same +/-2 semitone
    // span as host pitch-wheel); w01 and vel01 are 0..1.
    void guiNoteOn (int note, float vel01) noexcept;
    void guiNoteOff(int note)              noexcept;
    void guiPitchBend(float bendNorm)      noexcept;
    void guiModWheel(float w01)            noexcept;
    void guiAllNotesOff()                  noexcept;

private:
    struct NoteEvent {
        int   offset = 0;
        enum class Type : int { on, off, bend, sustain, allNotesOff, allSoundOff } type = Type::on;
        int   note = 0;
        float value = 0.0f;
    };
    static constexpr int kMaxEvents = 512;
    NoteEvent events[kMaxEvents];
    int numEvents = 0;
    void pushEvent(const NoteEvent& e) { if (numEvents < kMaxEvents) events[numEvents++] = e; }

    // GUI-originated MIDI (v11 on-screen keyboard + wheels). SPSC ring buffer:
    // producer = message-thread editor native fns (guiNoteOn/... above),
    // consumer = processBlock (drainGuiMidi). Lock-free (juce::AbstractFifo,
    // no CriticalSection) to honour rule 5; fixed POD backing store, allocated
    // once. Owned by the processor so it outlives repeated editor create/destroy.
    struct GuiMidiEvent { uint8_t type = 0; uint8_t note = 0; float value = 0.0f; };
    enum class GuiMidi : uint8_t { on, off, bend, modWheel, allNotesOff };
    static constexpr int kGuiFifoSize = 256;
    juce::AbstractFifo guiFifo { kGuiFifoSize };
    GuiMidiEvent       guiEvents[kGuiFifoSize];
    void pushGui(GuiMidiEvent e) noexcept;     // message thread
    void drainGuiMidi() noexcept;              // audio thread, top of processBlock

    void buildPatch(dreamer::DreamPatch& p) const;

    // ---- D6 MIDI-learn state -----------------------------------------------
    // CC (0..127) -> flat parameter index (-1 = unmapped). Written on the
    // message thread (learn/clear/state-load) and the audio thread (learn
    // capture); atomic ints keep it lock-free. midiLearnTarget is the param
    // index armed for the next incoming CC (-1 = not learning).
    std::atomic<int> ccToParam[128];
    std::atomic<int> midiLearnTarget { -1 };
    std::vector<juce::RangedAudioParameter*> paramByIndex;   // flat index -> param
    int  paramIndexForId(const juce::String& id) const;      // -1 if not found
    void applyParamMap(const std::vector<std::pair<juce::String, juce::var>>& values);
    juce::DynamicObject::Ptr captureParamMap() const;        // current APVTS -> JSON map
    juce::File userPresetDir() const;                        // ~/The Dreamer/Presets

    // ---- factory preset bank (parsed once from BinaryData::presets_json) -----
    // Values are stored exactly as the JSON holds them: normalized 0..1 for all
    // Float/Int params, integer choice index for AudioParameterChoice, and
    // true/false for AudioParameterBool. applyPreset() converts per PARAM TYPE
    // (not per assumed range), so the encoding is robust regardless of a param's
    // native range. See PluginProcessor.cpp.
    struct Preset {
        juce::String name, category;
        std::vector<std::pair<juce::String, juce::var>> values;
    };
    std::vector<Preset> presets;
    int  currentProgram = 0;
    juce::String loadedUserPresetName;   // TD-010 (message thread; "" = factory/INIT)
    void loadFactoryPresets();   // construction only; never on the audio thread

    dreamer::DreamSynth   synth;
    int lastOversample = 0;

    dreamer::GlobalFilter f1[2], f2[2];         // [L, R] per slot
    dreamer::DCBlocker    dcBlock[2];
    dreamer::Ensemble     ensemble;
    dreamer::Dimension    dimension;
    dreamer::Rotary       rotary;
    dreamer::Barberpole   barberpole;
    dreamer::StereoDelay  delay;
    dreamer::ModDelayFx   chorus, flanger;
    dreamer::Phaser       phaser;
    juce::Reverb          reverb;
    dreamer::LoFi         lofi;
    dreamer::StereoWidth  stereoWidth;
    dreamer::Talkbox      talkbox;
    dreamer::OutputStage  outputStage;          // GAIN_STAGING s5
    std::vector<float>    reverbWetL, reverbWetR;
    struct RevCache { int type = -1; float size = -1.0f, damp = -1.0f; } revCache;
    bool reverbWasActive = false;
    int  gfCtrl = 0;

    // FX PARAMS extras, one-pole smoothed per block (continuous extras;
    // discrete ones snap + de-click inside their effect). [slot][0..3]
    float fxP_[3][4] = {};
    float fxSmoothCoef_ = 1.0f;

    // output metering feed to the editor (peak-hold in the GUI)
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    std::atomic<float> limiterGR { 0.0f };   // D12 output-stage GR in dB

    // D13 panic: GUI button / NaN-recovery -> flush voices + FX on the audio
    // thread (never mutate DSP state from the message thread). Consumed at the
    // top of processBlock.
    std::atomic<bool> panicRequested { false };
    void flushFx() noexcept;                 // kill FX tails (audio thread)

    // D3 analyzer tap: lock-free ring of the FINAL output (post-master/limiter),
    // written per-sample on the audio thread, snapshotted by the editor ~20 fps.
    // Single producer (audio) / single consumer (GUI); a torn read only shows a
    // slightly stale scope, never UB.
    static constexpr int kScopeSize = 2048;   // power of two
    std::array<float, (size_t)kScopeSize> scopeBuf {};
    std::atomic<uint32_t> scopeWrite { 0 };

    juce::SmoothedValue<float> masterSmoothed;

    struct LfoPtrs { std::atomic<float> *rate, *depth, *sync, *dest, *shape; };
    struct TonePtrs {
        std::atomic<float> *wave, *on, *level, *oct, *semi, *fine, *start, *startRandom,
                           *velo, *pan, *shape, *shapeDepth, *noise, *noiseColor,
                           *dir, *vint,
                           *detuneVoices, *detuneAmount,          // D9
                           *voicing, *dreamySpread, *loopMode,     // s11/s12
                           *hitPlay, *hitStretch, *hitPitchTrim,   // s13
                           *loopRate, *loopRateSync, *loopRateBeats, *loopVarispeed, // D15
                           *auxDest, *auxAmt,
                           *tvfType, *tvfCut, *tvfRes, *tvfEnv, *tvfKf,
                           *tvfA, *tvfD, *tvfS, *tvfR,
                           *tvaA, *tvaD, *tvaS, *tvaR,
                           *auxA, *auxD, *auxS, *auxR;
        LfoPtrs lfo[2];
    };
    TonePtrs pTone[4] {};

    std::atomic<float> *pMaster,
                       *pVecPhase, *pVecOrbitOn, *pVecOrbitRate, *pVecOrbitShape,
                       *pVecOrbitVoice, *pVecPenvOn, *pVecPenvStart, *pVecPenvEnd,
                       *pVecPenvTime, *pVecPenvLoop,
                       *pFltRoute, *pFltBal,
                       *pFlt1Type, *pFlt1Cut, *pFlt1Res, *pFlt1Env,
                       *pFlt2Type, *pFlt2Cut, *pFlt2Res, *pFlt2Morph,
                       *pLfoRate, *pLfoShape,
                       *pMtxSrc[3], *pMtxDst[3], *pMtxAmt[3],
                       *pModfxType, *pModfxRate, *pModfxDepth, *pModfxMix, *pModfxOn,
                       *pDlyMode, *pDlyTime, *pDlyFb, *pDlyMix, *pDlyOn, *pDlySync,
                       *pRevType, *pRevSize, *pRevDamp, *pRevMix, *pRevOn,
                       *pModfxP[4], *pDlyP[4], *pRevP[4],
                       *pLofiOn, *pLofiBits, *pLofiSrate, *pLofiCompand, *pLofiAlias,
                       *pWidthOn, *pWidth, *pWidthHaas, *pWidthBassMono,
                       *pTalkOn, *pTalkVa, *pTalkVb, *pTalkMorph, *pTalkSens,
                       *pFxPrePost,
                       *pDrift, *pInterp, *pEngine,
                       // UX round: D5 global offsets, D8 g-octave, D12 limiter
                       *pGEnvA, *pGEnvD, *pGEnvS, *pGEnvR, *pGCutoff, *pGRes,
                       *pGOctave, *pLimiterOn,
                       // v15 GUI: F2 env, global LFO sync, FX focus-shadow model
                       *pFlt2Env, *pLfoSync,
                       *pLfo2Rate, *pLfo2Shape, *pLfo2Sync,   // v16 GLOBAL LFO 2
                       *pModfxPFocus, *pDlyPFocus, *pRevPFocus,
                       *pModfxParam, *pDlyParam, *pRevParam,
                       *pLofiParam, *pLofiPFocus, *pTalkParam, *pTalkPFocus;

    void cacheTonePtrs(TonePtrs& dst, int toneIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
