// Params.h -- The Dreamer canonical APVTS layout per DSP_BUILD.md section 9
// (the contract with GUI_SPEC.md). IDs and ranges follow the table verbatim;
// per-tone params use suffixes _a.._d. Do not rename/renumber/re-range
// without flagging.
//
// FLAGGED additions beyond the section-9 table (each user-approved or
// spec-implied elsewhere, documented in PROJECT-NOTES):
//   aux_amt_[t]     -1..+1  aux env depth (section-9 omission; approved)
//   vec_penv_loop   bool    P-ENV loop flag (section 6 + GUI_SPEC name it)
//   vec_orbit_voice bool    per-voice free-run (section 6 "non-panel param")
//   interp, engine  choices v2 carryovers (DropSample/Linear, Vintage/Modern)
// FLAGGED interpretation: tvf_env / flt1_env are UNIPOLAR 0..1 per the
// table (upward-only env sweep); negative env is not reachable.
//
// Normalized 0..1 -> real-unit maps live in PluginProcessor.cpp (documented
// there); GUI displays 0-127 (bipolar -63..+63) regardless.
//
// Choice-count freezes: wave 104 (78 cycle + 16 Ens + 10 Shot, bank3 order);
// flt type 14; shaper 6; tvf type 4; lfo/orbit shapes 5; mtx src 5, dst 8;
// modfx 4; dly 3; rev 3.

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/bank/Bank3.h"

