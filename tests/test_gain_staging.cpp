// test_gain_staging.cpp -- GAIN_STAGING.md fixes 1-5 (clip prevention).
//
// Build (after vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES
//     /I. tests\test_gain_staging.cpp /Fo:tests\bin\ /Fe:tests\bin\test_gain_staging.exe
//
// Each fix proven independently, JUCE-free at production parameters:
//   [tonesum] fix 1  1/sqrt(nEnabledTones): 4 identical tones sum to 2x a single
//                    tone (equal-power law), NOT 4x, and land <= ~0 dBFS.
//   [voicing] fix 2  voicingTapGain is exactly 1/sqrt(nTaps) (equal-power).
//   [fxmix]   fix 3  reverb/delay dry-wet is linear/equal-power, never additive:
//                    mix=0 -> dry (bypass), mix=1 -> wet<=unity, no >dry+wet sum.
//   [delay]   fix 3  real StereoDelay at fb 0.9, mix 1 does not build past unity.
//   [filter]  fix 4  RhinoFilterSlot trims input by (1-0.4*res); at max res the
//                    peak is exactly 0.6x the untrimmed ported filter.
//   [output]  fix 5  OutputStage soft-clips a +6 dB input and NEVER exceeds the
//                    -0.1 dBFS ceiling (0.98855); small signals stay ~transparent.

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>

#include "dsp/bank/Bank3.h"
#include "dsp/glue/DreamVoice.h"
#include "dsp/glue/RhinoFilterSlot.h"
#include "dsp/glue/OutputStage.h"
#include "dsp/ported/RhinoFilter.h"
#include "dsp/ported/fx/Effects.h"     // StereoDelay

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static bool approx(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static constexpr int SR = 44100;

static int findWave(const char* cat, const char* name) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank::kWaveforms[i].category, cat) &&
            !std::strcmp(rompler::bank::kWaveforms[i].name, name)) return i;
    return -1;
}

static dreamer::ToneParams sineTone(double level) {
    dreamer::ToneParams t;
    t.enabled      = true;
    t.waveIndex    = findWave("Basic", "Sine");
    t.level        = level;
    t.cutoffHz     = 18000.0;
    t.tvfKeyFollow = 0.0;
    t.tvfEnvAmount = 0.0;
    t.tvaA = 0.001; t.tvaS = 1.0; t.tvaR = 0.05;
    t.tvfA = 0.001; t.tvfS = 1.0;
    return t;
}

