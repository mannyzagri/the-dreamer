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
//   voicing_[t]     choice  s11 multi-tap voicing (Single/Oct/Power/Dreamy)
//   dreamy_spread_[t] choice s11 DREAMY interval set (Add9/Min7/Sus2)
//   loop_mode_[t]   choice  s12 loop mode (Fwd/Pingpong; Loop waves only)
//   hit_play_[t]    choice  s13 one-shot playback (Normal/Stretch)
//   hit_stretch_[t] 0..1    s13 varispeed 0.25x..4x (log, 0.5=1.0x)
//   hit_pitchtrim_[t] int   s13 -24..+24 semitone re-tune (still varispeed)
// FLAGGED interpretation: tvf_env / flt1_env are UNIPOLAR 0..1 per the
// table (upward-only env sweep); negative env is not reachable.
// s13 note: hit_stretch is NOT a matrix dest -- DSP_BUILD s9 (which WINS)
// lists FX PARAM in the matrix dest list, so mtxDests gains "Fx Param"
// (RESERVED/inert engine dest dstFxParam=9), not hit_stretch.
//
// Normalized 0..1 -> real-unit maps live in PluginProcessor.cpp (documented
// there); GUI displays 0-127 (bipolar -63..+63) regardless.
//
// Choice-count freezes: wave 218 (78 cycle + 130 Loop + 10 Shot, bank3 order);
// flt type 14; shaper 6; tvf type 4; lfo/orbit shapes 5; mtx src 7, dst 10;
// voicing 4; dreamy_spread 3; loop_mode 2; hit_play 2; modfx 7; dly 3; rev 3.

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/bank/Bank3.h"
#include "plugin/ParamLaws.h"

