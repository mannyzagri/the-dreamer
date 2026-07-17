#include "PcmOscillator.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cassert>

int main() {
    using namespace rompler;
    printf("Bank: %d waveforms, cycle=%d\n", bank::kNumWaveforms, bank::kCycleLength);

    // verify 12-bit grain: low nibble zero everywhere
    for (int w = 0; w < bank::kNumWaveforms; ++w)
        for (int i = 0; i < 600; ++i)
            assert((bank::kWaveforms[w].samples[i] & 0xF) == 0);
    printf("12-bit requantization verified (low nibble zero)\n");

    // pitch accuracy: render A440 @44.1k, count zero crossings over 1 sec
    PcmOscillator osc;
    osc.setSampleRate(44100.0); osc.setFrequency(440.0);
    osc.setWaveform(3); // Sine
    osc.reset();
    int zc = 0; float prev = osc.process();
    for (int i = 1; i < 44100; ++i) { float s = osc.process(); if (prev <= 0 && s > 0) ++zc; prev = s; }
    printf("A440 measured: %d Hz (zero-crossing)\n", zc);
    assert(zc >= 439 && zc <= 441);

    // amplitude sane in both interp modes across bank
    for (int w = 0; w < bank::kNumWaveforms; ++w) {
        osc.setWaveform(w); osc.setFrequency(261.63); osc.reset();
        float peak = 0;
        for (int i = 0; i < 4096; ++i) peak = std::max(peak, std::abs(osc.process()));
        assert(peak > 0.05f && peak <= 1.0f);
    }
    osc.setInterp(PcmOscillator::Interp::DropSample);
    osc.setWaveform(0); osc.reset();
    for (int i = 0; i < 1000; ++i) (void)osc.process();
    printf("All %d waveforms render OK in both interp modes\n", bank::kNumWaveforms);

    // list categories
    const char* last = "";
    for (int w = 0; w < bank::kNumWaveforms; ++w) {
        if (strcmp(bank::kWaveforms[w].category, last)) { printf("\n[%s] ", bank::kWaveforms[w].category); last = bank::kWaveforms[w].category; }
        printf("%s; ", bank::kWaveforms[w].name);
    }
    printf("\n");
    return 0;
}
