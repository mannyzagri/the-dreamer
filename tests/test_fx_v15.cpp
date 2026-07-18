// test_fx_v15.cpp -- gate for the FX v1.5 stages (FEATURES.md 11): the new
// modfx modes (Dimension/Rotary/Barberpole/Ensemble extras) and the
// standalone LO-FI / WIDTH / TALK stages. Each section proves a specific
// claim: finite/bounded, the effect is audibly present, and its key control
// laws do what they say.
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_fx_v15.cpp
//     /Fo:tests\bin\ /Fe:tests\bin\test_fx_v15.exe

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>

#include "dsp/glue/GlobalFilter.h"
#include "dsp/glue/Ensemble.h"
#include "dsp/glue/Dimension.h"
#include "dsp/glue/Rotary.h"
#include "dsp/glue/Barberpole.h"
#include "dsp/glue/LoFi.h"
#include "dsp/glue/StereoWidth.h"
#include "dsp/glue/Talkbox.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static const double SR = 48000.0;

struct Rng { uint32_t s; Rng(uint32_t x):s(x){} float n(){ s^=s<<13; s^=s>>17; s^=s<<5; return (int32_t)s/2147483648.0f*0.5f; } };

// run a stereo effect fn over N samples of a test tone; report finite, peak,
// and L/R decorrelation (1 - normalized correlation) as a stereo-motion proxy
template <class F>
static void analyze(F fn, int N, bool& finite, float& peak, float& decorr) {
    finite = true; peak = 0.0f;
    double sll = 0, srr = 0, slr = 0;
    for (int i = 0; i < N; ++i) {
        float l = std::sin(2.0 * M_PI * 220.0 * i / SR) * 0.4f;
        float r = l;
        fn(l, r, i);
        if (!std::isfinite(l) || !std::isfinite(r)) finite = false;
        peak = std::fmax(peak, std::fmax(std::fabs(l), std::fabs(r)));
        if (i > N / 4) { sll += (double)l*l; srr += (double)r*r; slr += (double)l*r; }
    }
    const double denom = std::sqrt(sll * srr);
    decorr = denom > 1e-9 ? (float)(1.0 - slr / denom) : 0.0f;
}

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);

    // ---- [dimension] ----------------------------------------------------
    std::printf("[dimension]\n");
    {
        dreamer::Dimension d; d.prepare(SR);
        bool fin; float pk, dc;
        analyze([&](float& l, float& r, int){ d.process(l, r, 0.6f, 0.2f, 1.0f); }, 48000, fin, pk, dc);
        std::printf("  finite=%d peak=%.3f decorr=%.4f\n", (int)fin, pk, dc);
        CHECK(fin, "dimension finite");
        CHECK(pk < 2.0f, "dimension bounded");
        CHECK(dc > 1e-4f, "dimension produces stereo motion");
        // mode change must not blow up (de-click path)
        dreamer::Dimension d2; d2.prepare(SR);
        bool ok = true;
        for (int i = 0; i < 8000; ++i) {
            float l = 0.3f, r = 0.3f;
            d2.process(l, r, (i / 1000) * 0.25f, 0.3f, 1.0f);   // step MODE each 1000
            if (!std::isfinite(l)) ok = false;
        }
        CHECK(ok, "dimension MODE steps finite");
    }

    // ---- [rotary] -------------------------------------------------------
    std::printf("[rotary]\n");
    {
        dreamer::Rotary ro; ro.prepare(SR);
        bool fin; float pk, dc;
        analyze([&](float& l, float& r, int){ ro.process(l, r, 0.9f, 0.5f, 0.4f, 0.8f, 0.8f); }, 96000, fin, pk, dc);
        std::printf("  finite=%d peak=%.3f decorr=%.4f\n", (int)fin, pk, dc);
        CHECK(fin, "rotary finite");
        CHECK(pk < 2.0f, "rotary bounded");
        CHECK(dc > 1e-3f, "rotary produces stereo doppler motion");
    }

    // ---- [barberpole] ---------------------------------------------------
    std::printf("[barberpole]\n");
    {
        // worst case: max feedback, must stay bounded (F1 -- tanh+DC in loop)
        dreamer::Barberpole b; b.prepare(SR);
        Rng rng(7);
        bool fin = true; float pk = 0.0f;
        for (int i = 0; i < 192000; ++i) {
            float l = rng.n(), r = rng.n();
            b.process(l, r, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f);   // dir up, max stages+fb, full mix
            if (!std::isfinite(l) || !std::isfinite(r)) fin = false;
            pk = std::fmax(pk, std::fmax(std::fabs(l), std::fabs(r)));
        }
        std::printf("  max-feedback peak=%.3f finite=%d\n", pk, (int)fin);
        CHECK(fin, "barberpole finite at max feedback");
        CHECK(pk < 8.0f, "barberpole feedback bounded (no runaway)");
    }

    // ---- [lofi] ---------------------------------------------------------
    std::printf("[lofi]\n");
    {
        dreamer::LoFi lf; lf.prepare(SR);
        // output must always land on the 12-bit grain (low nibble zero)
        Rng rng(3);
        int bad = 0; bool fin = true;
        for (int i = 0; i < 48000; ++i) {
            float l = rng.n(), r = rng.n();
            lf.process(l, r, 0.7f, 0.5f, 0.4f, true);        // 6-bit, decimate, compand, alias
            const int32_t q = (int32_t)(l * 32768.0f);
            if (q & 0xF) ++bad;
            if (!std::isfinite(l)) fin = false;
        }
        CHECK(fin, "lofi finite");
        CHECK(bad == 0, "lofi output keeps 12-bit grain (low nibble zero)");
        // bits=0 (12-bit), srate=0, compand=0, alias -> near-transparent-ish
        // (still S&H at factor 1 = passthrough of the held sample)
        dreamer::LoFi lf2; lf2.prepare(SR);
        float maxAbs = 0;
        for (int i = 0; i < 4000; ++i) {
            float l = std::sin(i * 0.05f) * 0.5f, r = l;
            lf2.process(l, r, 0.0f, 0.0f, 0.0f, true);
            maxAbs = std::fmax(maxAbs, std::fabs(l));
        }
        CHECK(maxAbs > 0.3f, "lofi at 12-bit/no-decimate passes signal");
    }

    // ---- [width] --------------------------------------------------------
    std::printf("[width]\n");
    {
        // fully panned input: width 0 collapses toward mono, width 1 keeps side
        auto sideEnergy = [&](float width01) {
            dreamer::StereoWidth w; w.prepare(SR);
            double side = 0;
            for (int i = 0; i < 24000; ++i) {
                float l = std::sin(i * 0.05f) * 0.5f;
                float r = -l;                                // pure side signal
                w.process(l, r, width01, 0.0f, false);
                if (i > 4000) { const float s = 0.5f * (l - r); side += (double)s * s; }
            }
            return std::sqrt(side / 20000.0);
        };
        const double s0 = sideEnergy(0.0), s1 = sideEnergy(1.0);
        std::printf("  side energy: width0=%.4f width1=%.4f\n", s0, s1);
        CHECK(s0 < 0.01, "width 0 collapses side to (near) mono");
        CHECK(s1 > s0 * 4.0, "width 1 preserves the side channel");
        // bass-mono must substantially REDUCE the low-frequency side energy
        // (a pure-side 60 Hz tone) vs not using it -- honest claim for a
        // filter-based bass-mono (it monos lows, doesn't null them perfectly).
        auto lowSide = [&](bool bm) {
            dreamer::StereoWidth w; w.prepare(SR);
            double side = 0;
            for (int i = 0; i < 24000; ++i) {
                float l = std::sin(2.0 * M_PI * 60.0 * i / SR) * 0.5f;
                float r = -l;
                w.process(l, r, 1.0f, 0.0f, bm);
                if (i > 12000) { const float s = 0.5f * (l - r); side += (double)s * s; }
            }
            return std::sqrt(side / 12000.0);
        };
        const double sOff = lowSide(false), sOn = lowSide(true);
        std::printf("  60Hz side: bassmono off=%.4f on=%.4f (%.0f%% cut)\n",
                    sOff, sOn, 100.0 * (1.0 - sOn / sOff));
        CHECK(sOn < sOff * 0.5, "bass-mono cuts low-frequency side energy > 50%");
    }

    // ---- [talk] ---------------------------------------------------------
    std::printf("[talk]\n");
    {
        // vowel A vs vowel I formant sets must shape a bright input differently
        auto bandEnergy = [&](float va01, float vb01) {
            dreamer::Talkbox t; t.prepare(SR);
            Rng rng(11);
            double e = 0; bool fin = true;
            for (int i = 0; i < 48000; ++i) {
                float l = rng.n(), r = rng.n();              // white -> formant filter
                t.process(l, r, va01, vb01, 0.0f, 0.0f, 1.0f);   // static morph=0 -> vowel A
                if (!std::isfinite(l)) fin = false;
                if (i > 8000) e += (double)l * l;
            }
            CHECK(fin, "talk finite");
            return std::sqrt(e / 40000.0);
        };
        const double eA = bandEnergy(0.0f, 0.5f);   // vowel A (index 0)
        const double eI = bandEnergy(0.5f, 0.0f);   // vowel I (index 2)
        std::printf("  formant rms: A=%.4f I=%.4f\n", eA, eI);
        CHECK(eA > 1e-4 && eI > 1e-4, "talk formants pass energy");
        CHECK(std::fabs(eA - eI) > 1e-4, "different vowels shape the signal differently");
    }

    // ---- [ensemble-extras] ----------------------------------------------
    std::printf("[ensemble-extras]\n");
    {
        // TONE (p1) at 1.0 must roll off HF vs 0.0 (darker wet)
        auto hf = [&](float tone) {
            dreamer::Ensemble e; e.prepare(SR);
            double d = 0;
            float prev = 0;
            for (int i = 0; i < 24000; ++i) {
                float l = std::sin(2.0 * M_PI * 4000.0 * i / SR) * 0.4f, r = l;
                e.process(l, r, 1.0f, 0.5f, 1.0f, 0.5f, tone);
                if (i > 4000) { const float df = l - prev; d += (double)df * df; }
                prev = l;
            }
            return std::sqrt(d / 20000.0);
        };
        const double bright = hf(0.0f), dark = hf(1.0f);
        std::printf("  ensemble hf: tone0=%.4f tone1=%.4f\n", bright, dark);
        CHECK(dark < bright, "ensemble TONE rolls off HF");
    }

    // ---- [global-filter] (0.7.1 bug fixes) -------------------------------
    std::printf("[global-filter]\n");
    {
        // F1 LP24 at a low cutoff must attenuate HF (F1-applies regression;
        // the "F1 not applying" report was GUI-side, DSP is proven here)
        dreamer::GlobalFilter f; f.setSampleRate(SR); f.setType(0);
        f.setCutoffRes(300.0, 0.0);
        auto band = [&](double hz) {
            dreamer::GlobalFilter g; g.setSampleRate(SR); g.setType(0);
            g.setCutoffRes(300.0, 0.0);
            double e = 0;
            for (int i = 0; i < 24000; ++i) {
                const float y = g.process((float)std::sin(2.0 * M_PI * hz * i / SR) * 0.5f);
                if (i > 6000) e += (double)y * y;
            }
            return std::sqrt(e / 18000.0);
        };
        const double lo = band(100.0), hi = band(6000.0);
        std::printf("  LP24@300: 100Hz=%.4f 6kHz=%.4f\n", lo, hi);
        CHECK(lo > 0.2 && hi < lo * 0.05, "F1 LP24 attenuates HF (F1 applies)");

        // resonant SVF output must be bounded by the safety net (no overflow
        // into FX/master; the BAL-solo-resonant overflow report)
        dreamer::GlobalFilter g; g.setSampleRate(SR); g.setType(0);
        g.setCutoffRes(900.0, 0.98);
        float pk = 0.0f;
        for (int i = 0; i < SR; ++i) {
            const float y = g.process((float)std::sin(2.0 * M_PI * 900.0 * i / SR) * 0.5f);
            pk = std::fmax(pk, std::fabs(y));
        }
        std::printf("  resonant LP24 peak=%.3f (bounded)\n", pk);
        CHECK(pk < 1.6f, "resonant global filter output bounded (safety net)");

        // small signals pass through the safety transparently (a 0.3 tone in
        // the passband stays ~0.3, not boosted or clipped by the knee)
        dreamer::GlobalFilter t; t.setSampleRate(SR); t.setType(3);   // HP
        t.setCutoffRes(50.0, 0.0);
        float pkT = 0.0f;
        for (int i = 0; i < 8000; ++i) {
            const float x = (float)std::sin(2.0 * M_PI * 2000.0 * i / SR) * 0.3f;  // < 0.8
            const float y = t.process(x);
            if (i > 4000) pkT = std::fmax(pkT, std::fabs(y));
        }
        std::printf("  0.3 passband peak=%.4f (transparent)\n", pkT);
        CHECK(pkT > 0.25f && pkT < 0.35f, "safety transparent for small signals");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
