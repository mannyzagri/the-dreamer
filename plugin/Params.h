// Params.h -- The Dreamer v2 APVTS layout (FEATURES.md CORE + GUI v4).
// Parameter IDs are LOCKED for the WebView GUI handoff (bind by ID).
// Per-tone blocks use prefixes a_/b_/c_/d_ (panel TONE A-D).
//
// Choice-count freezes (Cubase save-compat -- never reorder/resize):
//   wave: 78 bank entries              filter type: FULL 14-entry list
//   shaper: OFF + 5 tables             tvf type: LP24/LP12/BP/HP
//   matrix src: 6, dest: 8             glfo shape: 5 (panel TRI/SIN/SAW/SQR/S+H)
//
// FX are the panel subset (FEATURES section 8): MOD FX -> DELAY -> REVERB.
// Rubber-Rhino stages beyond the panel run at fixed bit-transparent defaults
// and are omitted from the processor (documented in PROJECT-NOTES).

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/bank/RomplerBank.h"

namespace dreamer::params {

// ---- global ----------------------------------------------------------------
inline constexpr auto kInterp   = "interp";
inline constexpr auto kEngine   = "engine";      // Vintage / Modern
inline constexpr auto kVolume   = "volume";      // master, pre-FX
inline constexpr auto kOutput   = "output";      // post-FX

// ---- Dream Vector ----------------------------------------------------------
inline constexpr auto kVecPhase   = "vec_phase";
inline constexpr auto kOrbitOn    = "orbit_on";
inline constexpr auto kOrbitRate  = "orbit_rate";
inline constexpr auto kPenvOn     = "penv_on";
inline constexpr auto kPenvLoop   = "penv_loop";
inline constexpr auto kPenvStart  = "penv_start";
inline constexpr auto kPenvEnd    = "penv_end";
inline constexpr auto kPenvTime   = "penv_time";

// ---- global LFO ------------------------------------------------------------
inline constexpr auto kGlfoShape = "glfo_shape";
inline constexpr auto kGlfoRate  = "glfo_rate";

// ---- global filters --------------------------------------------------------
inline constexpr auto kF1Type    = "f1_type";
inline constexpr auto kF1Cutoff  = "f1_cutoff";
inline constexpr auto kF1Reso    = "f1_reso";
inline constexpr auto kF1Env     = "f1_env";     // GUI: F1 ENV knob -- voice-max AUX -> cutoff, +/-2 oct
inline constexpr auto kF2Type    = "f2_type";
inline constexpr auto kF2Cutoff  = "f2_cutoff";
inline constexpr auto kF2Reso    = "f2_reso";
inline constexpr auto kF2Morph   = "f2_morph";   // GUI: F2 MORPH knob -- reserved (FORMANT/DREAMPLN), inert in v1
inline constexpr auto kFiltRouting = "filt_routing";   // SER / PAR

// ---- FX --------------------------------------------------------------------
inline constexpr auto kModfxOn    = "modfx_on";
inline constexpr auto kModfxType  = "modfx_type";      // CHORUS / FLANGER / PHASER
inline constexpr auto kModfxRate  = "modfx_rate";
inline constexpr auto kModfxDepth = "modfx_depth";
inline constexpr auto kModfxMix   = "modfx_mix";
inline constexpr auto kDelayOn    = "delay_on";
inline constexpr auto kDelayMode  = "delay_mode";      // DIGITAL / TAPE / PONG
inline constexpr auto kDelayTime  = "delay_time";
inline constexpr auto kDelayFb    = "delay_fb";
inline constexpr auto kDelayMix   = "delay_mix";
inline constexpr auto kRevOn      = "rev_on";
inline constexpr auto kRevType    = "rev_type";        // ROOM / HALL / PLATE
inline constexpr auto kRevSize    = "rev_size";
inline constexpr auto kRevDamp    = "rev_damp";
inline constexpr auto kRevMix     = "rev_mix";

inline juce::String pid(const char* prefix, const juce::String& suffix) {
    return juce::String(prefix) + suffix;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto zeroToOne = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(0.0f, 1.0f, 0.001f), def);
    };
    auto bipolar = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def);
    };
    auto msTime = [](const String& id, const String& name, float maxMs, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name,
            NormalisableRange<float>(1.0f, maxMs, 0.1f, 0.3f), def,
            AudioParameterFloatAttributes().withLabel("ms"));
    };
    auto cutoffHz = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name,
            NormalisableRange<float>(60.0f, 12000.0f, 0.1f, 0.25f), def,
            AudioParameterFloatAttributes().withLabel("Hz"));
    };

    StringArray waves;
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        waves.add(String(rompler::bank::kWaveforms[i].category) + ": "
                  + rompler::bank::kWaveforms[i].name);

    const StringArray shaperTypes { "Off", "Soft Fold", "Hard Fold", "Sine Fold",
                                    "Asym", "Drive" };
    const StringArray tvfTypes    { "LP 24", "LP 12", "BP", "HP" };
    const StringArray filterTypes { "LP 24", "LP 12", "BP", "HP", "Liquid",
                                    "Classic", "Ladder", "Notch", "Comb +",
                                    "Comb -", "NL3 N+LP", "Formant", "Allpass",
                                    "DreamPln" };
    const StringArray glfoShapes  { "Tri", "Sin", "Saw", "Sqr", "S+H" };
    const StringArray auxDests    { "Pitch", "Start", "Shape", "Pan" };
    const StringArray lfoDests    { "Pitch", "Cutoff", "Shape", "Level" };
    // GUI handoff order, no "-" entry: a slot is inert at amt = 0.
    // (Engine enums keep index 0 = none; the processor maps param + 1.)
    const StringArray mtxSrcs     { "G-LFO", "Vec Phs", "Aux", "Velo", "Wheel" };
    const StringArray mtxDests    { "Pitch", "Cut 1", "Cut 2", "Morph",
                                    "Shape", "Vec Phs", "Pan" };

    // ---- tone blocks (A-D) --------------------------------------------------
    const char* prefixes[4] = { "a_", "b_", "c_", "d_" };
    const char* toneNames[4] = { "A ", "B ", "C ", "D " };
    for (int t = 0; t < 4; ++t) {
        const char* px = prefixes[t];
        const String P = toneNames[t];

        layout.add(std::make_unique<AudioParameterBool>(
            ParameterID { pid(px, "on"), 1 }, P + "On", t == 0));
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "wave"), 1 }, P + "Wave", waves, 0));
        layout.add(std::make_unique<AudioParameterInt>(
            ParameterID { pid(px, "coarse"), 1 }, P + "Coarse", -24, 24, 0));
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { pid(px, "fine"), 1 }, P + "Fine",
            NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel("ct")));
        layout.add(zeroToOne(pid(px, "level"),   P + "Level",    0.8f));
        layout.add(zeroToOne(pid(px, "velsens"), P + "Vel Sens", 0.5f));
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { pid(px, "start"), 1 }, P + "Sample Start",
            NormalisableRange<float>(0.0f, 599.0f, 1.0f), 0.0f));
        layout.add(bipolar(pid(px, "pan"), P + "Pan", 0.0f));

        layout.add(zeroToOne(pid(px, "dir"), P + "Vec Dir", 0.25f * (float)t));
        layout.add(zeroToOne(pid(px, "int"), P + "Vec Int", 0.0f));

        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "shape_type"), 1 }, P + "Shape Type", shaperTypes, 0));
        layout.add(zeroToOne(pid(px, "shape_depth"), P + "Shape Depth", 0.0f));

        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "tvf_type"), 1 }, P + "TVF Type", tvfTypes, 0));
        layout.add(cutoffHz(pid(px, "cutoff"), P + "Cutoff", 8000.0f));
        layout.add(zeroToOne(pid(px, "reso"), P + "Resonance", 0.0f));
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { pid(px, "env_amt"), 1 }, P + "TVF Env Amt",
            NormalisableRange<float>(-12000.0f, 12000.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        layout.add(zeroToOne(pid(px, "keyfollow"), P + "Key Follow", 0.5f));

        layout.add(msTime(pid(px, "tvf_att"), P + "TVF Attack",  5000.0f, 5.0f));
        layout.add(msTime(pid(px, "tvf_dec"), P + "TVF Decay",   8000.0f, 300.0f));
        layout.add(zeroToOne(pid(px, "tvf_sus"), P + "TVF Sustain", 1.0f));
        layout.add(msTime(pid(px, "tvf_rel"), P + "TVF Release", 8000.0f, 300.0f));
        layout.add(msTime(pid(px, "tva_att"), P + "TVA Attack",  5000.0f, 5.0f));
        layout.add(msTime(pid(px, "tva_dec"), P + "TVA Decay",   8000.0f, 300.0f));
        layout.add(zeroToOne(pid(px, "tva_sus"), P + "TVA Sustain", 1.0f));
        layout.add(msTime(pid(px, "tva_rel"), P + "TVA Release", 8000.0f, 300.0f));
        layout.add(msTime(pid(px, "aux_att"), P + "Aux Attack",  5000.0f, 5.0f));
        layout.add(msTime(pid(px, "aux_dec"), P + "Aux Decay",   8000.0f, 300.0f));
        layout.add(zeroToOne(pid(px, "aux_sus"), P + "Aux Sustain", 1.0f));
        layout.add(msTime(pid(px, "aux_rel"), P + "Aux Release", 8000.0f, 300.0f));
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "aux_dest"), 1 }, P + "Aux Dest", auxDests, 0));
        layout.add(bipolar(pid(px, "aux_amt"), P + "Aux Amt", 0.0f));

        layout.add(zeroToOne(pid(px, "lfo_depth"), P + "LFO Depth", 0.0f));
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "lfo_dest"), 1 }, P + "LFO Dest", lfoDests, 0));
    }

    // ---- Dream Vector globals ----------------------------------------------
    layout.add(zeroToOne(kVecPhase, "Vec Phase", 0.0f));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kOrbitOn, 1 }, "Orbit On", false));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kOrbitRate, 1 }, "Orbit Rate",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kPenvOn, 1 }, "P-Env On", false));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kPenvLoop, 1 }, "P-Env Loop", false));
    layout.add(zeroToOne(kPenvStart, "P-Env Start", 0.0f));
    layout.add(zeroToOne(kPenvEnd,   "P-Env End",   0.5f));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kPenvTime, 1 }, "P-Env Time",
        NormalisableRange<float>(0.01f, 10.0f, 0.01f, 0.4f), 1.0f,
        AudioParameterFloatAttributes().withLabel("s")));

    // ---- global LFO ---------------------------------------------------------
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kGlfoShape, 1 }, "G-LFO Shape", glfoShapes, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kGlfoRate, 1 }, "G-LFO Rate",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));

    // ---- mod matrix ---------------------------------------------------------
    for (int i = 1; i <= 3; ++i) {
        const String n(i);
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { "mod" + n + "_src", 1 }, "Mod " + n + " Src", mtxSrcs, 0));
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { "mod" + n + "_dest", 1 }, "Mod " + n + " Dest", mtxDests, 0));
        layout.add(bipolar("mod" + n + "_amt", "Mod " + n + " Amt", 0.0f));
    }

    // ---- global filters -----------------------------------------------------
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kF1Type, 1 }, "Filter 1 Type", filterTypes, 0));
    layout.add(cutoffHz(kF1Cutoff, "Filter 1 Cutoff", 12000.0f));
    layout.add(zeroToOne(kF1Reso, "Filter 1 Reso", 0.0f));
    layout.add(bipolar(kF1Env, "Filter 1 Env", 0.0f));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kF2Type, 1 }, "Filter 2 Type", filterTypes, 0));
    layout.add(cutoffHz(kF2Cutoff, "Filter 2 Cutoff", 12000.0f));
    layout.add(zeroToOne(kF2Reso, "Filter 2 Reso", 0.0f));
    layout.add(zeroToOne(kF2Morph, "Filter 2 Morph", 0.0f));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kFiltRouting, 1 }, "Filter Routing", StringArray { "Ser", "Par" }, 0));

    // ---- FX -----------------------------------------------------------------
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kModfxOn, 1 }, "Mod FX On", false));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kModfxType, 1 }, "Mod FX Type",
        StringArray { "Chorus", "Flanger", "Phaser" }, 0));
    layout.add(zeroToOne(kModfxRate,  "Mod FX Rate",  0.3f));
    layout.add(zeroToOne(kModfxDepth, "Mod FX Depth", 0.5f));
    layout.add(zeroToOne(kModfxMix,   "Mod FX Mix",   0.5f));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kDelayOn, 1 }, "Delay On", false));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kDelayMode, 1 }, "Delay Mode",
        StringArray { "Digital", "Tape", "Pong" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kDelayTime, 1 }, "Delay Time",
        NormalisableRange<float>(1.0f, 1000.0f, 1.0f, 0.4f), 250.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kDelayFb, 1 }, "Delay Feedback",
        NormalisableRange<float>(0.0f, 0.9f, 0.001f), 0.35f));
    layout.add(zeroToOne(kDelayMix, "Delay Mix", 0.0f));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kRevOn, 1 }, "Reverb On", false));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kRevType, 1 }, "Reverb Type",
        StringArray { "Room", "Hall", "Plate" }, 0));
    layout.add(zeroToOne(kRevSize, "Reverb Size", 0.5f));
    layout.add(zeroToOne(kRevDamp, "Reverb Damp", 0.5f));
    layout.add(zeroToOne(kRevMix,  "Reverb Mix",  0.0f));

    // ---- global -------------------------------------------------------------
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kInterp, 1 }, "Interpolation",
        StringArray { "Drop Sample", "Linear" }, 1));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kEngine, 1 }, "Engine", StringArray { "Vintage", "Modern" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kVolume, 1 }, "Volume",
        NormalisableRange<float>(-60.0f, 6.0f, 0.1f), -6.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kOutput, 1 }, "Output",
        NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

} // namespace dreamer::params