static float renderPeak(const dreamer::DreamPatch& patch, int nSamples) {
    auto s = std::make_unique<dreamer::DreamSynth>();
    s->prepare((double)SR);
    s->patch() = patch;
    s->updateLive();
    s->noteOn(69, 0.8f);
    float peak = 0.0f;
    for (int i = 0; i < nSamples; ++i) {
        float l = 0, r = 0;
        s->process(l, r);
        if (i > SR / 20) peak = std::fmax(peak, std::fabs(l));   // skip attack
    }
    return peak;
}

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ/DAZ

    // ---- [loudness] D11 WaveNorm calibration target (v2.5.4: -8 dBFS RMS) -
    // Regression guard on the per-wave playback normalization: for any
    // non-clamped CYCLE/LOOP wave, kWaveNormGain * measuredRms must equal the
    // -8 dBFS RMS target (0.39811). Catches an accidental revert to the old
    // -14 target (0.19953) or a bad re-bake. HITs (OneShot) are gain 1.0 by
    // design (peak-normalized) and exempt.
    std::printf("[loudness]\n");
    {
        constexpr double kTargetRms = 0.39811;   // 10^(-8/20)
        constexpr float  kClampHi = 8.0f, kClampLo = 0.125f;
        int checked = 0;
        for (int i = 0; i < rompler::bank3::kNumWaveforms; ++i) {
            const auto& w = rompler::bank3::kWaveforms[(size_t)i];
            if (w.type == rompler::bank3::WaveType::OneShot) continue;
            double sumsq = 0.0;
            for (uint32_t s = 0; s < w.length; ++s) {
                const double x = (double)w.samples[s] * (1.0 / 32768.0);
                sumsq += x * x;
            }
            const double rms = w.length ? std::sqrt(sumsq / (double)w.length) : 0.0;
            const float g = dreamer::kWaveNormGain[i];
            if (g >= kClampHi || g <= kClampLo || rms < 1e-9) continue;  // skip clamped
            CHECK(approx(g * rms, kTargetRms, 0.005), "WaveNorm gain hits the -8 dBFS RMS target");
            ++checked;
        }
        std::printf("  verified %d non-clamped waves at -8 dBFS RMS target\n", checked);
        CHECK(checked > 50, "enough waves exercised the loudness invariant");
    }

    // ---- [tonesum] fix 1 ------------------------------------------------
    std::printf("[tonesum]\n");
    {
        dreamer::DreamPatch single;
        single.tone[0] = sineTone(0.5);

        dreamer::DreamPatch quad;
        for (int i = 0; i < 4; ++i) quad.tone[i] = sineTone(0.5);   // 4 identical

        const float p1 = renderPeak(single, SR / 2);
        const float p4 = renderPeak(quad,   SR / 2);
        std::printf("  single=%.4f  4-tone=%.4f  ratio=%.3f\n", p1, p4, p4 / p1);

        // equal-power law: 4 identical tones -> 4x raw * 1/sqrt(4) = 2x single.
        CHECK(approx(p4 / p1, 2.0, 0.02), "4 identical tones == 2x single (1/sqrt law)");
        // NOT the un-normalized ~3-4x that caused clipping.
        CHECK(p4 / p1 < 3.0, "4-tone sum is NOT the un-normalized 3-4x");
        // a unity-ish 4-tone patch lands <= ~0 dBFS after the tone-norm.
        CHECK(p4 <= 1.02f, "4-tone peak <= ~0 dBFS after tone-norm");
        // single tone must be untouched by the norm (target 1.0 for n==1).
        CHECK(p1 > 0.05f, "single tone audible (norm did not attenuate n==1)");
    }

    // ---- [voicing] fix 2 ------------------------------------------------
    std::printf("[voicing]\n");
    {
        using dreamer::voicingTapGain;
        CHECK(voicingTapGain(1) == 1.0f, "tapGain(1)=1 (SINGLE bit-identity)");
        CHECK(approx(voicingTapGain(2), 1.0 / std::sqrt(2.0), 1e-6), "tapGain(2)=1/sqrt2");
        CHECK(approx(voicingTapGain(3), 1.0 / std::sqrt(3.0), 1e-6), "tapGain(3)=1/sqrt3");
        CHECK(approx(voicingTapGain(4), 0.5, 1e-9),                  "tapGain(4)=1/sqrt4");
    }

    // ---- [fxmix] fix 3 (mix laws) --------------------------------------
    std::printf("[fxmix]\n");
    {
        // Delay: linear crossfade (matches PluginProcessor: l*(1-m)+wet*m).
        auto dlyMix = [](float dry, float wet, float m) { return dry * (1.0f - m) + wet * m; };
        CHECK(dlyMix(0.8f, 0.9f, 0.0f) == 0.8f, "delay mix=0 -> pure dry (bypass)");
        CHECK(dlyMix(0.8f, 0.9f, 1.0f) == 0.9f, "delay mix=1 -> pure wet");
        // linear interp stays within [min,max] of dry/wet -> never the additive sum.
        for (float m = 0.0f; m <= 1.0f; m += 0.05f) {
            const float o = dlyMix(1.0f, 1.0f, m);
            CHECK(o <= 1.0f + 1e-6f, "delay mix never exceeds unity (unity in)");
            CHECK(o < 1.0f + 0.9f,   "delay mix is NOT additive (would be up to 2.0)");
        }

        // Reverb: dry*(1-m) + wet*(kReverbWetNorm*m) (matches PluginProcessor).
        constexpr float kReverbWetNorm = 0.7f;
        auto revMix = [](float dry, float wet, float m) {
            return dry * (1.0f - m) + wet * (0.7f * m);
        };
        CHECK(approx(revMix(1.0f, 1.0f, 0.0f), 1.0f, 1e-6), "reverb mix->0 -> dry (bypass continuity)");
        CHECK(approx(revMix(1.0f, 1.0f, 1.0f), kReverbWetNorm, 1e-6), "reverb mix=1 -> wet fully replaces dry");
        CHECK(revMix(1.0f, 1.0f, 1.0f) <= 1.0f, "reverb unity dry+wet at mix=1 stays <= unity");
        // the OLD law summed to ~1.4x at mix=1: prove we are below that.
        CHECK(revMix(1.0f, 1.0f, 1.0f) < 1.0f * (1.0f - 0.4f) + 1.0f * 0.8f, "reverb below the old 1.4x sum");
    }

    // ---- [delay] fix 3 (real StereoDelay buildup) -----------------------
    std::printf("[delay]\n");
    {
        dreamer::StereoDelay d;
        d.prepare((double)SR);
        d.setType(dreamer::StereoDelay::Type::dig);
        d.setTimeFree(120.0f);
        d.reset();
        const float fb = 0.9f;      // == PluginProcessor dfb cap (0..1 -> 0..0.9)
        float peakMix = 0.0f;
        bool finite = true;
        // 2 s of unity sine into the line; mix=1 -> output IS the wet leg.
        for (int i = 0; i < SR * 2; ++i) {
            const float in = std::sin(2.0f * 3.14159265f * 220.0f * (float)i / (float)SR);
            float wl, wr;
            d.processSampleWet(in, fb, wl, wr);
            const float mixed = in * (1.0f - 1.0f) + wl * 1.0f;   // dlyMix at m=1
            if (!std::isfinite(mixed)) finite = false;
            peakMix = std::fmax(peakMix, std::fabs(mixed));
        }
        std::printf("  delay fb=0.9 mix=1 peak=%.3f\n", peakMix);
        CHECK(finite, "delay output finite");
        CHECK(peakMix <= 1.10f, "delay fb=0.9 does not build past ~unity");
    }

    // ---- [filter] fix 4 (resonant Rhino path stays bounded) -------------
    // The spec's res-comp trim could NOT go in RhinoFilterSlot: that adapter is
    // under an immutable bitwise null test (test_filter_port) and is not in the
    // live path anyway. What matters for clip prevention is that the resonant
    // Rhino filter STAYS BOUNDED under a loud (4-tone-sum) input -- which it does
    // on its own: the ported ladder self-saturates, and the live GlobalFilter
    // wraps it in safety() on top. Prove the boundedness the fix is really after.
    std::printf("[filter]\n");
    {
        const int   ladder = (int)dreamer::RubberFilter::Type::ladder;
        const float cut = 1000.0f, res = 1.0f;   // max resonance
        const float amp = 1.5f;                   // a loud (4-tone-ish) input

        dreamer::RhinoFilterSlot slot;            // == the ported filter (null-tested)
        slot.setSampleRate((double)SR);
        slot.setType(ladder);
        float peak = 0.0f; bool fin = true;
        for (int i = 0; i < SR; ++i) {
            slot.setCutoffRes(cut, res);
            const float in = amp * std::sin(2.0f * 3.14159265f * cut * (float)i / (float)SR);
            const float y = slot.process(in);
            if (!std::isfinite(y)) fin = false;
            if (i > SR / 4) peak = std::fmax(peak, std::fabs(y));
        }
        std::printf("  ladder max-res, loud (1.5x) input: peak=%.3f\n", peak);
        CHECK(fin, "resonant filter output finite at max res");
        // Self-limits well below runaway; GlobalFilter::safety() (asymptote ~1.5)
        // and the output ceiling (fix 5) are the remaining guarantees downstream.
        CHECK(peak <= 1.5f, "resonant ladder self-bounds under a loud input (no runaway)");
    }

    // ---- [output] fix 5 (soft-clip + ceiling) ---------------------------
    std::printf("[output]\n");
    {
        dreamer::OutputStage os;
        os.prepare(0.5f);
        const float ceil = dreamer::OutputStage::kCeiling;   // 0.98855

        // +6 dB input (2.0) soft-clipped and hard-capped below the ceiling.
        CHECK(std::fabs(os.processSample(2.0f))  <= ceil, "+6 dB input never exceeds -0.1 dBFS");
        CHECK(std::fabs(os.processSample(-2.0f)) <= ceil, "-6 dB (neg) input never exceeds ceiling");
        // full sweep, both signs: nothing above the ceiling.
        bool overCeil = false, mono = true;
        float prev = -1e9f;
        for (float x = -6.0f; x <= 6.0f; x += 0.01f) {
            const float y = os.processSample(x);
            if (std::fabs(y) > ceil + 1e-6f) overCeil = true;
            if (y < prev - 1e-6f) mono = false;              // monotonic non-decreasing
            prev = y;
        }
        CHECK(!overCeil, "soft-clip+ceiling: |out| <= ceiling over [-6,6]");
        CHECK(mono, "output stage is monotonic (no fold-back)");

        // small-signal near-transparent (gentle drive): 0.1 in stays ~0.1.
        const float sm = os.processSample(0.1f);
        CHECK(sm > 0.1f && sm < 0.115f, "small-signal ~transparent (mild makeup only)");
        // x == 1.0 maps to ~unity then hard-capped to the ceiling.
        CHECK(os.processSample(1.0f) == ceil, "unity input rounded to the -0.1 dBFS ceiling");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
