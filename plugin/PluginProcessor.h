// The Dreamer v2 -- 24-voice 4-tone vector ROMpler, VST3 (FEATURES.md CORE).
// Engine: dsp/glue/DreamVoice.h (4 tones: PcmOscillator -> Waveshaper ->
// ToneSvf -> TVA, AUX env, G-LFO taps, Dream Vector v4, pan; single global
// LFO; 3-slot mod matrix). Bus per FEATURES section 5/8:
//   stereo tone sum -> GLOBAL FILTERS 1/2 (SER/PAR) -> [vintage Truncate16]
//   -> volume -> MOD FX (chorus|flanger|phaser) -> DELAY (mono-sum in,
//   stereo wet) -> REVERB (room/hall/plate) -> output.
// Rubber-Rhino stages beyond the panel (dist, comp, clip, instability,
// spring) run at their bit-transparent defaults and are OMITTED (documented).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/glue/DreamVoice.h"
#include "dsp/glue/GlobalFilter.h"
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

    // ---- engine -------------------------------------------------------------
    dreamer::DreamSynth   synth;
    int lastOversample = 0;

    // ---- bus ---------------------------------------------------------------
    dreamer::GlobalFilter f1[2], f2[2];         // [L, R] per slot
    dreamer::DCBlocker    dcBlock[2];
    dreamer::StereoDelay  delay;
    dreamer::ModDelayFx   chorus, flanger;
    dreamer::Phaser       phaser;
    juce::Reverb          reverb;
    std::vector<float>    reverbWetL, reverbWetR;
    struct RevCache { int type = -1; float size = -1.0f, damp = -1.0f; } revCache;
    bool reverbWasActive = false;
    int  gfCtrl = 0;                            // global-filter control counter

    juce::SmoothedValue<float> volumeSmoothed, outputSmoothed;

    // ---- cached raw parameter pointers -------------------------------------
    struct TonePtrs {
        std::atomic<float> *on, *wave, *coarse, *fine, *level, *velsens, *start,
                           *pan, *dir, *vint, *shapeType, *shapeDepth,
                           *tvfType, *cutoff, *reso, *envAmt, *keyfollow,
                           *tvfA, *tvfD, *tvfS, *tvfR, *tvaA, *tvaD, *tvaS, *tvaR,
                           *auxA, *auxD, *auxS, *auxR, *auxDest, *auxAmt,
                           *lfoDepth, *lfoDest;
    };
    TonePtrs pTone[4] {};

    std::atomic<float> *pInterp, *pEngine, *pVolume, *pOutput,
                       *pVecPhase, *pOrbitOn, *pOrbitRate,
                       *pPenvOn, *pPenvLoop, *pPenvStart, *pPenvEnd, *pPenvTime,
                       *pGlfoShape, *pGlfoRate,
                       *pMtxSrc[3], *pMtxDest[3], *pMtxAmt[3],
                       *pF1Type, *pF1Cutoff, *pF1Reso, *pF1Env,
                       *pF2Type, *pF2Cutoff, *pF2Reso, *pF2Morph,
                       *pFiltRouting,
                       *pModfxOn, *pModfxType, *pModfxRate, *pModfxDepth, *pModfxMix,
                       *pDelayOn, *pDelayMode, *pDelayTime, *pDelayFb, *pDelayMix,
                       *pRevOn, *pRevType, *pRevSize, *pRevDamp, *pRevMix;

    void cacheTonePtrs(TonePtrs& dst, const char* prefix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
