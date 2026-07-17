#include "Mode2Voice.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cassert>
#include <cmath>

// minimal WAV writer
static void writeWav(const char* path, const std::vector<float>& s, int sr) {
    std::vector<int16_t> pcm(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        float v = std::max(-1.0f, std::min(1.0f, s[i]));
        pcm[i] = (int16_t)std::lround(v * 32767.0f);
    }
    FILE* f = fopen(path, "wb");
    uint32_t dataSize = (uint32_t)(pcm.size() * 2), riff = 36 + dataSize;
    uint16_t fmt = 1, ch = 1, bits = 16, ba = 2; uint32_t br = sr * 2, fsz = 16;
    fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); fwrite(&fsz,4,1,f); fwrite(&fmt,2,1,f); fwrite(&ch,2,1,f);
    fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&dataSize,4,1,f); fwrite(pcm.data(),2,pcm.size(),f);
    fclose(f);
}

int findWave(const char* cat, const char* name) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!strcmp(rompler::bank::kWaveforms[i].category, cat) &&
            !strcmp(rompler::bank::kWaveforms[i].name, name)) return i;
    return -1;
}

int main() {
    using namespace rompler;
    const int SR = 44100;

    // ---- classic ROMpler bell: bright sparse partial (fast decay) over EP tine (slow)
    Mode2Synth<8> synth;
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
    assert(p.a.waveIndex >= 0 && p.b.waveIndex >= 0);

    std::vector<float> out;
    auto play = [&](int note, float vel, double holdSec, double tailSec) {
        synth.noteOn(note, vel);
        for (int i = 0; i < (int)(holdSec * SR); ++i) out.push_back(synth.process() * 0.5f);
        synth.noteOff(note);
        for (int i = 0; i < (int)(tailSec * SR); ++i) out.push_back(synth.process() * 0.5f);
    };
    for (int n : {72, 76, 79, 84}) play(n, 0.9f, 0.15, 0.55);
    // chord
    synth.noteOn(60, 0.8f); synth.noteOn(64, 0.8f); synth.noteOn(67, 0.8f);
    for (int i = 0; i < SR; ++i) out.push_back(synth.process() * 0.26f);
    synth.noteOff(60); synth.noteOff(64); synth.noteOff(67);
    for (int i = 0; i < SR * 3; ++i) out.push_back(synth.process() * 0.26f);
    writeWav("demo_bell.wav", out, SR);
    printf("demo_bell.wav: %.1fs\n", out.size() / (double)SR);

    // ---- string pad: detuned StringBox pair, slow attack
    Mode2Synth<8> pad;
    pad.setSampleRate(SR);
    auto& q = pad.patch();
    q.a.waveIndex = findWave("String", "StringBox 3");
    q.a.fineCents = -7; q.a.cutoffHz = 2500; q.a.tvfEnvAmount = 1500; q.a.resonance = 0.1;
    q.a.tvaA = 0.5; q.a.tvaD = 0.1; q.a.tvaS = 0.9; q.a.tvaR = 1.2;
    q.a.tvfA = 0.8; q.a.tvfD = 0.5; q.a.tvfS = 0.6; q.a.tvfR = 1.0;
    q.b = q.a;
    q.b.waveIndex = findWave("String", "Cello 2");
    q.b.fineCents = +7; q.b.startOffset = 300; q.b.level = 0.8;
    std::vector<float> out2;
    pad.noteOn(57, 0.7f); pad.noteOn(60, 0.7f); pad.noteOn(64, 0.7f); pad.noteOn(69, 0.7f);
    for (int i = 0; i < SR * 3; ++i) out2.push_back(pad.process() * 0.19f);
    for (int n : {57, 60, 64, 69}) pad.noteOff(n);
    for (int i = 0; i < SR * 2; ++i) out2.push_back(pad.process() * 0.19f);
    writeWav("demo_stringpad.wav", out2, SR);
    printf("demo_stringpad.wav: %.1fs\n", out2.size() / (double)SR);

    // ---- sanity: tail silence, voice stealing, no NaN/inf
    float peak = 0, tail = 0;
    for (float s : out) { assert(std::isfinite(s)); peak = std::max(peak, std::abs(s)); }
    for (size_t i = out.size() - 4410; i < out.size(); ++i) tail = std::max(tail, std::abs(out[i]));
    printf("bell peak=%.3f tail=%.5f\n", peak, tail);
    assert(peak > 0.05f && peak <= 1.0f && tail < 0.012f);

    Mode2Synth<2> tiny; tiny.setSampleRate(SR);
    tiny.patch() = p;
    for (int n = 60; n < 70; ++n) { tiny.noteOn(n, 0.8f); for (int i = 0; i < 64; ++i) (void)tiny.process(); }
    printf("voice stealing OK (10 notes on 2 voices)\n");
    return 0;
}
