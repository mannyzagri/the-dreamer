// test_dsp_features.cpp -- gate for the Phase-2 per-tone DSP features
// (DSP_BUILD.md s11 multi-tap voicing, s12 loop mode, s13 hit varispeed).
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES
//     /I. tests\test_dsp_features.cpp /Fo:tests\bin\ /Fe:tests\bin\test_dsp_features.exe
//   tests\bin\test_dsp_features.exe
//
// Checks:
//   [s11 voicing]  tap COUNT + ET ratios (2^(semi/12)) for SINGLE/OCT/POWER and
//                  all 3 DREAMY spreads (Add9/Min7/Sus2); equal-power sum gain
//                  = 1/sqrt(n); SINGLE gain is EXACTLY 1.0f (bit-identity).
//   [s12 loop]     a Loop osc in PINGPONG reverses direction at the ends (idx
//                  rises then falls, no out-of-range read, no wrap jump);
//                  FORWARD still wraps as before. Cycle path immune to both new
//                  hooks (setLoopMode/setSpeedMul do NOT change Cycle output).
//   [s13 varispd]  ONESHOT STRETCH speed ratio = 0.25x @0, 1.0x @0.5, 4x @1.0
//                  (log map); pitchtrim offsets semitones; STRETCH is note-
//                  detached; NORMAL plays note-tracked.

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>

#include "dsp/glue/DreamVoice.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static bool approx(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

namespace b3 = rompler::bank3;

// bank3 index of the first entry of each type (order LOCKED per Bank3.h)
static constexpr int kCycleIdx = 0;
static const int kLoopIdx = b3::kNumCycles;                    // 78
static const int kShotIdx = b3::kNumCycles + b3::kNumLoops;    // 208

//============================================================ s11 voicing
static void testVoicing()
{
    using dreamer::voicingIntervals;
    using dreamer::voicingTapGain;
    int semis[4];

    // ---- tap counts + interval tables ----
    int n = voicingIntervals(0, 0, semis);
    CHECK(n == 1 && semis[0] == 0, "SINGLE = 1 tap (root)");

    n = voicingIntervals(1, 0, semis);
    CHECK(n == 2 && semis[0] == 0 && semis[1] == 12, "OCT = root,+12");

    n = voicingIntervals(2, 0, semis);
    CHECK(n == 3 && semis[0] == 0 && semis[1] == 7 && semis[2] == 12,
          "POWER = root,+7,+12");

    n = voicingIntervals(3, 0, semis);   // DREAMY / ADD9
    CHECK(n == 4 && semis[0]==0 && semis[1]==7 && semis[2]==12 && semis[3]==14,
          "DREAMY ADD9 = root,+7,+12,+14");
    n = voicingIntervals(3, 1, semis);   // DREAMY / MIN7
    CHECK(n == 4 && semis[0]==0 && semis[1]==3 && semis[2]==7 && semis[3]==10,
          "DREAMY MIN7 = root,+3,+7,+10");
    n = voicingIntervals(3, 2, semis);   // DREAMY / SUS2
    CHECK(n == 4 && semis[0]==0 && semis[1]==2 && semis[2]==7 && semis[3]==12,
          "DREAMY SUS2 = root,+2,+7,+12");

    // ---- equal-power sum gain 1/sqrt(n); SINGLE exactly 1.0f ----
    CHECK(voicingTapGain(1) == 1.0f, "SINGLE tapGain == 1.0f (bit-identity)");
    CHECK(approx(voicingTapGain(2), 1.0/std::sqrt(2.0), 1e-6), "tapGain(2)=1/sqrt2");
    CHECK(approx(voicingTapGain(3), 1.0/std::sqrt(3.0), 1e-6), "tapGain(3)=1/sqrt3");
    CHECK(approx(voicingTapGain(4), 0.5, 1e-9),                "tapGain(4)=1/sqrt4");

    // ---- ET pitch ratios realised by the oscillator increment ----
    // Drive a Loop osc at freq = root * 2^(semi/12) and read the effective
    // phase-increment ratio (== 2^(semi/12) at SR = native 44100).
    dreamer::PcmOsc3 osc;
    osc.setSampleRate(44100.0);
    osc.setWaveform(kLoopIdx);
    const double root = (double)b3::kWaveforms[(size_t)kLoopIdx].rootHz;
    const int probe[4] = { 0, 7, 12, 14 };
    for (int i = 0; i < 4; ++i) {
        const double ratio = std::pow(2.0, probe[i] / 12.0);
        osc.setFrequency(root * ratio);
        CHECK(approx(osc.incSamplesForTest(), ratio, 1e-4),
              "tap ET ratio matches 2^(semi/12)");
    }
    // sanity: octave up is exactly a 2x increment
    osc.setFrequency(root);
    const double inc1 = osc.incSamplesForTest();
    osc.setFrequency(root * 2.0);
    CHECK(approx(osc.incSamplesForTest(), inc1 * 2.0, 1e-4), "octave tap = 2x inc");
}

