// test_bank.cpp -- phase-1 gate: bank integrity + oscillator pitch + voice sanity
//
// Build (x64 Native Tools / after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_bank.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_bank.exe
//   tests\bin\test_bank.exe [wav-output-dir]
//
// Checks:
//   [grain] 12-bit left-justified: (sample & 0xF) == 0 for all 78 x 600 samples
//   [pitch] bank Sine at A440, 48 kHz: positive zero crossings over 1 s = 440 +/- 1
//   [voice] bell + string-pad demo patches render finite, bounded, tail-silent
//   [steal] 10 notes on a 2-voice synth survives
// Optional: writes demo_bell.wav / demo_stringpad.wav when an output dir is given.

#include <immintrin.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

#include "dsp/bank/Mode2Voice.h"   // pulls PcmOscillator.h + RomplerBank.h

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

static void writeWav(const char* path, const std::vector<float>& s, int sr) {
    std::vector<int16_t> pcm(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        float v = s[i] < -1.0f ? -1.0f : (s[i] > 1.0f ? 1.0f : s[i]);
        pcm[i] = (int16_t)std::lround(v * 32767.0f);
    }
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::printf("FAIL: cannot open %s\n", path); ++failures; return; }
    uint32_t dataSize = (uint32_t)(pcm.size() * 2), riff = 36 + dataSize;
    uint16_t fmt = 1, ch = 1, bits = 16, ba = 2; uint32_t br = (uint32_t)sr * 2, fsz = 16;
    fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); fwrite(&fsz,4,1,f); fwrite(&fmt,2,1,f); fwrite(&ch,2,1,f);
    fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&dataSize,4,1,f); fwrite(pcm.data(),2,pcm.size(),f);
    fclose(f);
}

static int findWave(const char* cat, const char* name) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank::kWaveforms[i].category, cat) &&
            !std::strcmp(rompler::bank::kWaveforms[i].name, name)) return i;
    return -1;
}

