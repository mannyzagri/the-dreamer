// Params.h -- The Dreamer's APVTS layout. Parameter IDs are LOCKED for the
// WebView GUI handoff (bind by ID). Per-partial blocks use prefixes a_ / b_.
// FX parameters mirror Rubber-Rhino's IDs/ranges/defaults verbatim so the FX
// section behaves identically to the donor.
//
// Choice-count freeze: the 78-entry wave list and 7-entry filter list must
// never change order or count post-release (Cubase save-compat / re-scan).

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/bank/RomplerBank.h"

namespace dreamer::params {

// ---- global ----------------------------------------------------------------
inline constexpr auto kInterp    = "interp";      // Drop Sample / Linear
inline constexpr auto kEngine    = "engine";      // Vintage / Modern (RR id)
inline constexpr auto kVolume    = "volume";      // master, pre-FX (RR id)
inline constexpr auto kOutput    = "output";      // post-FX (RR id)
inline constexpr auto kCondition = "condition";   // hiss (RR id)
inline constexpr auto kStability = "stability";   // tune/cutoff drift (RR id)
inline constexpr auto kTempo     = "tempo";       // delay-sync fallback BPM (RR id)

// ---- FX (Rubber-Rhino IDs, verbatim) --------------------------------------
inline constexpr auto kDistType    = "dist_type";
inline constexpr auto kDistDrive   = "dist_drive";
inline constexpr auto kDistColor   = "dist_color";
inline constexpr auto kDistMix     = "dist_mix";
inline constexpr auto kDelayType   = "delay_type";
inline constexpr auto kDelayMode   = "delay_mode";
inline constexpr auto kDelayMs     = "delay_ms";
inline constexpr auto kDelayFb     = "delay_fb";
inline constexpr auto kDelayMix    = "delay_mix";
inline constexpr auto kDelayHpSlope  = "delay_hp_slope";
inline constexpr auto kDelayHpFreq   = "delay_hp_freq";
inline constexpr auto kDelayLpSlope  = "delay_lp_slope";
inline constexpr auto kDelayLpFreq   = "delay_lp_freq";
inline constexpr auto kChorusOn      = "chorus_on";
inline constexpr auto kChorusDepth   = "chorus_depth";
inline constexpr auto kChorusRate    = "chorus_rate";
inline constexpr auto kFlangerOn     = "flanger_on";
inline constexpr auto kFlangerDepth  = "flanger_depth";
inline constexpr auto kFlangerRate   = "flanger_rate";
inline constexpr auto kPhaserOn      = "phaser_on";
inline constexpr auto kPhaserDepth   = "phaser_depth";
inline constexpr auto kPhaserRate    = "phaser_rate";
inline constexpr auto kRevType       = "rev_type";
inline constexpr auto kRevSize       = "rev_size";
inline constexpr auto kRevDamp       = "rev_damp";
inline constexpr auto kRevMix        = "rev_mix";
inline constexpr auto kReverbHpSlope = "reverb_hp_slope";
inline constexpr auto kReverbHpFreq  = "reverb_hp_freq";
inline constexpr auto kReverbLpSlope = "reverb_lp_slope";
inline constexpr auto kReverbLpFreq  = "reverb_lp_freq";
inline constexpr auto kCompOn     = "comp_on";
inline constexpr auto kCompLim    = "comp_lim";
inline constexpr auto kCompThresh = "comp_thresh";
inline constexpr auto kCompRatio  = "comp_ratio";
inline constexpr auto kCompGain   = "comp_gain";
inline constexpr auto kClipOn     = "clip_on";

// ---- per-partial suffixes (prefix "a_" / "b_") -----------------------------
// on, wave, coarse, fine, level, velsens, start, filter_type, cutoff, reso,
// env_amt, keyfollow, tvf_att/dec/sus/rel, tva_att/dec/sus/rel,
// lfo{1,2}_shape/rate/depth/dest/keytrig            (29 params per partial)

inline juce::String pid(const char* prefix, const char* suffix) {
    return juce::String(prefix) + suffix;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto zeroToOne = [](const String& id, const String& name, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float>(0.0f, 1.0f, 0.001f), def);
    };
    auto msTime = [](const String& id, const String& name, float maxMs, float def) {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name,
            NormalisableRange<float>(1.0f, maxMs, 0.1f, 0.3f), def,
            AudioParameterFloatAttributes().withLabel("ms"));
    };

    // wave list from the bank (78 entries, frozen)
    StringArray waves;
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        waves.add(String(rompler::bank::kWaveforms[i].category) + ": "
                  + rompler::bank::kWaveforms[i].name);

    const StringArray filterTypes { "Classic", "Liquid", "Scream", "Plane",
                                    "Ladder", "MS20", "Wasp" };
    const StringArray lfoShapes { "Sine", "Tri", "Ramp", "Square", "S&H" };
    const StringArray lfoDests  { "Pitch", "Cutoff", "Level" };

    // ---- partial blocks -----------------------------------------------------
    for (const char* px : { "a_", "b_" }) {
        const String P = (px[0] == 'a') ? "A " : "B ";

        layout.add(std::make_unique<AudioParameterBool>(
            ParameterID { pid(px, "on"), 1 }, P + "On", true));
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

        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID { pid(px, "filter_type"), 1 }, P + "Filter Type", filterTypes, 0));
        layout.add(std::make_unique<AudioParameterFloat>(
            ParameterID { pid(px, "cutoff"), 1 }, P + "Cutoff",
            NormalisableRange<float>(60.0f, 12000.0f, 0.1f, 0.25f), 8000.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));
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

        for (int l = 1; l <= 2; ++l) {
            const String lp = "lfo" + String(l) + "_";
            const String LN = P + "LFO " + String(l) + " ";
            layout.add(std::make_unique<AudioParameterChoice>(
                ParameterID { pid(px, (lp + "shape").toRawUTF8()), 1 }, LN + "Shape", lfoShapes, 0));
            layout.add(std::make_unique<AudioParameterFloat>(
                ParameterID { pid(px, (lp + "rate").toRawUTF8()), 1 }, LN + "Rate",
                NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
            layout.add(zeroToOne(pid(px, (lp + "depth").toRawUTF8()), LN + "Depth", 0.0f));
            layout.add(std::make_unique<AudioParameterChoice>(
                ParameterID { pid(px, (lp + "dest").toRawUTF8()), 1 }, LN + "Dest", lfoDests, 0));
            layout.add(std::make_unique<AudioParameterBool>(
                ParameterID { pid(px, (lp + "keytrig").toRawUTF8()), 1 }, LN + "Key Trig", true));
        }
    }

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
    layout.add(zeroToOne(kCondition, "Condition", 1.0f));
    layout.add(zeroToOne(kStability, "Stability", 1.0f));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kTempo, 1 }, "Tempo",
        NormalisableRange<float>(30.0f, 300.0f, 0.1f), 120.0f,
        AudioParameterFloatAttributes().withLabel("BPM")));

    // ---- FX (Rubber-Rhino verbatim: IDs, ranges, defaults) -----------------
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kDistType, 1 }, "Dist Type",
        StringArray { "Soft", "Hard", "Fold", "Dig", "Fuzz", "Over" }, 0));
    layout.add(zeroToOne(kDistDrive, "Dist Drive", 0.2f));
    layout.add(zeroToOne(kDistColor, "Dist Color", 0.5f));
    layout.add(zeroToOne(kDistMix,   "Dist Mix",   0.0f));

    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kDelayType, 1 }, "Delay Type", StringArray { "Tape", "Ping", "Dig" }, 2));
    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kDelayMode, 1 }, "Delay Mode", StringArray { "Sync", "Free" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kDelayMs, 1 }, "Delay Free Time",
        NormalisableRange<float>(1.0f, 1000.0f, 1.0f, 0.4f), 250.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kDelayFb, 1 }, "Delay Feedback",
        NormalisableRange<float>(0.0f, 0.9f, 0.001f), 0.35f));
    layout.add(zeroToOne(kDelayMix, "Delay Mix", 0.0f));

    auto slopeChoice = [](const char* id, const char* name) {
        return std::make_unique<AudioParameterChoice>(
            ParameterID { id, 1 }, name, StringArray { "Off", "12dB", "24dB" }, 0);
    };
    layout.add(slopeChoice(kDelayHpSlope, "Delay HP Slope"));
    layout.add(zeroToOne  (kDelayHpFreq,  "Delay HP Freq", 0.0f));
    layout.add(slopeChoice(kDelayLpSlope, "Delay LP Slope"));
    layout.add(zeroToOne  (kDelayLpFreq,  "Delay LP Freq", 1.0f));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kChorusOn, 1 }, "Chorus On", false));
    layout.add(zeroToOne(kChorusDepth, "Chorus Depth", 0.5f));
    layout.add(zeroToOne(kChorusRate,  "Chorus Rate",  0.3f));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kFlangerOn, 1 }, "Flanger On", false));
    layout.add(zeroToOne(kFlangerDepth, "Flanger Depth", 0.5f));
    layout.add(zeroToOne(kFlangerRate,  "Flanger Rate",  0.3f));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kPhaserOn, 1 }, "Phaser On", false));
    layout.add(zeroToOne(kPhaserDepth, "Phaser Depth", 0.5f));
    layout.add(zeroToOne(kPhaserRate,  "Phaser Rate",  0.3f));

    layout.add(std::make_unique<AudioParameterChoice>(
        ParameterID { kRevType, 1 }, "Reverb Type",
        StringArray { "Room", "Plate", "Hall", "Spring" }, 0));
    layout.add(zeroToOne(kRevSize, "Reverb Size", 0.5f));
    layout.add(zeroToOne(kRevDamp, "Reverb Damp", 0.5f));
    layout.add(zeroToOne(kRevMix,  "Reverb Mix",  0.0f));
    layout.add(slopeChoice(kReverbHpSlope, "Reverb HP Slope"));
    layout.add(zeroToOne  (kReverbHpFreq,  "Reverb HP Freq", 0.0f));
    layout.add(slopeChoice(kReverbLpSlope, "Reverb LP Slope"));
    layout.add(zeroToOne  (kReverbLpFreq,  "Reverb LP Freq", 1.0f));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kCompOn, 1 }, "Comp On", false));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kCompLim, 1 }, "Comp Limiter", false));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kCompThresh, 1 }, "Comp Threshold",
        NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -12.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kCompRatio, 1 }, "Comp Ratio",
        NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        AudioParameterFloatAttributes().withLabel(":1")));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { kCompGain, 1 }, "Comp Gain",
        NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID { kClipOn, 1 }, "Clip (Brickwall -0.1dB)", false));

    return layout;
}

} // namespace dreamer::params