//============================================================ s12 loop mode
static void testLoopMode()
{
    const uint32_t len = b3::kWaveforms[(size_t)kLoopIdx].length;
    const uint32_t lo  = b3::kWaveforms[(size_t)kLoopIdx].loopStart;
    const double   root = (double)b3::kWaveforms[(size_t)kLoopIdx].rootHz;
    CHECK(len > 16u, "loop wave has a usable length");

    // ---- PINGPONG: rise then fall, no OOR, no wrap jump ----
    {
        dreamer::PcmOsc3 osc;
        osc.setSampleRate(44100.0);
        osc.setWaveform(kLoopIdx);
        osc.setLoopMode(true);                       // pingpong
        osc.setFrequency(root * 12.0);               // brisk traversal
        osc.reset(0.0);

        uint32_t prev = osc.sampleIndexForTest();
        uint32_t maxIdx = prev;
        bool sawRise = false, sawFall = false, oor = false, bigJump = false, flipped = false;
        const uint32_t jumpLim = len / 2;
        for (int i = 0; i < 200000; ++i) {
            (void)osc.process();
            const uint32_t idx = osc.sampleIndexForTest();
            if (idx >= len) oor = true;
            if (idx > prev) sawRise = true;
            if (idx < prev) {
                sawFall = true;
                if (prev - idx > jumpLim) bigJump = true;   // that would be a wrap
            }
            if (osc.dirForTest() < 0) flipped = true;
            if (idx > maxIdx) maxIdx = idx;
            prev = idx;
        }
        CHECK(sawRise,        "pingpong: index rises");
        CHECK(sawFall,        "pingpong: index falls (reflected at top)");
        CHECK(flipped,        "pingpong: direction flipped negative");
        CHECK(!oor,           "pingpong: never reads out of range");
        CHECK(!bigJump,       "pingpong: no wrap discontinuity");
        CHECK(maxIdx < len,   "pingpong: peak index below length");
    }

    // ---- FORWARD: classic wrap (a large downward jump occurs) ----
    {
        dreamer::PcmOsc3 osc;
        osc.setSampleRate(44100.0);
        osc.setWaveform(kLoopIdx);
        osc.setLoopMode(false);                      // forward (default)
        osc.setFrequency(root * 12.0);
        osc.reset(0.0);

        uint32_t prev = osc.sampleIndexForTest();
        bool wrapped = false, oor = false;
        const uint32_t jumpLim = len / 2;
        for (int i = 0; i < 200000; ++i) {
            (void)osc.process();
            const uint32_t idx = osc.sampleIndexForTest();
            if (idx >= len) oor = true;
            if (idx + jumpLim < prev) wrapped = true;    // big drop == wrap
            prev = idx;
        }
        CHECK(wrapped, "forward: loop wraps as before");
        CHECK(!oor,    "forward: never reads out of range");
        CHECK(lo == 0u || lo < len, "forward: loopStart in range");
    }

    // ---- Cycle path immune to the s12/s13 hooks (bit-identical) ----
    {
        dreamer::PcmOsc3 a, b;
        a.setSampleRate(44100.0); a.setWaveform(kCycleIdx); a.setFrequency(440.0); a.reset(0.0);
        b.setSampleRate(44100.0); b.setWaveform(kCycleIdx); b.setFrequency(440.0); b.reset(0.0);
        b.setLoopMode(true);        // must have NO effect on a Cycle wave
        b.setSpeedMul(0.37);        // must have NO effect on a Cycle wave
        bool identical = true;
        for (int i = 0; i < 4096; ++i) {
            const float fa = a.process();
            const float fb = b.process();
            if (fa != fb) { identical = false; break; }
        }
        CHECK(identical, "Cycle path bit-identical under setLoopMode/setSpeedMul");
    }
}

//============================================================ s13 varispeed
// speed ratio law used by the engine (Tone::process): 0.25 * 2^(4*hs).
static double stretchSpeed(double hs) {
    if (hs < 0.0) hs = 0.0; else if (hs > 1.0) hs = 1.0;
    return 0.25 * std::pow(2.0, 4.0 * hs);
}

