// test_fx_port.cpp -- phase-3 gate: FX + LFO port parity (donor vs port)
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES /I.
//     tests\test_fx_port.cpp /Fo:tests\bin\ /Fe:tests\bin\test_fx_port.exe
//   tests\bin\test_fx_port.exe
//
// Every ported class is driven against its donor with identical stimulus and
// parameter cadence; output must be bitwise identical (the port is a
// namespace rename). RHINO_TEST_FEATURES=1 exercises the shipped tape/dig
// delay revoice. Plus Lfo semantics: rate-map endpoints, S&H determinism.

#include <immintrin.h>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include "C:/rubber-rhino/Source/dsp/Effects.h"
#include "C:/rubber-rhino/Source/dsp/ReturnFilter.h"
#include "C:/rubber-rhino/Source/dsp/ModFx.h"
#include "C:/rubber-rhino/Source/dsp/SpringReverb.h"
#include "C:/rubber-rhino/Source/dsp/Instability.h"
#include "C:/rubber-rhino/Source/dsp/Dynamics.h"
#include "C:/rubber-rhino/Source/dsp/Lfo.h"

#include "dsp/ported/fx/Effects.h"
#include "dsp/ported/fx/ReturnFilter.h"
#include "dsp/ported/fx/ModFx.h"
#include "dsp/ported/fx/SpringReverb.h"
#include "dsp/ported/fx/Instability.h"
#include "dsp/ported/fx/Dynamics.h"
#include "dsp/ported/RhinoLfo.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

struct Noise {
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed) {}
    float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (static_cast<int32_t>(s) / 2147483648.0f) * 0.5f;
    }
};

static const double SR = 48000.0;
static const int    N  = 48000;