namespace dreamer::params {

// ---- global ----------------------------------------------------------------
inline constexpr auto kMaster        = "master";
inline constexpr auto kVecPhase      = "vec_phase";
inline constexpr auto kVecOrbitOn    = "vec_orbit_on";
inline constexpr auto kVecOrbitRate  = "vec_orbit_rate";
inline constexpr auto kVecOrbitShape = "vec_orbit_shape";
inline constexpr auto kVecOrbitVoice = "vec_orbit_voice";   // flagged (s6)
inline constexpr auto kVecPenvOn     = "vec_penv_on";
inline constexpr auto kVecPenvStart  = "vec_penv_start";
inline constexpr auto kVecPenvEnd    = "vec_penv_end";
inline constexpr auto kVecPenvTime   = "vec_penv_time";
inline constexpr auto kVecPenvLoop   = "vec_penv_loop";     // flagged (s6)
inline constexpr auto kFltRoute      = "flt_route";
inline constexpr auto kFltBal        = "flt_bal";    // v7: filter 1<->2 balance
inline constexpr auto kFlt1Type      = "flt1_type";
inline constexpr auto kFlt1Cut       = "flt1_cut";
inline constexpr auto kFlt1Res       = "flt1_res";
inline constexpr auto kFlt1Env       = "flt1_env";
inline constexpr auto kFlt2Type      = "flt2_type";
inline constexpr auto kFlt2Cut       = "flt2_cut";
inline constexpr auto kFlt2Res       = "flt2_res";
inline constexpr auto kFlt2Morph     = "flt2_morph";
inline constexpr auto kLfoRate       = "lfo_rate";
inline constexpr auto kLfoShape      = "lfo_shape";
inline constexpr auto kModfxType     = "modfx_type";
inline constexpr auto kModfxRate     = "modfx_rate";
inline constexpr auto kModfxDepth    = "modfx_depth";
inline constexpr auto kModfxMix      = "modfx_mix";
inline constexpr auto kModfxOn       = "modfx_on";
inline constexpr auto kDlyMode       = "dly_mode";
inline constexpr auto kDlyTime       = "dly_time";
inline constexpr auto kDlyFb         = "dly_fb";
inline constexpr auto kDlyMix        = "dly_mix";
inline constexpr auto kDlyOn         = "dly_on";
inline constexpr auto kRevType       = "rev_type";
inline constexpr auto kRevSize       = "rev_size";
inline constexpr auto kRevDamp       = "rev_damp";
inline constexpr auto kRevMix        = "rev_mix";
inline constexpr auto kRevOn         = "rev_on";
// FX v1.5 (FEATURES.md 11): per-slot PARAMS (4 extras p0..p3 + focus),
// standalone LO-FI / WIDTH / TALK stages, and the LO-FI PRE/POST switch.
inline constexpr auto kModfxPFocus   = "modfx_pfocus";
inline constexpr auto kDlyPFocus     = "dly_pfocus";
inline constexpr auto kRevPFocus     = "rev_pfocus";
inline constexpr auto kLofiOn        = "lofi_on";
inline constexpr auto kLofiBits      = "lofi_bits";
inline constexpr auto kLofiSrate     = "lofi_srate";
inline constexpr auto kLofiCompand   = "lofi_compand";
inline constexpr auto kLofiAlias     = "lofi_alias";
inline constexpr auto kWidthOn       = "width_on";
inline constexpr auto kWidth         = "width";
inline constexpr auto kWidthHaas     = "width_haas";
inline constexpr auto kWidthBassMono = "width_bassmono";
inline constexpr auto kTalkOn        = "talk_on";
inline constexpr auto kTalkVa        = "talk_va";
inline constexpr auto kTalkVb        = "talk_vb";
inline constexpr auto kTalkMorph     = "talk_morph";
inline constexpr auto kTalkSens      = "talk_sens";
inline constexpr auto kFxPrePost     = "fx_prepost";   // governs LO-FI placement (WIDTH fixed POST)
inline constexpr auto kDrift         = "drift";
inline constexpr auto kInterp        = "interp";            // flagged carryover
inline constexpr auto kEngine        = "engine";            // flagged carryover

inline juce::String tid(const char* base, int t) {          // per-tone id
    static const char* sfx[4] = { "_a", "_b", "_c", "_d" };
    return juce::String(base) + sfx[t];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto uni = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(0.0f, 1.0f, 0.001f), def);
    };
    auto bip = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def);
    };
    auto boolean = [](const String& id, const String& name, bool def) {
        return std::make_unique<AudioParameterBool>(ParameterID { id, 1 }, name, def);
    };
    auto choice = [](const String& id, const String& name, const StringArray& items, int def) {
        return std::make_unique<AudioParameterChoice>(ParameterID { id, 1 }, name, items, def);
    };

    // wave list from bank3 (104, order LOCKED)
    StringArray waves;
    for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i)
        waves.add(String(rompler::bank3::kWaveforms[(size_t)i].category) + ": "
                  + rompler::bank3::kWaveforms[(size_t)i].name);

    const StringArray shaperTypes { "Off", "Soft Fold", "Hard Fold", "Sine Fold",
                                    "Asym", "Drive" };
    const StringArray tvfTypes    { "LP 24", "LP 12", "BP", "HP" };
    const StringArray filterTypes { "LP 24", "LP 12", "BP", "HP", "Liquid",
                                    "Classic", "Ladder", "Notch", "Comb +",
                                    "Comb -", "NL3 N+LP", "Formant", "Allpass",
                                    "DreamPln" };
    const StringArray lfoShapes   { "Tri", "Sin", "Saw", "Sqr", "S+H" };
    const StringArray orbitShapes { "Saw", "Tri", "Sin", "Sqr", "S+H" };
    const StringArray auxDests    { "Pitch", "Start", "Shape", "Pan", "Noise" };
    const StringArray lfoDests    { "Pitch", "Cutoff", "Shape", "Level" };
    const StringArray mtxSrcs     { "G-LFO", "Vec Phs", "Aux", "Velo", "Wheel" };
    const StringArray mtxDests    { "Pitch", "Cut 1", "Cut 2", "Morph",
                                    "Shape", "Vec Phs", "Pan", "Noise" };

    // ---- per-tone blocks (suffix _a.._d) -----------------------------------
    const char* toneNames[4] = { "A ", "B ", "C ", "D " };
    for (int t = 0; t < 4; ++t) {
        const String P = toneNames[t];
        layout.add(choice(tid("wave", t), P + "Wave", waves, 0));
        layout.add(boolean(tid("on", t), P + "On", t == 0));
        layout.add(uni(tid("level", t), P + "Level", 0.8f));
        layout.add(std::make_unique<AudioParameterInt>(
            ParameterID { tid("oct", t), 1 }, P + "Octave", -2, 2, 0));
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { tid("fine", t), 1 }, P + "Fine",
            NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel("ct")));
        layout.add(uni(tid("start", t), P + "Sample Start", 0.0f));
        layout.add(boolean(tid("start_random", t), P + "Start Random", false));
        layout.add(uni(tid("velo", t), P + "Vel Sens", 0.5f));
        layout.add(bip(tid("pan", t), P + "Pan", 0.0f));
        layout.add(choice(tid("shape", t), P + "Shape", shaperTypes, 0));
        layout.add(uni(tid("shape_depth", t), P + "Shape Depth", 0.0f));
        layout.add(uni(tid("noise", t), P + "Noise", 0.0f));
        layout.add(uni(tid("noise_color", t), P + "Noise Color", 0.0f));
        layout.add(uni(tid("dir", t), P + "Vec Dir", 0.25f * (float)t));
        layout.add(uni(tid("vint", t), P + "Vec Int", 0.0f));
        // v7: two per-tone LFOs (rate free-Hz or tempo-synced to 12 divisions)
        for (int lf = 1; lf <= 2; ++lf) {
            const String LB = "lfo" + String(lf) + "_";
            const String LN = P + "LFO " + String(lf) + " ";
            layout.add(uni(tid((LB + "rate").toRawUTF8(), t), LN + "Rate", 0.5f));
            layout.add(uni(tid((LB + "depth").toRawUTF8(), t), LN + "Depth", 0.0f));
            layout.add(boolean(tid((LB + "sync").toRawUTF8(), t), LN + "Sync", false));
            layout.add(choice(tid((LB + "dest").toRawUTF8(), t), LN + "Dest", lfoDests, 0));
        }
        layout.add(choice(tid("aux_dest", t), P + "Aux Dest", auxDests, 0));
        layout.add(bip(tid("aux_amt", t), P + "Aux Amt", 0.0f));   // flagged
        layout.add(choice(tid("tvf_type", t), P + "TVF Type", tvfTypes, 0));
        layout.add(uni(tid("tvf_cut", t), P + "TVF Cutoff", 0.8f));
        layout.add(uni(tid("tvf_res", t), P + "TVF Reso", 0.0f));
        layout.add(uni(tid("tvf_env", t), P + "TVF Env", 0.0f));
        layout.add(uni(tid("tvf_kf", t), P + "TVF KeyFollow", 0.5f));
        layout.add(uni(tid("tvf_a", t), P + "TVF Attack", 0.05f));
        layout.add(uni(tid("tvf_d", t), P + "TVF Decay", 0.35f));
        layout.add(uni(tid("tvf_s", t), P + "TVF Sustain", 1.0f));
        layout.add(uni(tid("tvf_r", t), P + "TVF Release", 0.35f));
        layout.add(uni(tid("tva_a", t), P + "TVA Attack", 0.05f));
        layout.add(uni(tid("tva_d", t), P + "TVA Decay", 0.35f));
        layout.add(uni(tid("tva_s", t), P + "TVA Sustain", 1.0f));
        layout.add(uni(tid("tva_r", t), P + "TVA Release", 0.35f));
        layout.add(uni(tid("aux_a", t), P + "Aux Attack", 0.05f));
        layout.add(uni(tid("aux_d", t), P + "Aux Decay", 0.35f));
        layout.add(uni(tid("aux_s", t), P + "Aux Sustain", 1.0f));
        layout.add(uni(tid("aux_r", t), P + "Aux Release", 0.35f));
    }

    // ---- global ------------------------------------------------------------
    layout.add(uni(kMaster, "Master", 0.78f));
    layout.add(uni(kVecPhase, "Vec Phase", 0.0f));
    layout.add(boolean(kVecOrbitOn, "Orbit On", false));
    layout.add(uni(kVecOrbitRate, "Orbit Rate", 0.3f));
    layout.add(choice(kVecOrbitShape, "Orbit Shape", orbitShapes, 0));
    layout.add(boolean(kVecOrbitVoice, "Orbit Per Voice", false));
    layout.add(boolean(kVecPenvOn, "P-Env On", false));
    layout.add(uni(kVecPenvStart, "P-Env Start", 0.0f));
    layout.add(uni(kVecPenvEnd, "P-Env End", 0.5f));
    layout.add(uni(kVecPenvTime, "P-Env Time", 0.3f));
    layout.add(boolean(kVecPenvLoop, "P-Env Loop", false));

    layout.add(choice(kFltRoute, "Filter Routing", StringArray { "Ser", "Par" }, 0));
    layout.add(bip(kFltBal, "Filter Balance", 0.0f));
    layout.add(choice(kFlt1Type, "Filter 1 Type", filterTypes, 0));
    layout.add(uni(kFlt1Cut, "Filter 1 Cutoff", 1.0f));
    layout.add(uni(kFlt1Res, "Filter 1 Reso", 0.0f));
    layout.add(uni(kFlt1Env, "Filter 1 Env", 0.0f));
    layout.add(choice(kFlt2Type, "Filter 2 Type", filterTypes, 0));
    layout.add(uni(kFlt2Cut, "Filter 2 Cutoff", 1.0f));
    layout.add(uni(kFlt2Res, "Filter 2 Reso", 0.0f));
    layout.add(uni(kFlt2Morph, "Filter 2 Morph", 0.0f));

    layout.add(uni(kLfoRate, "G-LFO Rate", 0.5f));
    layout.add(choice(kLfoShape, "G-LFO Shape", lfoShapes, 0));

    for (int i = 1; i <= 3; ++i) {
        const String n(i);
        layout.add(choice("mtx" + n + "_src", "Mtx " + n + " Src", mtxSrcs, 0));
        layout.add(choice("mtx" + n + "_dst", "Mtx " + n + " Dst", mtxDests, 0));
        layout.add(bip("mtx" + n + "_amt", "Mtx " + n + " Amt", 0.0f));
    }

    // ---- FX slots with the PARAMS model (4 host-automatable extras p0..p3
    //      + a focus selector; the v8 panel binds p0, deeper extras are
    //      automation-only -- FEATURES.md 11.1). pfocus is host-only in v8.
    const StringArray pFocus { "P1", "P2", "P3", "P4" };
    auto slotExtras = [&](const char* prefix, const String& nm) {
        for (int i = 0; i < 4; ++i)
            layout.add(uni(juce::String(prefix) + "_p" + juce::String(i),
                           nm + " Param " + juce::String(i + 1), 0.5f));
    };

    layout.add(choice(kModfxType, "Mod FX Type",
        StringArray { "Chorus", "Flanger", "Phaser", "Ensemble",
                      "Dimension", "Rotary", "Barberpole" }, 0));
    layout.add(uni(kModfxRate,  "Mod FX Rate",  0.3f));
    layout.add(uni(kModfxDepth, "Mod FX Depth", 0.5f));
    layout.add(uni(kModfxMix,   "Mod FX Mix",   0.5f));
    layout.add(boolean(kModfxOn, "Mod FX On", false));
    slotExtras("modfx", "Mod FX");
    layout.add(choice(kModfxPFocus, "Mod FX Focus", pFocus, 0));

    layout.add(choice(kDlyMode, "Delay Mode", StringArray { "Digital", "Tape", "Pong" }, 0));
    layout.add(uni(kDlyTime, "Delay Time", 0.55f));
    layout.add(uni(kDlyFb,   "Delay Feedback", 0.39f));
    layout.add(uni(kDlyMix,  "Delay Mix", 0.0f));
    layout.add(boolean(kDlyOn, "Delay On", false));
    slotExtras("dly", "Delay");
    layout.add(choice(kDlyPFocus, "Delay Focus", pFocus, 0));

    layout.add(choice(kRevType, "Reverb Type", StringArray { "Room", "Hall", "Plate" }, 0));
    layout.add(uni(kRevSize, "Reverb Size", 0.5f));
    layout.add(uni(kRevDamp, "Reverb Damp", 0.5f));
    layout.add(uni(kRevMix,  "Reverb Mix", 0.0f));
    layout.add(boolean(kRevOn, "Reverb On", false));
    slotExtras("rev", "Reverb");
    layout.add(choice(kRevPFocus, "Reverb Focus", pFocus, 0));

    // ---- standalone stages (named params per DSP_BUILD s9) ----------------
    layout.add(boolean(kLofiOn, "Lo-Fi On", false));
    layout.add(uni(kLofiBits, "Lo-Fi Bits", 0.0f));
    layout.add(uni(kLofiSrate, "Lo-Fi Rate", 0.0f));
    layout.add(uni(kLofiCompand, "Lo-Fi Compand", 0.0f));
    layout.add(boolean(kLofiAlias, "Lo-Fi Alias", false));
    layout.add(boolean(kWidthOn, "Width On", false));
    layout.add(uni(kWidth, "Width", 0.5f));
    layout.add(uni(kWidthHaas, "Width Haas", 0.0f));
    layout.add(boolean(kWidthBassMono, "Width Bass Mono", false));
    layout.add(boolean(kTalkOn, "Talk On", false));
    layout.add(uni(kTalkVa, "Talk Vowel A", 0.0f));
    layout.add(uni(kTalkVb, "Talk Vowel B", 0.5f));
    layout.add(uni(kTalkMorph, "Talk Morph", 0.0f));
    layout.add(uni(kTalkSens, "Talk Sens", 0.0f));
    layout.add(choice(kFxPrePost, "Lo-Fi Routing", StringArray { "Post", "Pre" }, 0));

    layout.add(uni(kDrift, "Drift", 0.0f));
    layout.add(choice(kInterp, "Interpolation", StringArray { "Drop Sample", "Linear" }, 1));
    layout.add(choice(kEngine, "Engine", StringArray { "Vintage", "Modern" }, 0));

    return layout;
}

} // namespace dreamer::params