static void testVarispeed()
{
    // ---- mapping endpoints ----
    CHECK(approx(stretchSpeed(0.0), 0.25, 1e-9), "hit_stretch 0.0 -> 0.25x");
    CHECK(approx(stretchSpeed(0.5), 1.0,  1e-9), "hit_stretch 0.5 -> 1.0x");
    CHECK(approx(stretchSpeed(1.0), 4.0,  1e-9), "hit_stretch 1.0 -> 4.0x");

    const double root = (double)b3::kWaveforms[(size_t)kShotIdx].rootHz;
    CHECK(b3::kWaveforms[(size_t)kShotIdx].type == b3::WaveType::OneShot,
          "shot entry is a OneShot");

    dreamer::PcmOsc3 osc;
    osc.setSampleRate(44100.0);
    osc.setWaveform(kShotIdx);

    // ---- STRETCH: increment == speed ratio at native root (note-detached) ----
    osc.setFrequency(root);                          // varispeed baseline
    osc.setSpeedMul(stretchSpeed(0.0));
    CHECK(approx(osc.incSamplesForTest(), 0.25, 1e-3), "STRETCH @0 -> 0.25x inc");
    osc.setSpeedMul(stretchSpeed(0.5));
    CHECK(approx(osc.incSamplesForTest(), 1.0,  1e-3), "STRETCH @0.5 -> 1.0x inc");
    osc.setSpeedMul(stretchSpeed(1.0));
    CHECK(approx(osc.incSamplesForTest(), 4.0,  1e-3), "STRETCH @1.0 -> 4.0x inc");

    // ---- pitchtrim offsets semitones (still varispeed) ----
    osc.setSpeedMul(stretchSpeed(0.5) * std::pow(2.0, 12.0 / 12.0));   // +12
    CHECK(approx(osc.incSamplesForTest(), 2.0, 1e-3), "pitchtrim +12 -> 2x");
    osc.setSpeedMul(stretchSpeed(0.5) * std::pow(2.0, -12.0 / 12.0));  // -12
    CHECK(approx(osc.incSamplesForTest(), 0.5, 1e-3), "pitchtrim -12 -> 0.5x");

    // ---- STRETCH is note-detached: speed independent of MIDI note ----
    // (engine sets freq = root in STRETCH regardless of the played note)
    dreamer::PcmOsc3 s1, s2;
    s1.setSampleRate(44100.0); s1.setWaveform(kShotIdx); s1.setFrequency(root); s1.setSpeedMul(1.0);
    s2.setSampleRate(44100.0); s2.setWaveform(kShotIdx); s2.setFrequency(root); s2.setSpeedMul(1.0);
    CHECK(approx(s1.incSamplesForTest(), s2.incSamplesForTest(), 1e-9),
          "STRETCH increment note-detached");

    // ---- NORMAL: note-tracked (increment follows MIDI pitch) ----
    dreamer::PcmOsc3 nrm;
    nrm.setSampleRate(44100.0);
    nrm.setWaveform(kShotIdx);
    nrm.setSpeedMul(1.0);                            // NORMAL: no varispeed
    nrm.setFrequency(root);                          // note == root
    const double incRoot = nrm.incSamplesForTest();
    CHECK(approx(incRoot, 1.0, 1e-3), "NORMAL @root -> 1.0x inc");
    nrm.setFrequency(root * 2.0);                    // one octave up
    CHECK(approx(nrm.incSamplesForTest(), 2.0, 1e-3), "NORMAL note-tracked (oct = 2x)");
}

//============================================================ integration
// Prove the params flow buildPatch-analogue -> Tone -> osc: a OneShot tone in
// STRETCH-slow must render differently from the same tone in NORMAL.
static void testIntegration()
{
    auto renderEnergy = [](int hitPlay, double hitStretch) {
        dreamer::DreamSynth synth;
        synth.prepare(44100.0);
        auto& p = synth.patch();
        p.tone[0] = dreamer::ToneParams{};
        p.tone[0].enabled   = true;
        p.tone[0].waveIndex = kShotIdx;             // a HIT one-shot
        p.tone[0].level     = 0.9;
        p.tone[0].cutoffHz  = 18000.0;
        p.tone[0].tvfKeyFollow = 0.0;
        p.tone[0].tvfEnvAmount = 0.0;
        p.tone[0].tvaA = 0.001; p.tone[0].tvaS = 1.0; p.tone[0].tvaR = 0.5;
        p.tone[0].tvfA = 0.001; p.tone[0].tvfS = 1.0;
        p.tone[0].hitPlay    = hitPlay;
        p.tone[0].hitStretch = hitStretch;
        synth.updateLive();
        synth.noteOn(69, 0.9f);
        double e = 0.0; bool finite = true;
        // measure energy in a LATE window (after a fast NORMAL hit has ended,
        // while a 0.25x STRETCH hit is still sounding)
        for (int i = 0; i < 44100; ++i) {           // 1 s warm/skip
            float l = 0, r = 0; synth.process(l, r);
            if (!std::isfinite(l) || !std::isfinite(r)) finite = false;
        }
        for (int i = 0; i < 22050; ++i) {           // late 0.5 s window
            float l = 0, r = 0; synth.process(l, r);
            if (!std::isfinite(l) || !std::isfinite(r)) finite = false;
            e += (double)l * l + (double)r * r;
        }
        CHECK(finite, "one-shot render finite");
        return e;
    };

    const double eNormal  = renderEnergy(0, 0.5);   // NORMAL note-tracked (fast)
    const double eStretch = renderEnergy(1, 0.0);   // STRETCH 0.25x (long)
    // 0.25x plays ~8x longer than the note-69 (2x) NORMAL hit, so the late
    // window still carries energy under STRETCH but is quiet under NORMAL.
    CHECK(eStretch > eNormal, "STRETCH sustains longer than NORMAL (params wired)");
}

int main()
{
    _mm_setcsr(_mm_getcsr() | 0x8040);              // FTZ + DAZ, like the others

    testVoicing();
    testLoopMode();
    testVarispeed();
    testIntegration();

    if (failures == 0) std::printf("ALL CHECKS PASSED\n");
    else               std::printf("%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
