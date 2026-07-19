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

// steady-tone RMS through a type at (cutoffHz,res,[morph]). Input sine RMS = 0.70711.
static double toneRMS(int type, double cutHz, double res, double sigHz, int os = 1, double morph = 0.0) {
    dreamer::GlobalFilter f; f.setSampleRate(SR); f.setOversample(os);
    double e = 0, ph = 0, inc = 2.0 * M_PI * sigHz / SR; int N = 24000, ctrl = 0;
    for (int i = 0; i < N; ++i) {
        f.setType(type);
        if (ctrl-- <= 0) { ctrl = 15; f.setCutoffRes(cutHz, res); f.setMorph((float)morph); }
        float y = f.process((float)std::sin(ph)); ph += inc;
        if (i > 8000) e += (double)y * y;
    }
    return std::sqrt(e / (N - 8000));
}
static double peakAbs(int type, double cutHz, double res, double sigHz, double morph = 0.0) {
    dreamer::GlobalFilter f; f.setSampleRate(SR); f.setOversample(1);
    double ph = 0, inc = 2.0 * M_PI * sigHz / SR, pk = 0; int N = 24000, ctrl = 0;
    for (int i = 0; i < N; ++i) {
        f.setType(type);
        if (ctrl-- <= 0) { ctrl = 15; f.setCutoffRes(cutHz, res); f.setMorph((float)morph); }
        float y = f.process((float)std::sin(ph)); ph += inc;
        if (i > 8000) pk = std::fmax(pk, std::fabs((double)y));
    }
    return pk;
}
static const double IN_RMS = 0.70711;
// resonant peak (dB) = max gain over a coarse freq sweep around the cutoff.
static double resPeakDb(int type, double cutHz, double res, double morph = 0.0) {
    double pk = 0;
    for (double fr = 0.5; fr <= 2.0; fr *= 1.03)
        pk = std::fmax(pk, toneRMS(type, cutHz, res, cutHz * fr, 1, morph) / IN_RMS);
    return 20.0 * std::log10(pk + 1e-12);
}
// min gain (dB) over a log freq sweep -- deepest notch (allpass) / floor.
static double minGainDb(int type, double cutHz, double res, double morph = 0.0) {
    double lo = 1e9;
    for (double f = 80.0; f <= SR * 0.45; f *= 1.03)
        lo = std::fmin(lo, toneRMS(type, cutHz, res, f, 1, morph) / IN_RMS);
    return 20.0 * std::log10(lo + 1e-12);
}
static double maxGainDb(int type, double cutHz, double res, double morph = 0.0) {
    double hi = 0;
    for (double f = 80.0; f <= SR * 0.45; f *= 1.03)
        hi = std::fmax(hi, toneRMS(type, cutHz, res, f, 1, morph) / IN_RMS);
    return 20.0 * std::log10(hi + 1e-12);
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
    // fractional delay: fine cutoff steps must produce distinct responses --
    // integer delay plateaued (many adjacent cutoffs -> same fs/N -> identical
    // = the audible stepping). Expect ~all 20 steps distinct now.
    {
        int distinct = 0; double prev = -1e9;
        for (int k = 0; k < 20; ++k) {
            const double v = toneRMS(8, 980.0 + k * 3.0, 0.6, 1000.0);
            if (std::fabs(v - prev) > 1e-5) ++distinct;
            prev = v;
        }
        std::printf("  COMB+ distinct responses over 20 fine cutoff steps: %d/20\n", distinct);
        CHECK(distinct >= 18, "COMB tuning is continuous (fractional delay, no plateaus)");
    }

    std::printf("[nlp] type 10 = notch + lowpass\n");
    CHECK(toneRMS(10, 1000, 0.7, 1000.0) < 0.05, "N+LP notches the cutoff");
    CHECK(toneRMS(10, 1000, 0.7, 6000.0) < 0.15, "N+LP lowpasses well above cutoff");
    CHECK(toneRMS(10, 1000, 0.7, 300.0)  > 0.4,  "N+LP passes the low band");

    std::printf("[formant] type 11 is a vowel bandpass (audible level)\n");
    CHECK(toneRMS(11, 2000, 0.5, 2000.0) > 0.1,  "FORMANT passes a band near its formants");
    CHECK(toneRMS(11, 200,  0.5, 8000.0) < 0.1,  "FORMANT rejects far-out HF");

    std::printf("[allpass] type 12 resonant swept phaser: RES now audibly acts\n");
    CHECK(toneRMS(12, 1000, 0.5, 500.0)  > 0.35, "ALLPASS is not silent");
    // FIX2: the old build was a single static notch with RES INERT (measured
    // identical at res 0/0.5/1.0, and -1.9 dB shallow at 400 Hz -- inaudible).
    // Now: deep sweeping notches at RES=0, resonant emphasis (peaks) as RES rises.
    {
        const double d400  = minGainDb(12, 400.0, 0.0);   // deep notch at RES 0
        const double pk0   = maxGainDb(12, 1000.0, 0.0);  // ~0 dB, no resonance
        const double pk1   = maxGainDb(12, 1000.0, 1.0);  // resonant peaks appear
        std::printf("  400Hz notch(res0)=%.1f dB  peak(res0)=%.1f  peak(res1)=%.1f dB\n",
                    d400, pk0, pk1);
        CHECK(d400 < -8.0, "ALLPASS notches deeply at 400Hz (was -1.9 dB, inaudible)");
        CHECK(pk1 - pk0 > 3.0, "RES now audibly adds resonant emphasis (was inert)");
    }

    std::printf("[ladder-curve] FIX1 res->q perceptual curve spreads the ring\n");
    // Rhino q-law types (LIQUID 4 / CLASSIC 5): the resonant peak used to be
    // dead until res~0.6 then ring hard near 1 (linear q law). The res^0.35
    // curve in GlobalFilter linearises the onset: res=0.4 already rings, and
    // the peak rises monotonically across the knob.
    {
        double prev = -1e9; int mono = 1;
        std::printf("  CLASSIC resonant peak vs res:");
        for (int s = 0; s <= 10; ++s) {
            const double p = resPeakDb(5, 1000.0, s / 10.0);
            if (s % 2 == 0) std::printf(" %.0f%%=%.1f", s * 10.0, p);
            if (p < prev - 0.4) mono = 0;                 // allow tiny non-monotone noise
            prev = p;
        }
        std::printf(" dB\n");
        CHECK(mono, "CLASSIC resonance rises monotonically with the knob (smooth onset)");
        CHECK(resPeakDb(5, 1000.0, 0.4) - resPeakDb(5, 1000.0, 0.0) > 3.0,
              "res=0.4 already rings (curve spread the onset off the top)");
        CHECK(resPeakDb(4, 1000.0, 0.4) - resPeakDb(4, 1000.0, 0.0) > 2.0,
              "LIQUID onset spread too (shares the q law + curve)");
    }

    std::printf("[zplane] type 13 DREAMPLN E-MU Z-plane morph: stable + morph changes response\n");
    {
        // morph AUDIBLY changes the response: max |gain(morph0)-gain(morph1)|.
        double md = 0;
        for (double f = 120.0; f <= 7000.0; f *= 1.04)
            md = std::fmax(md, std::fabs(toneRMS(13, 1200, 0.6, f, 1, 0.0)
                                       - toneRMS(13, 1200, 0.6, f, 1, 1.0)) / IN_RMS * 100.0);
        std::printf("  max |morph0-morph1| response delta = %.1f%% of input\n", md);
        CHECK(md > 15.0, "MORPH audibly changes the Z-plane response");
        // full-level resonant formant peaks: measure AT the morph-0 bell
        // centres (frame0 freqs x cutoffScale=1200/1000). Use a LOW-amplitude
        // probe (0.2) so the shared output safety() soft-clip stays transparent
        // -- a 0-dBFS sine would drive the +11 dB bell into the limiter and
        // understate the true small-signal response. Coarse log sweeps also
        // undersample the narrow peaks, hence exact centres.
        double zpk = -1e9; const double A = 0.2;
        for (double f : {216.0, 360.0, 624.0, 984.0, 1500.0, 2280.0}) {
            dreamer::GlobalFilter g; g.setSampleRate(SR); g.setOversample(1);
            double e = 0, ph = 0, inc = 2.0 * M_PI * f / SR; int ctrl = 0;
            for (int i = 0; i < 24000; ++i) {
                g.setType(13);
                if (ctrl-- <= 0) { ctrl = 15; g.setCutoffRes(1200, 0.6); g.setMorph(0.0f); }
                float y = g.process((float)(A * std::sin(ph))); ph += inc;
                if (i > 8000) e += (double)y * y;
            }
            const double gain = std::sqrt(e / 16000.0) / (A * IN_RMS);
            zpk = std::fmax(zpk, 20.0 * std::log10(gain + 1e-12));
        }
        std::printf("  Z-plane small-signal peak at morph0 bell centres = %.1f dB\n", zpk);
        CHECK(zpk > 6.0, "Z-plane has audible resonant formant peaks");
        CHECK(toneRMS(13, 1200, 0.0, 1000.0) > 0.2, "Z-plane passes signal (not silent)");
        // stability: no NaN/Inf, bounded across morph x cutoff x res sweeps
        int bad = 0;
        for (double m = 0.0; m <= 1.001; m += 0.2)
            for (double c : {120.0, 500.0, 1500.0, 6000.0})
                for (double r : {0.0, 0.5, 1.0}) {
                    const double pk = peakAbs(13, c, r, c, m);
                    if (!std::isfinite(pk) || pk >= 1.6) bad++;
                }
        CHECK(bad == 0, "Z-plane finite + bounded across morph x cutoff x res");
    }

    std::printf("[bounded] every type finite + bounded at max res\n");
    for (int t = 0; t < 14; ++t) {
        const double pk = peakAbs(t, 700, 1.0, 700.0);
        CHECK(std::isfinite(pk) && pk < 1.6, "type bounded < 1.6 at max res");
    }

    if (fails == 0) std::printf("ALL CHECKS PASSED\n");
    else            std::printf("%d CHECK(S) FAILED\n", fails);
    return fails ? 1 : 0;
}