int main(int argc, char** argv) {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ + DAZ, matching ScopedNoDenormals

    // ---- [grain] --------------------------------------------------------
    std::printf("[grain]\n");
    {
        int bad = 0;
        for (int w = 0; w < rompler::bank::kNumWaveforms; ++w)
            for (int i = 0; i < rompler::bank::kCycleLength; ++i)
                if (rompler::bank::kWaveforms[w].samples[i] & 0xF) ++bad;
        CHECK(bad == 0, "12-bit grain: low nibble must be zero across the bank");
        CHECK(rompler::bank::kNumWaveforms == 78, "expected 78 waveforms");
    }

    // ---- [pitch] --------------------------------------------------------
    std::printf("[pitch]\n");
    {
        const int SR = 48000;
        rompler::PcmOscillator osc;
        osc.setSampleRate(SR);
        const int sineIdx = findWave("Basic", "Sine");
        CHECK(sineIdx >= 0, "bank Sine present");
        osc.setWaveform(sineIdx);
        osc.setFrequency(440.0);
        osc.reset();
        int crossings = 0;
        float prev = osc.process();
        for (int i = 1; i < SR; ++i) {
            const float s = osc.process();
            if (prev <= 0.0f && s > 0.0f) ++crossings;
            prev = s;
        }
        std::printf("  zero crossings @440Hz/1s: %d\n", crossings);
        CHECK(crossings >= 439 && crossings <= 441, "pitch 440 +/- 1");
    }

    // ---- [voice] bell ---------------------------------------------------
    std::printf("[voice]\n");
    const int SR = 44100;
    std::vector<float> bell, stringpad;
    {
        rompler::Mode2Synth<8> synth;
        synth.setSampleRate(SR);
        auto& p = synth.patch();
        p.a.waveIndex = findWave("Bell", "FM Bell 4");
        p.a.level = 0.8; p.a.cutoffHz = 9000; p.a.tvfEnvAmount = 3000;
        p.a.tvaA = 0.002; p.a.tvaD = 0.9; p.a.tvaS = 0.0; p.a.tvaR = 0.4;
        p.a.tvfA = 0.001; p.a.tvfD = 0.5; p.a.tvfS = 0.2; p.a.tvfR = 0.4;
        p.b.waveIndex = findWave("Bell", "EP Tine 1");
        p.b.level = 0.7; p.b.fineCents = 4.0; p.b.startOffset = 150;
        p.b.cutoffHz = 4000;
        p.b.tvaA = 0.003; p.b.tvaD = 2.2; p.b.tvaS = 0.0; p.b.tvaR = 0.6;
        CHECK(p.a.waveIndex >= 0 && p.b.waveIndex >= 0, "bell waves found");

        auto play = [&](int note, float vel, double holdSec, double tailSec) {
            synth.noteOn(note, vel);
            for (int i = 0; i < (int)(holdSec * SR); ++i) bell.push_back(synth.process() * 0.5f);
            synth.noteOff(note);
            for (int i = 0; i < (int)(tailSec * SR); ++i) bell.push_back(synth.process() * 0.5f);
        };
        for (int n : {72, 76, 79, 84}) play(n, 0.9f, 0.15, 0.55);
        synth.noteOn(60, 0.8f); synth.noteOn(64, 0.8f); synth.noteOn(67, 0.8f);
        for (int i = 0; i < SR; ++i) bell.push_back(synth.process() * 0.26f);
        synth.noteOff(60); synth.noteOff(64); synth.noteOff(67);
        for (int i = 0; i < SR * 3; ++i) bell.push_back(synth.process() * 0.26f);

        float peak = 0, tail = 0;
        bool finite = true;
        for (float s : bell) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        for (size_t i = bell.size() - 4410; i < bell.size(); ++i) tail = std::fmax(tail, std::fabs(bell[i]));
        std::printf("  bell peak=%.3f tail=%.5f\n", peak, tail);
        CHECK(finite, "bell output finite");
        CHECK(peak > 0.05f && peak <= 1.0f, "bell peak in range");
        CHECK(tail < 0.012f, "bell tail silent");
    }

    // ---- [voice] string pad ---------------------------------------------
    {
        rompler::Mode2Synth<8> pad;
        pad.setSampleRate(SR);
        auto& q = pad.patch();
        q.a.waveIndex = findWave("String", "StringBox 3");
        q.a.fineCents = -7; q.a.cutoffHz = 2500; q.a.tvfEnvAmount = 1500; q.a.resonance = 0.1;
        q.a.tvaA = 0.5; q.a.tvaD = 0.1; q.a.tvaS = 0.9; q.a.tvaR = 1.2;
        q.a.tvfA = 0.8; q.a.tvfD = 0.5; q.a.tvfS = 0.6; q.a.tvfR = 1.0;
        q.b = q.a;
        q.b.waveIndex = findWave("String", "Cello 2");
        q.b.fineCents = +7; q.b.startOffset = 300; q.b.level = 0.8;
        CHECK(q.a.waveIndex >= 0 && q.b.waveIndex >= 0, "pad waves found");

        pad.noteOn(57, 0.7f); pad.noteOn(60, 0.7f); pad.noteOn(64, 0.7f); pad.noteOn(69, 0.7f);
        for (int i = 0; i < SR * 3; ++i) stringpad.push_back(pad.process() * 0.19f);
        for (int n : {57, 60, 64, 69}) pad.noteOff(n);
        // exp release tau=1.2s hits the 1e-4 idle gate after ~11s -- render 12s
        for (int i = 0; i < SR * 12; ++i) stringpad.push_back(pad.process() * 0.19f);

        float peak = 0, tail = 0;
        bool finite = true;
        for (float s : stringpad) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        for (size_t i = stringpad.size() - 4410; i < stringpad.size(); ++i) tail = std::fmax(tail, std::fabs(stringpad[i]));
        std::printf("  pad  peak=%.3f tail=%.5f\n", peak, tail);
        CHECK(finite, "pad output finite");
        CHECK(peak > 0.05f && peak <= 1.0f, "pad peak in range");
        CHECK(tail < 0.012f, "pad tail silent");
    }

    // ---- [steal] --------------------------------------------------------
    std::printf("[steal]\n");
    {
        rompler::Mode2Synth<2> tiny;
        tiny.setSampleRate(SR);
        bool finite = true;
        for (int n = 60; n < 70; ++n) {
            tiny.noteOn(n, 0.8f);
            for (int i = 0; i < 64; ++i) if (!std::isfinite(tiny.process())) finite = false;
        }
        CHECK(finite, "stealing renders finite");
    }

    if (argc > 1) {
        const std::string dir(argv[1]);
        writeWav((dir + "\\demo_bell.wav").c_str(), bell, SR);
        writeWav((dir + "\\demo_stringpad.wav").c_str(), stringpad, SR);
        std::printf("wavs written to %s\n", dir.c_str());
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
