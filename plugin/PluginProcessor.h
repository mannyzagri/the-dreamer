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

    void buildPatch(dreamer::DreamPatch& p) const;

    dreamer::DreamSynth   synth;
    int lastOversample = 0;

    dreamer::GlobalFilter f1[2], f2[2];         // [L, R] per slot
    dreamer::DCBlocker    dcBlock[2];
    dreamer::Ensemble     ensemble;
    dreamer::StereoDelay  delay;
    dreamer::ModDelayFx   chorus, flanger;
    dreamer::Phaser       phaser;
    juce::Reverb          reverb;
    std::vector<float>    reverbWetL, reverbWetR;
    struct RevCache { int type = -1; float size = -1.0f, damp = -1.0f; } revCache;
    bool reverbWasActive = false;
    int  gfCtrl = 0;

    juce::SmoothedValue<float> masterSmoothed;

    struct LfoPtrs { std::atomic<float> *rate, *depth, *sync, *dest; };
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
                       *pDlyMode, *pDlyTime, *pDlyFb, *pDlyMix, *pDlyOn,
                       *pRevType, *pRevSize, *pRevDamp, *pRevMix, *pRevOn,
                       *pDrift, *pInterp, *pEngine;

    void cacheTonePtrs(TonePtrs& dst, int toneIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
