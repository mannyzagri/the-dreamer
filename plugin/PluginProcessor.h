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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // output metering feed for the editor's header L/R meters (peak per block)
    float getMeterL() const noexcept { return meterL.load(std::memory_order_relaxed); }
    float getMeterR() const noexcept { return meterR.load(std::memory_order_relaxed); }

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

    juce::SmoothedValue<float> masterSmoothed;

    struct LfoPtrs { std::atomic<float> *rate, *depth, *sync, *dest, *shape; };
    struct TonePtrs {
        std::atomic<float> *wave, *on, *level, *oct, *fine, *start, *startRandom,
                           *velo, *pan, *shape, *shapeDepth, *noise, *noiseColor,
                           *dir, *vint, *auxDest, *auxAmt,
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
                       *pDrift, *pInterp, *pEngine;

    void cacheTonePtrs(TonePtrs& dst, int toneIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