namespace dreamer::params {

// ---- D1: envelope-time real-unit display -----------------------------------
// The knob value (normalized 0..1) maps to seconds via lawv::envTimeSec; the
// LCD shows "340 ms" under 1 s and "1.2 s" above. textToValue parses either.
inline juce::String envTimeToText(float v01) {
    const double sec = lawv::envTimeSec((double)v01);
    if (sec < 1.0)  return juce::String(juce::roundToInt(sec * 1000.0)) + " ms";
    return juce::String(sec, sec < 10.0 ? 2 : 1) + " s";
}
inline float envTimeFromText(const juce::String& t) {
    const juce::String s = t.trim().toLowerCase();
    double sec;
    if (s.endsWith("ms"))     sec = s.dropLastCharacters(2).getDoubleValue() * 0.001;
    else if (s.endsWith("s")) sec = s.dropLastCharacters(1).getDoubleValue();
    else                      sec = s.getDoubleValue() * 0.001;   // bare number = ms
    return (float)lawv::envTimeInv(sec);
}
inline juce::String penvTimeToText(float v01) {
    const double sec = lawv::penvTimeSec((double)v01);
    if (sec < 1.0)  return juce::String(juce::roundToInt(sec * 1000.0)) + " ms";
    return juce::String(sec, sec < 10.0 ? 2 : 1) + " s";
}
inline float penvTimeFromText(const juce::String& t) {
    const juce::String s = t.trim().toLowerCase();
    double sec;
    if (s.endsWith("ms"))     sec = s.dropLastCharacters(2).getDoubleValue() * 0.001;
    else if (s.endsWith("s")) sec = s.dropLastCharacters(1).getDoubleValue();
    else                      sec = s.getDoubleValue();           // bare number = s
    return (float)lawv::penvTimeInv(sec);
}

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
inline constexpr auto kFlt2Env       = "flt2_env";    // v15 GUI: F2 env (symmetric w/ F1)
inline constexpr auto kFlt2Morph     = "flt2_morph";
inline constexpr auto kLfoRate       = "lfo_rate";
inline constexpr auto kLfoShape      = "lfo_shape";
inline constexpr auto kLfoSync       = "lfo_sync";    // v15 GUI: global LFO tempo sync
inline constexpr auto kLfo2Rate      = "lfo2_rate";   // v16 GUI: GLOBAL LFO 2 (distinct
inline constexpr auto kLfo2Shape     = "lfo2_shape";  //  from the per-tone lfo2_*_[t] ids)
inline constexpr auto kLfo2Sync      = "lfo2_sync";
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
inline constexpr auto kDlySync       = "dly_sync";   // v9: tempo-sync delay time
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
// v15 GUI: one PARAMS knob per FX slot (edits the focus-selected extra), +
// lofi/talk focus selectors. Engine "focus-shadow": the focused sub-param's
// value comes from <slot>_param, the rest from their existing p_i/named params.
inline constexpr auto kModfxParam    = "modfx_param";
inline constexpr auto kDlyParam      = "dly_param";
inline constexpr auto kRevParam      = "rev_param";
inline constexpr auto kLofiParam     = "lofi_param";
inline constexpr auto kLofiPFocus    = "lofi_pfocus";
inline constexpr auto kTalkParam     = "talk_param";
inline constexpr auto kTalkPFocus    = "talk_pfocus";
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
// ---- UX round (UX_DSP_TASKS) global additions ------------------------------
inline constexpr auto kGEnvA         = "g_env_a";   // D5 global TVA-env offsets
inline constexpr auto kGEnvD         = "g_env_d";
inline constexpr auto kGEnvS         = "g_env_s";
inline constexpr auto kGEnvR         = "g_env_r";
inline constexpr auto kGCutoff       = "g_cutoff";  // D5 global tone-cutoff offset
inline constexpr auto kGRes          = "g_res";     // D5 global tone-reso offset
inline constexpr auto kGOctave       = "g_octave";  // D8 global octave -2..+2
inline constexpr auto kLimiterOn     = "limiter_on";// D12 output limiter enable

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
    // D1: env A/D/R time knob -- 0..1 range (log law in ParamLaws), LCD in ms/s.
    auto envt = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(0.0f, 1.0f, 0.001f), def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v, int) { return envTimeToText(v); })
                .withValueFromStringFunction([](const String& s) { return envTimeFromText(s); }));
    };
    auto penvt = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(0.0f, 1.0f, 0.001f), def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v, int) { return penvTimeToText(v); })
                .withValueFromStringFunction([](const String& s) { return penvTimeFromText(s); }));
    };

    // wave list from bank3 (218, order LOCKED)
    StringArray waves;
    for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i)
        waves.add(String(rompler::bank3::kWaveforms[(size_t)i].category) + ": "
                  + rompler::bank3::kWaveforms[(size_t)i].name);

    const StringArray shaperTypes { "Off", "Soft Fold", "Hard Fold", "Sine Fold",
                                    "Asym", "Drive" };
    const StringArray tvfTypes    { "LP 24", "LP 12", "BP", "HP" };
    const StringArray filterTypes { "LP 24", "LP 12", "BP", "HP", "Liquid",
                                    "Classic", "Ladder", "Notch", "Comb +",
                                    "Comb -", "N+LP", "Formant", "Allpass",
                                    "DreamPln" };
    const StringArray lfoShapes   { "Tri", "Sin", "Saw", "Sqr", "S+H" };
    const StringArray orbitShapes { "Saw", "Tri", "Sin", "Sqr", "S+H" };
    const StringArray auxDests    { "Pitch", "Start", "Shape", "Pan", "Noise" };
    const StringArray lfoDests    { "Pitch", "Cutoff", "Shape", "Level" };
    const StringArray mtxSrcs     { "G-LFO 1", "Vec Phs", "Aux", "Velo", "Wheel",
                                    "G-LFO 2", "G-Aux" };   // v16: +G-LFO 2 (live), +G-Aux (reserved)
    const StringArray mtxDests    { "Pitch", "Cut 1", "Cut 2", "Morph",
                                    "Shape", "Vec Phs", "Pan", "Noise",
                                    "Fx Param", "Loop Rate" };   // s9 dst 9 (inert)
                                                                 // + D15 dst 10 loop rate
    const StringArray voicings    { "Single", "Oct", "Power", "Dreamy" };
    const StringArray dreamySpreads { "Add9", "Min7", "Sus2" };
    const StringArray loopModes   { "Fwd", "Pingpong" };
    const StringArray hitPlays    { "Normal", "Stretch" };
    // D15: loop-rate tempo divisions (one loop-morph sweep per division).
    const StringArray loopRateBeats { "4/1", "2/1", "1/1", "1/2", "1/2T", "1/4",
                                      "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

    // ---- per-tone blocks (suffix _a.._d) -----------------------------------
    const char* toneNames[4] = { "A ", "B ", "C ", "D " };
    for (int t = 0; t < 4; ++t) {
        const String P = toneNames[t];
        layout.add(choice(tid("wave", t), P + "Wave", waves, 0));
        layout.add(boolean(tid("on", t), P + "On", t == 0));
        layout.add(uni(tid("level", t), P + "Level", 0.8f));
        layout.add(std::make_unique<AudioParameterInt>(
            ParameterID { tid("oct", t), 1 }, P + "Octave", -2, 2, 0));
        layout.add(std::make_unique<AudioParameterInt>(          // D7 per-tone SEMI
            ParameterID { tid("semi", t), 1 }, P + "Semi", -12, 12, 0));
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
        // s11 multi-tap voicing / s12 loop mode / s13 hit varispeed (per tone)
        layout.add(choice(tid("voicing", t), P + "Voicing", voicings, 0));
        layout.add(choice(tid("dreamy_spread", t), P + "Dreamy Spread", dreamySpreads, 0));
        // D9 per-tone engine-side detune: N symmetric taps per voicing tap.
        // Choice (not Int) so the GUI binds it as a stepper/combo; engine reads
        // index+1 -> 1..4 voices.
        layout.add(choice(tid("detune_voices", t), P + "Detune Voices",
                          StringArray { "1", "2", "3", "4" }, 0));
        layout.add(uni(tid("detune_amount", t), P + "Detune Amount", 0.0f));
        layout.add(choice(tid("loop_mode", t), P + "Loop Mode", loopModes, 0));
        // D15: PLAY MODE (was hit-only) now applies to ALL wave types. Kept the
        // id "hit_play" (no preset break); display generalized to "Play Mode".
        layout.add(choice(tid("hit_play", t), P + "Play Mode", hitPlays, 0));
        layout.add(uni(tid("hit_stretch", t), P + "Hit Stretch", 0.5f));
        layout.add(std::make_unique<AudioParameterInt>(
            ParameterID { tid("hit_pitchtrim", t), 1 }, P + "Hit Pitch Trim", -24, 24, 0));
        // D15: LOOP + STRETCH -> decoupled morph speed (pitch stays on the note)
        // unless loop_varispeed (plain pitch-follows varispeed). loop_rate is
        // 0..1 -> 0.25x..4x (log, 0.5 = 1.0x); tempo-syncable via the beat list.
        layout.add(uni(tid("loop_rate", t), P + "Loop Rate", 0.5f));
        layout.add(boolean(tid("loop_rate_sync", t), P + "Loop Rate Sync", false));
        layout.add(choice(tid("loop_rate_beats", t), P + "Loop Rate Beats", loopRateBeats, 5));
        layout.add(boolean(tid("loop_varispeed", t), P + "Loop Varispeed", false));
        // v7: two per-tone LFOs (rate free-Hz or tempo-synced to 12 divisions)
        for (int lf = 1; lf <= 2; ++lf) {
            const String LB = "lfo" + String(lf) + "_";
            const String LN = P + "LFO " + String(lf) + " ";
            layout.add(uni(tid((LB + "rate").toRawUTF8(), t), LN + "Rate", 0.5f));
            layout.add(uni(tid((LB + "depth").toRawUTF8(), t), LN + "Depth", 0.0f));
            layout.add(boolean(tid((LB + "sync").toRawUTF8(), t), LN + "Sync", false));
            layout.add(choice(tid((LB + "dest").toRawUTF8(), t), LN + "Dest", lfoDests, 0));
            // v11: per-LFO shape (panel TRI/SIN/SAW/SQR/S+H); default SIN keeps
            //  the pre-v11 fixed-sine behaviour bit-identical for init patches.
            layout.add(choice(tid((LB + "shape").toRawUTF8(), t), LN + "Shape", lfoShapes, 1));
        }
        layout.add(choice(tid("aux_dest", t), P + "Aux Dest", auxDests, 0));
        layout.add(bip(tid("aux_amt", t), P + "Aux Amt", 0.0f));   // flagged
        layout.add(choice(tid("tvf_type", t), P + "TVF Type", tvfTypes, 0));
        layout.add(uni(tid("tvf_cut", t), P + "TVF Cutoff", 0.8f));
        layout.add(uni(tid("tvf_res", t), P + "TVF Reso", 0.0f));
        layout.add(uni(tid("tvf_env", t), P + "TVF Env", 0.0f));
        // D10: filter key-follow is now BIPOLAR (-1..+1, display -100..+100,
        // center detent 0). Negative = darker up the keyboard. Preset values
        // migrated old[0..1] -> new[0.5..1] (tools/migrate_env_times.sh companion).
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { tid("tvf_kf", t), 1 }, P + "TVF KeyFollow",
            NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v, int) {
                    return juce::String(juce::roundToInt(v * 100.0f)); })
                .withValueFromStringFunction([](const juce::String& s) {
                    return juce::jlimit(-1.0f, 1.0f, (float)(s.getDoubleValue() / 100.0)); })));
        // D1: env A/D/R defaults re-expressed for the new log law so a fresh
        // tone keeps its pre-D1 SECONDS (0.05->~2 ms attack, 0.35->~344 ms).
        layout.add(envt(tid("tvf_a", t), P + "TVF Attack", 0.075f));
        layout.add(envt(tid("tvf_d", t), P + "TVF Decay", 0.634f));
        layout.add(uni (tid("tvf_s", t), P + "TVF Sustain", 1.0f));
        layout.add(envt(tid("tvf_r", t), P + "TVF Release", 0.634f));
        layout.add(envt(tid("tva_a", t), P + "TVA Attack", 0.075f));
        layout.add(envt(tid("tva_d", t), P + "TVA Decay", 0.634f));
        layout.add(uni (tid("tva_s", t), P + "TVA Sustain", 1.0f));
        layout.add(envt(tid("tva_r", t), P + "TVA Release", 0.634f));
        layout.add(envt(tid("aux_a", t), P + "Aux Attack", 0.075f));
        layout.add(envt(tid("aux_d", t), P + "Aux Decay", 0.634f));
        layout.add(uni (tid("aux_s", t), P + "Aux Sustain", 1.0f));
        layout.add(envt(tid("aux_r", t), P + "Aux Release", 0.634f));
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
    layout.add(penvt(kVecPenvTime, "P-Env Time", 0.3f));
    layout.add(boolean(kVecPenvLoop, "P-Env Loop", false));

    layout.add(boolean(kFltRoute, "Filter Routing", false));   // v15 GUI: Bool (false=Ser, true=Par)
    layout.add(bip(kFltBal, "Filter Balance", 0.0f));
    layout.add(choice(kFlt1Type, "Filter 1 Type", filterTypes, 0));
    layout.add(uni(kFlt1Cut, "Filter 1 Cutoff", 1.0f));
    layout.add(uni(kFlt1Res, "Filter 1 Reso", 0.0f));
    layout.add(uni(kFlt1Env, "Filter 1 Env", 0.0f));
    layout.add(choice(kFlt2Type, "Filter 2 Type", filterTypes, 0));
    layout.add(uni(kFlt2Cut, "Filter 2 Cutoff", 1.0f));
    layout.add(uni(kFlt2Res, "Filter 2 Reso", 0.0f));
    layout.add(uni(kFlt2Env, "Filter 2 Env", 0.0f));      // v15: F2 env (like F1)
    layout.add(uni(kFlt2Morph, "Filter 2 Morph", 0.0f));

    layout.add(uni(kLfoRate, "G-LFO Rate", 0.5f));
    layout.add(choice(kLfoShape, "G-LFO Shape", lfoShapes, 0));
    layout.add(boolean(kLfoSync, "G-LFO Sync", false));   // v15: global LFO tempo sync
    layout.add(uni(kLfo2Rate, "G-LFO 2 Rate", 0.25f));    // v16: second global LFO
    layout.add(choice(kLfo2Shape, "G-LFO 2 Shape", lfoShapes, 0));
    layout.add(boolean(kLfo2Sync, "G-LFO 2 Sync", false));

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
    layout.add(uni(kModfxParam, "Mod FX Param", 0.5f));   // v15 focus-shadow knob

    layout.add(choice(kDlyMode, "Delay Mode", StringArray { "Digital", "Tape", "Pong" }, 0));
    layout.add(uni(kDlyTime, "Delay Time", 0.55f));
    layout.add(uni(kDlyFb,   "Delay Feedback", 0.39f));
    layout.add(uni(kDlyMix,  "Delay Mix", 0.0f));
    layout.add(boolean(kDlyOn, "Delay On", false));
    layout.add(boolean(kDlySync, "Delay Sync", false));
    slotExtras("dly", "Delay");
    layout.add(choice(kDlyPFocus, "Delay Focus", pFocus, 0));
    layout.add(uni(kDlyParam, "Delay Param", 0.5f));      // v15 focus-shadow (reserved/inert)

    layout.add(choice(kRevType, "Reverb Type", StringArray { "Room", "Hall", "Plate" }, 0));
    layout.add(uni(kRevSize, "Reverb Size", 0.5f));
    layout.add(uni(kRevDamp, "Reverb Damp", 0.5f));
    layout.add(uni(kRevMix,  "Reverb Mix", 0.0f));
    layout.add(boolean(kRevOn, "Reverb On", false));
    slotExtras("rev", "Reverb");
    layout.add(choice(kRevPFocus, "Reverb Focus", pFocus, 0));
    layout.add(uni(kRevParam, "Reverb Param", 0.5f));     // v15 focus-shadow (reserved/inert)

    // ---- standalone stages (named params per DSP_BUILD s9) ----------------
    layout.add(boolean(kLofiOn, "Lo-Fi On", false));
    layout.add(uni(kLofiBits, "Lo-Fi Bits", 0.0f));
    layout.add(uni(kLofiSrate, "Lo-Fi Rate", 0.0f));
    layout.add(uni(kLofiCompand, "Lo-Fi Compand", 0.0f));
    layout.add(boolean(kLofiAlias, "Lo-Fi Alias", false));
    layout.add(uni(kLofiParam, "Lo-Fi Param", 0.5f));     // v15 focus-shadow knob
    layout.add(choice(kLofiPFocus, "Lo-Fi Focus", pFocus, 0));
    layout.add(boolean(kWidthOn, "Width On", false));
    layout.add(uni(kWidth, "Width", 0.5f));
    layout.add(uni(kWidthHaas, "Width Haas", 0.0f));
    layout.add(boolean(kWidthBassMono, "Width Bass Mono", false));
    layout.add(boolean(kTalkOn, "Talk On", false));
    layout.add(uni(kTalkVa, "Talk Vowel A", 0.0f));
    layout.add(uni(kTalkVb, "Talk Vowel B", 0.5f));
    layout.add(uni(kTalkMorph, "Talk Morph", 0.0f));
    layout.add(uni(kTalkSens, "Talk Sens", 0.0f));
    layout.add(uni(kTalkParam, "Talk Param", 0.5f));      // v15 focus-shadow knob
    layout.add(choice(kTalkPFocus, "Talk Focus", pFocus, 0));
    layout.add(boolean(kFxPrePost, "Lo-Fi Routing", false));   // v15 GUI: Bool (false=Post, true=Pre)

    layout.add(uni(kDrift, "Drift", 0.0f));
    layout.add(choice(kInterp, "Interpolation", StringArray { "Drop Sample", "Linear" }, 1));
    layout.add(choice(kEngine, "Engine", StringArray { "Vintage", "Modern" }, 0));

    // ---- UX round additions (host-automatable; GUI exposes per UX_GUI_TASKS) --
    // D5 GLOBAL OFFSETS: bipolar, default 0, ADD to every tone's normalized knob
    // value at control rate (clamped to 0..1) -- they never overwrite per-tone
    // values, so patch relationships are preserved. g_env_* target the TVA env
    // of all tones; g_cutoff/g_res target every tone's TVF.
    layout.add(bip(kGEnvA, "Global Env Attack",  0.0f));
    layout.add(bip(kGEnvD, "Global Env Decay",   0.0f));
    layout.add(bip(kGEnvS, "Global Env Sustain", 0.0f));
    layout.add(bip(kGEnvR, "Global Env Release", 0.0f));
    layout.add(bip(kGCutoff, "Global Cutoff",    0.0f));
    layout.add(bip(kGRes,    "Global Reso",      0.0f));
    // D8 GLOBAL OCTAVE: one integer, shifts all tones.
    layout.add(std::make_unique<AudioParameterInt>(
        ParameterID { kGOctave, 1 }, "Global Octave", -2, 2, 0));
    // D12 OUTPUT LIMITER enable (soft-clip + ceiling); default ON = pre-D12.
    layout.add(boolean(kLimiterOn, "Limiter On", true));

    return layout;
}

} // namespace dreamer::params
