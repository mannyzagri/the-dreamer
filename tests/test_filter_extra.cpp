// test_filter_extra -- the V1.1 global-filter types (GUI 7-12, dsp/glue/
// FilterExtra.h via dsp/glue/GlobalFilter.h) plus regression that the
// built-in SVF (0-3) and Rhino ports (4-6) still filter and that the output
// safety clamp keeps every type bounded. Drives GlobalFilter exactly like the
// processor does (setType each "block", setCutoffRes at control rate).
//
// Build: cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. \
//        tests\test_filter_extra.cpp /Fe:tests\bin\test_filter_extra.exe

#include <cstdio>
#include <cmath>
#include "dsp/glue/GlobalFilter.h"

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++fails; } } while (0)

static constexpr double SR = 48000.0;

// steady-tone RMS through a type at (cutoffHz,res). Input sine RMS = 0.70711.
static double toneRMS(int type, double cutHz, double res, double sigHz, int os = 1) {
    dreamer::GlobalFilter f; f.setSampleRate(SR); f.setOversample(os);
    double e = 0, ph = 0, inc = 2.0 * M_PI * sigHz / SR; int N = 24000, ctrl = 0;
    for (int i = 0; i < N; ++i) {
        f.setType(type);
        if (ctrl-- <= 0) { ctrl = 15; f.setCutoffRes(cutHz, res); }
        float y = f.process((float)std::sin(ph)); ph += inc;
        if (i > 8000) e += (double)y * y;
    }
    return std::sqrt(e / (N - 8000));
}
static double peakAbs(int type, double cutHz, double res, double sigHz) {
    dreamer::GlobalFilter f; f.setSampleRate(SR); f.setOversample(1);
    double ph = 0, inc = 2.0 * M_PI * sigHz / SR, pk = 0; int N = 24000, ctrl = 0;
    for (int i = 0; i < N; ++i) {
        f.setType(type);
        if (ctrl-- <= 0) { ctrl = 15; f.setCutoffRes(cutHz, res); }
        float y = f.process((float)std::sin(ph)); ph += inc;
        if (i > 8000) pk = std::fmax(pk, std::fabs((double)y));
    }
    return pk;
}

int main() {
    const double IN = 0.70711;   // sine RMS

    std::printf("[svf] built-in 0-3 still filter\n");
    CHECK(toneRMS(0, 800, 0.3, 6000.0) < 0.02, "LP24 kills 6kHz above 800Hz cut");
    CHECK(toneRMS(0, 800, 0.3, 200.0)  > 0.5,  "LP24 passes 200Hz");
    CHECK(toneRMS(3, 800, 0.3, 200.0)  < 0.1,  "HP kills 200Hz below 800Hz cut");

    std::printf("[rhino] ports 4-6 still filter + bounded\n");
    CHECK(toneRMS(4, 500, 0.0, 6000.0) < 0.05, "LIQUID kills 6kHz at 500Hz cut");
    CHECK(toneRMS(5, 500, 0.0, 6000.0) < 0.05, "CLASSIC kills 6kHz at 500Hz cut");
    CHECK(toneRMS(4, 500, 0.0, 200.0)  > 0.4,  "LIQUID passes 200Hz (unity-ish passband)");
    CHECK(toneRMS(5, 500, 0.0, 200.0)  > 0.4,  "CLASSIC passes 200Hz (unity-ish passband)");

    std::printf("[notch] type 7 rejects the cutoff band\n");
    CHECK(toneRMS(7, 1000, 0.7, 1000.0) < 0.05, "NOTCH rejects 1kHz at 1kHz cut");
    CHECK(toneRMS(7, 1000, 0.7, 250.0)  > 0.5,  "NOTCH passes 250Hz");
    CHECK(toneRMS(7, 1000, 0.7, 4000.0) > 0.5,  "NOTCH passes 4kHz");

    std::printf("[comb] types 8/9 comb the spectrum, bounded\n");
    // comb fundamental = fs/D = cut; harmonic (2*cut) reinforced by comb+, dip between
    CHECK(toneRMS(8, 1000, 0.6, 1000.0) > IN * 1.1, "COMB+ reinforces the comb freq");
    CHECK(toneRMS(9, 1000, 0.6, 1000.0) < IN * 0.8, "COMB- is hollow at the comb freq");
    CHECK(peakAbs(8, 1000, 0.9, 1000.0) < 1.6, "COMB+ bounded at max res (safety)");

    std::printf("[nlp] type 10 = notch + lowpass\n");
    CHECK(toneRMS(10, 1000, 0.7, 1000.0) < 0.05, "N+LP notches the cutoff");
    CHECK(toneRMS(10, 1000, 0.7, 6000.0) < 0.15, "N+LP lowpasses well above cutoff");
    CHECK(toneRMS(10, 1000, 0.7, 300.0)  > 0.4,  "N+LP passes the low band");

    std::printf("[formant] type 11 is a vowel bandpass (audible level)\n");
    CHECK(toneRMS(11, 2000, 0.5, 2000.0) > 0.1,  "FORMANT passes a band near its formants");
    CHECK(toneRMS(11, 200,  0.5, 8000.0) < 0.1,  "FORMANT rejects far-out HF");

    std::printf("[allpass] type 12 ~flat magnitude, not silent\n");
    CHECK(toneRMS(12, 1000, 0.5, 500.0)  > 0.4,  "ALLPASS is not silent");
    CHECK(toneRMS(12, 1000, 0.5, 3000.0) > 0.4,  "ALLPASS is roughly unity magnitude");

    std::printf("[bypass] type 13 DREAMPLN passes through (V2)\n");
    CHECK(std::fabs(toneRMS(13, 1000, 0.6, 6000.0) - IN) < 0.02, "DREAMPLN is transparent");

    std::printf("[bounded] every type finite + bounded at max res\n");
    for (int t = 0; t < 14; ++t) {
        const double pk = peakAbs(t, 700, 1.0, 700.0);
        CHECK(std::isfinite(pk) && pk < 1.6, "type bounded < 1.6 at max res");
    }

    if (fails == 0) std::printf("ALL CHECKS PASSED\n");
    else            std::printf("%d CHECK(S) FAILED\n", fails);
    return fails ? 1 : 0;
}