int main() {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ + DAZ

    // ---- Distortion (6 modes) ------------------------------------------
    std::printf("[distortion]\n");
    for (int m = 0; m <= 5; ++m) {
        rhino::Distortion a; dreamer::Distortion b;
        a.prepare(SR); b.prepare(SR);
        a.set((rhino::Distortion::Mode)m, 0.6f, 0.35f);
        b.set((dreamer::Distortion::Mode)m, 0.6f, 0.35f);
        Noise na(1u), nb(1u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            const float x = na.next(); (void)nb.next();
            maxDiff = std::fmax(maxDiff, std::fabs(
                a.processSample(x, 0.8f) - b.processSample(x, 0.8f)));
        }
        CHECK(maxDiff == 0.0f, "Distortion mode bitwise identical");
    }

    // ---- DCBlocker + Truncate ------------------------------------------
    std::printf("[dcblock+truncate]\n");
    {
        rhino::DCBlocker a; dreamer::DCBlocker b;
        a.prepare(SR); b.prepare(SR);
        Noise na(2u), nb(2u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            const float x = na.next() + 0.1f; (void)nb.next();
            const float ya = rhino::Truncate16::processSample(a.processSample(x));
            const float yb = dreamer::Truncate16::processSample(b.processSample(x));
            maxDiff = std::fmax(maxDiff, std::fabs(ya - yb));
            maxDiff = std::fmax(maxDiff, std::fabs(
                rhino::Truncate12::processSample(x) - dreamer::Truncate12::processSample(x)));
        }
        CHECK(maxDiff == 0.0f, "DCBlocker+Truncate16/12 bitwise identical");
    }

    // ---- StereoDelay (3 types, synced + free + nudge) ------------------
    std::printf("[delay]\n");
    for (int t = 0; t <= 2; ++t) {
        rhino::StereoDelay a; dreamer::StereoDelay b;
        a.prepare(SR); b.prepare(SR);
        a.setType((rhino::StereoDelay::Type)t);
        b.setType((dreamer::StereoDelay::Type)t);
        a.setTimeSynced(3, 120.0); b.setTimeSynced(3, 120.0);
        Noise na(3u), nb(3u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N * 2; ++i) {
            if (i == N) { a.setTimeFree(313.0f); b.setTimeFree(313.0f); }
            if (i == N + N / 2) { a.nudgeTime(1.01f); b.nudgeTime(1.01f); }
            const float x = na.next(); (void)nb.next();
            float wla = 0, wra = 0, wlb = 0, wrb = 0;
            a.processSampleWet(x, 0.45f, wla, wra);
            b.processSampleWet(x, 0.45f, wlb, wrb);
            maxDiff = std::fmax(maxDiff, std::fmax(std::fabs(wla - wlb), std::fabs(wra - wrb)));
        }
        CHECK(maxDiff == 0.0f, "StereoDelay type bitwise identical");
    }

    // ---- ReturnFilter (2 modes x 3 slopes) -----------------------------
    std::printf("[returnfilter]\n");
    for (int mode = 0; mode <= 1; ++mode) {
        for (int slope = 0; slope <= 2; ++slope) {
            rhino::ReturnFilter a; dreamer::ReturnFilter b;
            a.prepare(SR, (rhino::ReturnFilter::Mode)mode);
            b.prepare(SR, (dreamer::ReturnFilter::Mode)mode);
            a.setParams(slope, 0.6f); b.setParams(slope, 0.6f);
            Noise na(4u), nb(4u);
            float maxDiff = 0.0f;
            for (int i = 0; i < N; ++i) {
                const float x = na.next(); (void)nb.next();
                for (int ch = 0; ch < 2; ++ch)
                    maxDiff = std::fmax(maxDiff, std::fabs(
                        a.processSample(ch, x) - b.processSample(ch, x)));
            }
            CHECK(maxDiff == 0.0f, "ReturnFilter bitwise identical");
        }
    }

    // ---- ModDelayFx (chorus + flanger settings) + Phaser ---------------
    std::printf("[modfx]\n");
    {
        rhino::ModDelayFx a; dreamer::ModDelayFx b;
        a.prepare(SR, 30.0f); b.prepare(SR, 30.0f);
        Noise na(5u), nb(5u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            float la = na.next(), ra = na.next();
            float lb = la, rb = ra; (void)nb.next(); (void)nb.next();
            // chorus-style then flanger-style, alternating halves
            if (i < N / 2) {
                a.process(la, ra, 4.0f, 2.0f, 0.8f, 0.0f, 0.5f);
                b.process(lb, rb, 4.0f, 2.0f, 0.8f, 0.0f, 0.5f);
            } else {
                a.process(la, ra, 0.5f, 1.5f, 0.4f, 0.6f, 0.5f);
                b.process(lb, rb, 0.5f, 1.5f, 0.4f, 0.6f, 0.5f);
            }
            maxDiff = std::fmax(maxDiff, std::fmax(std::fabs(la - lb), std::fabs(ra - rb)));
        }
        CHECK(maxDiff == 0.0f, "ModDelayFx bitwise identical");

        rhino::Phaser pa; dreamer::Phaser pb;
        pa.prepare(SR); pb.prepare(SR);
        Noise nc(6u), nd(6u);
        maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            float la = nc.next(), ra = nc.next();
            float lb = la, rb = ra; (void)nd.next(); (void)nd.next();
            pa.process(la, ra, 0.7f, 0.9f, 0.6f);
            pb.process(lb, rb, 0.7f, 0.9f, 0.6f);
            maxDiff = std::fmax(maxDiff, std::fmax(std::fabs(la - lb), std::fabs(ra - rb)));
        }
        CHECK(maxDiff == 0.0f, "Phaser bitwise identical");
    }

    // ---- SpringReverb ---------------------------------------------------
    std::printf("[spring]\n");
    {
        rhino::SpringReverb a; dreamer::SpringReverb b;
        a.prepare(SR); b.prepare(SR);
        a.setParams(0.6f, 0.4f); b.setParams(0.6f, 0.4f);
        Noise na(7u), nb(7u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            const float x = na.next(); (void)nb.next();
            float la = 0, ra = 0, lb = 0, rb = 0;
            a.processSample(x, la, ra);
            b.processSample(x, lb, rb);
            maxDiff = std::fmax(maxDiff, std::fmax(std::fabs(la - lb), std::fabs(ra - rb)));
        }
        CHECK(maxDiff == 0.0f, "SpringReverb bitwise identical");
    }

    // ---- Instability ----------------------------------------------------
    std::printf("[instability]\n");
    {
        rhino::Instability a; dreamer::Instability b;
        a.prepare(SR, 0x1955B52Fu); b.prepare(SR, 0x1955B52Fu);
        a.setAmounts(0.3f, 0.4f); b.setAmounts(0.3f, 0.4f);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            a.tick(); b.tick();
            const auto da = a.drift(); const auto db = b.drift();
            maxDiff = std::fmax(maxDiff, std::fabs(da.tune  - db.tune));
            maxDiff = std::fmax(maxDiff, std::fabs(da.cutoff - db.cutoff));
            maxDiff = std::fmax(maxDiff, std::fabs(da.delay - db.delay));
            for (int ch = 0; ch < 2; ++ch)
                maxDiff = std::fmax(maxDiff, std::fabs(a.hiss(ch) - b.hiss(ch)));
        }
        CHECK(maxDiff == 0.0f, "Instability bitwise identical");
        // condition=stability=1.0 must be bit-transparent (zero drift, zero hiss)
        dreamer::Instability c; c.prepare(SR, 0x1955B52Fu); c.setAmounts(1.0f, 1.0f);
        float any = 0.0f;
        for (int i = 0; i < 4096; ++i) {
            c.tick();
            const auto d = c.drift();
            any = std::fmax(any, std::fabs(d.tune) + std::fabs(d.cutoff)
                                 + std::fabs(d.delay) + std::fabs(c.hiss(0)));
        }
        CHECK(any == 0.0f, "Instability pristine (1.0/1.0) is silent");
    }

    // ---- CompLimiter + BrickwallClip ------------------------------------
    std::printf("[dynamics]\n");
    {
        rhino::CompLimiter a; dreamer::CompLimiter b;
        a.prepare(SR); b.prepare(SR);
        a.setParams(true, false, -18.0f, 4.0f, 3.0f);
        b.setParams(true, false, -18.0f, 4.0f, 3.0f);
        Noise na(8u), nb(8u);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            float la = na.next() * 1.5f, ra = na.next() * 1.5f;
            float lb = la, rb = ra; (void)nb.next(); (void)nb.next();
            a.processSample(la, ra); b.processSample(lb, rb);
            maxDiff = std::fmax(maxDiff, std::fmax(std::fabs(la - lb), std::fabs(ra - rb)));
            maxDiff = std::fmax(maxDiff, std::fabs(
                rhino::BrickwallClip::processSample(la * 2.0f)
                - dreamer::BrickwallClip::processSample(lb * 2.0f)));
        }
        CHECK(maxDiff == 0.0f, "CompLimiter+BrickwallClip bitwise identical");
    }

    // ---- Lfo -------------------------------------------------------------
    std::printf("[lfo]\n");
    for (int shape = 0; shape <= 4; ++shape) {
        rhino::Lfo a; dreamer::Lfo b;
        a.prepare(SR, 77u); b.prepare(SR, 77u);
        a.setShape(shape); b.setShape(shape);
        a.setRateHz(3.7f); b.setRateHz(3.7f);
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            a.tick(); b.tick();
            maxDiff = std::fmax(maxDiff, std::fabs(a.value() - b.value()));
        }
        CHECK(maxDiff == 0.0f, "Lfo shape bitwise identical");
    }
    {
        const float lo = dreamer::Lfo::rateHzFromParam(0.0f);
        const float hi = dreamer::Lfo::rateHzFromParam(100.0f);
        std::printf("  rate map: 0 -> %.4f Hz, 100 -> %.2f Hz\n", (double)lo, (double)hi);
        CHECK(std::fabs(lo - 0.05f) < 1e-4f, "rate map low endpoint 0.05 Hz");
        CHECK(std::fabs(hi - 30.0f) < 1e-3f, "rate map high endpoint 30 Hz");
        // S&H determinism: same seed -> same stream after reset
        dreamer::Lfo x, y;
        x.prepare(SR, 12345u); y.prepare(SR, 12345u);
        x.setShape(4); y.setShape(4);
        x.setRateHz(25.0f); y.setRateHz(25.0f);
        bool same = true;
        for (int i = 0; i < N; ++i) { x.tick(); y.tick(); if (x.value() != y.value()) same = false; }
        x.reset(); y.reset();
        for (int i = 0; i < N; ++i) { x.tick(); y.tick(); if (x.value() != y.value()) same = false; }
        CHECK(same, "S&H deterministic per seed across reset");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
