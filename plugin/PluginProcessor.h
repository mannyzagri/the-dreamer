// The Dreamer -- 24-voice dual-partial 90s ROMpler, VST3.
// Voice: dsp/glue/DreamVoice.h (verified bank topology + ported Rhino filter
// + 2 LFOs/partial). FX bus: the full Rubber-Rhino chain, mirrored from its
// processBlock (dist -> dcblock -> [vintage: 16-bit truncate] -> volume ->
// delay(+HP/LP returns) -> chorus -> flanger -> phaser -> reverb(+returns) ->
// comp -> hiss -> output -> brickwall).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/glue/DreamVoice.h"
#include "dsp/ported/fx/Effects.h"
#include "dsp/ported/fx/ReturnFilter.h"
#include "dsp/ported/fx/ModFx.h"
#include "dsp/ported/fx/SpringReverb.h"
#include "dsp/ported/fx/Instability.h"
#include "dsp/ported/fx/Dynamics.h"

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
        float value = 0.0f;   // velocity 0..1 / bend semis / sustain 0|1
    };
    static constexpr int kMaxEvents = 512;
    NoteEvent events[kMaxEvents];
    int numEvents = 0;
    void pushEvent(const NoteEvent& e) { if (numEvents < kMaxEvents) events[numEvents++] = e; }

    void buildPatch(dreamer::DreamPatch& p) const;

    // ---- engine -------------------------------------------------------------
    dreamer::DreamSynth synth;
    int lastOversample = 0;

    // ---- FX bus (Rubber-Rhino chain) ---------------------------------------
    dreamer::Distortion   dist;
    dreamer::DCBlocker    dcBlock;
    dreamer::StereoDelay  delay;
    dreamer::ReturnFilter delayHP, delayLP, reverbHP, reverbLP;
    dreamer::ModDelayFx   chorus, flanger;
    dreamer::Phaser       phaser;
    juce::Reverb          reverb;          // the one deliberate JUCE DSP dependency
    dreamer::SpringReverb springReverb;
    dreamer::Instability  instability;
    dreamer::CompLimiter  comp;

    std::vector<float> reverbWetL, reverbWetR;
    struct RevCache { int type = -1; float size = -1.0f, damp = -1.0f; } revCache;
    bool reverbWasActive = false;

    juce::SmoothedValue<float> volumeSmoothed, outputSmoothed;

    // ---- cached raw parameter pointers -------------------------------------
    struct LfoPtrs { std::atomic<float> *shape, *rate, *depth, *dest, *keytrig; };
    struct PartialPtrs {
        std::atomic<float> *on, *wave, *coarse, *fine, *level, *velsens, *start,
                           *filterType, *cutoff, *reso, *envAmt, *keyfollow,
                           *tvfA, *tvfD, *tvfS, *tvfR, *tvaA, *tvaD, *tvaS, *tvaR;
        LfoPtrs lfo[2];
    };
    PartialPtrs pA {}, pB {};

    std::atomic<float> *pInterp, *pEngine, *pVolume, *pOutput,
                       *pCondition, *pStability, *pTempo,
                       *pDistType, *pDistDrive, *pDistColor, *pDistMix,
                       *pDelayType, *pDelayMode, *pDelayMs, *pDelayFb, *pDelayMix,
                       *pDelayHpSlope, *pDelayHpFreq, *pDelayLpSlope, *pDelayLpFreq,
                       *pChorusOn, *pChorusDepth, *pChorusRate,
                       *pFlangerOn, *pFlangerDepth, *pFlangerRate,
                       *pPhaserOn, *pPhaserDepth, *pPhaserRate,
                       *pRevType, *pRevSize, *pRevDamp, *pRevMix,
                       *pReverbHpSlope, *pReverbHpFreq, *pReverbLpSlope, *pReverbLpFreq,
                       *pCompOn, *pCompLim, *pCompThresh, *pCompRatio, *pCompGain,
                       *pClipOn;

    void cachePartialPtrs(PartialPtrs& dst, const char* prefix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheDreamerProcessor)
};
