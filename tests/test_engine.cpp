// test_engine.cpp -- gate for the v2 4-tone engine (FEATURES.md CORE set)
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES /I.
//     tests\test_engine.cpp /Fo:tests\bin\ /Fe:tests\bin\test_engine.exe
//   tests\bin\test_engine.exe [wav-output-dir]
//
// Checks:
//   [shaper]  tables carry the 12-bit grain; shaper output keeps the
//             (q & 0xF) == 0 invariant; OFF / depth-0 is bit-exact dry;
//             output bounded
//   [svf]     ToneSvf mode sanity: LP passes DC / kills HF, HP the reverse,
//             BP kills both ends
//   [chord]   24-voice x 4-tone chord: finite, zero allocations, tail silence
//   [steal]   oldest-note stealing survives the v2 rebuild
//   [vector]  v4 law: int=0 static; int=1 aligned = full, orthogonal/opposed
//             = silent; compass recovery
//   [aux]     AUX env -> PITCH (+12 st at amt 0.5) doubles frequency
//   [glfo]    depth-0 taps are a bit-exact no-op (hermetic synths)
//   [matrix]  WHEEL -> PITCH doubles frequency; G-LFO -> CUT1 swings the
//             exported octave offset; VEC PHS self-route is clamped
//   [pan]     hard-left tone leaves the right bus silent
//   [render]  4-tone vector pad demo renders finite (wav on request)

#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <new>

static bool g_countAllocs = false;
static long g_allocCount  = 0;
void* operator new(std::size_t n) {
    if (g_countAllocs) ++g_allocCount;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    if (g_countAllocs) ++g_allocCount;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

#include "dsp/glue/DreamVoice.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static const int SR = 48000;

static int findWave(const char* cat, const char* name) {
    for (int i = 0; i < rompler::bank::kNumWaveforms; ++i)
        if (!std::strcmp(rompler::bank::kWaveforms[i].category, cat) &&
            !std::strcmp(rompler::bank::kWaveforms[i].name, name)) return i;
    return -1;
}

static dreamer::DreamPatch sinePatch() {
    dreamer::DreamPatch p;
    p.tone[0].enabled = true;
    p.tone[0].waveIndex = findWave("Basic", "Sine");
    p.tone[0].level = 0.8;
    p.tone[0].cutoffHz = 18000.0;
    p.tone[0].tvfKeyFollow = 0.0;
    p.tone[0].tvfEnvAmount = 0.0;
    p.tone[0].tvaA = 0.001; p.tone[0].tvaS = 1.0; p.tone[0].tvaR = 0.05;
    p.tone[0].tvfA = 0.001; p.tone[0].tvfS = 1.0;
    return p;
}

static int countCrossings(const std::vector<float>& s, size_t from) {
    int c = 0;
    for (size_t i = from + 1; i < s.size(); ++i)
        if (s[i - 1] <= 0.0f && s[i] > 0.0f) ++c;
    return c;
}

// hermetic 1-second mono (left) render of a fresh synth
static std::vector<float> render1s(const dreamer::DreamPatch& patch, int note = 69,
                                   float wheel = 0.0f) {
    auto s = std::make_unique<dreamer::DreamSynth>();
    s->prepare(SR);
    s->patch() = patch;
    s->updateLive();
    s->setWheel(wheel);
    s->noteOn(note, 0.8f);
    std::vector<float> out(SR);
    for (int i = 0; i < SR; ++i) {
        float l = 0, r = 0;
        s->process(0.0f, 0.0f, l, r);
        out[(size_t)i] = l;
    }
    return out;
}

static void writeWav(const char* path, const std::vector<float>& L,
                     const std::vector<float>& R, int sr) {
    std::vector<int16_t> pcm(L.size() * 2);
    for (size_t i = 0; i < L.size(); ++i) {
        float lv = L[i] < -1 ? -1.f : (L[i] > 1 ? 1.f : L[i]);
        float rv = R[i] < -1 ? -1.f : (R[i] > 1 ? 1.f : R[i]);
        pcm[i * 2]     = (int16_t)std::lround(lv * 32767.0f);
        pcm[i * 2 + 1] = (int16_t)std::lround(rv * 32767.0f);
    }
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::printf("FAIL: cannot open %s\n", path); ++failures; return; }
    uint32_t dataSize = (uint32_t)(pcm.size() * 2), riff = 36 + dataSize;
    uint16_t fmt = 1, ch = 2, bits = 16, ba = 4; uint32_t br = (uint32_t)sr * 4, fsz = 16;
    fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); fwrite(&fsz,4,1,f); fwrite(&fmt,2,1,f); fwrite(&ch,2,1,f);
    fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&dataSize,4,1,f); fwrite(pcm.data(),2,pcm.size(),f);
    fclose(f);
}

int main(int argc, char** argv) {
    _mm_setcsr(_mm_getcsr() | 0x8040);

    // ---- [shaper] -------------------------------------------------------
    std::printf("[shaper]\n");
    {
        for (int m = 0; m < 5; ++m) {
            int bad = 0;
            for (int i = 0; i < dreamer::shapers::kTableLen; ++i)
                if (dreamer::shapers::kTables[m][i] & 0xF) ++bad;
            CHECK(bad == 0, "shaper table 12-bit grain");
        }
        // grained inputs stay grained through the shaper; OFF/depth0 bit-dry
        uint32_t rng = 123u;
        int badGrain = 0, badDry = 0, badBound = 0;
        for (int i = 0; i < 20000; ++i) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int16_t raw = (int16_t)((int32_t)(rng & 0xFFF0u) - 32768) ;
            const float x = (float)((raw / 16) * 16) * (1.0f / 32768.0f);
            for (int m = 1; m <= 5; ++m) {
                const float y = dreamer::Waveshaper::process(x, m, 0.7f);
                const int32_t q = (int32_t)(y * 32768.0f);
                if (q & 0xF) ++badGrain;
                if (std::fabs(y) > 1.0f) ++badBound;
            }
            if (dreamer::Waveshaper::process(x, 0, 0.7f) != x) ++badDry;
            if (dreamer::Waveshaper::process(x, 3, 0.0f) != x) ++badDry;
        }
        CHECK(badGrain == 0, "shaper output keeps 12-bit grain");
        CHECK(badDry == 0, "shaper OFF/depth-0 is bit-exact dry");
        CHECK(badBound == 0, "shaper output bounded");
    }

    // ---- [svf] ----------------------------------------------------------
    std::printf("[svf]\n");
    {
        auto respond = [&](int mode, double sigHz, double cutHz) {
            dreamer::ToneSvf f;
            f.setSampleRate(SR);
            f.setMode(mode);
            f.setCutoffRes(cutHz, 0.0);
            double peak = 0.0;
            for (int i = 0; i < SR / 2; ++i) {
                const float x = (float)std::sin(2.0 * M_PI * sigHz * i / SR);
                const float y = f.process(x);
                if (i > SR / 4) peak = std::fmax(peak, std::fabs(y));
            }
            return peak;
        };
        CHECK(respond(0,   50, 1000) > 0.7, "LP24 passes low");
        CHECK(respond(0, 8000, 1000) < 0.1, "LP24 kills high");
        CHECK(respond(1,   50, 1000) > 0.7, "LP12 passes low");
        CHECK(respond(1, 8000, 1000) < 0.35, "LP12 attenuates high");
        CHECK(respond(3,   50, 1000) < 0.1, "HP kills low");
        CHECK(respond(3, 8000, 1000) > 0.6, "HP passes high");
        CHECK(respond(2,   50, 1000) < 0.2, "BP kills low");
        CHECK(respond(2, 8000, 1000) < 0.5, "BP attenuates high");
        CHECK(respond(2, 1000, 1000) > 0.5, "BP passes centre");
    }

    // ---- [chord] + alloc guard ------------------------------------------
    std::printf("[chord]\n");
    static dreamer::DreamSynth synth;
    {
        synth.prepare(SR);
        auto& p = synth.patch();
        p = sinePatch();
        p.tone[1] = p.tone[0];
        p.tone[1].waveIndex = findWave("Pad", "Hollow 2");
        p.tone[1].fineCents = 6.0;  p.tone[1].pan = -0.5;
        p.tone[2] = p.tone[0];
        p.tone[2].waveIndex = findWave("String", "StringBox 1");
        p.tone[2].coarse = -12;     p.tone[2].pan = 0.5;
        p.tone[3] = p.tone[0];
        p.tone[3].waveIndex = findWave("Bell", "FM Bell 1");
        p.tone[3].coarse = 12;      p.tone[3].level = 0.4;
        p.tone[3].shapeMode = 1;    p.tone[3].shapeDepth = 0.4;
        for (auto& t : p.tone) { t.level *= 0.5; t.tvaR = 0.05; }
        synth.updateLive();
        for (int i = 0; i < 24; ++i) synth.noteOn(36 + i * 2, 0.8f);

        std::vector<float> L, R;
        L.reserve(SR * 3); R.reserve(SR * 3);
        g_allocCount = 0; g_countAllocs = true;
        for (int i = 0; i < SR; ++i) {
            float l = 0, r = 0;
            synth.process(0.0f, 0.0f, l, r);
            L.push_back(l * 0.02f); R.push_back(r * 0.02f);
        }
        g_countAllocs = false;
        CHECK(g_allocCount == 0, "zero allocations during 24x4 process()");
        for (int i = 0; i < 24; ++i) synth.noteOff(36 + i * 2);
        for (int i = 0; i < SR * 2; ++i) {
            float l = 0, r = 0;
            synth.process(0.0f, 0.0f, l, r);
            L.push_back(l * 0.02f); R.push_back(r * 0.02f);
        }
        bool finite = true; float peak = 0, tail = 0;
        for (float s : L) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        for (size_t i = L.size() - 4800; i < L.size(); ++i) tail = std::fmax(tail, std::fabs(L[i]));
        std::printf("  chord peak=%.3f tail=%.6f allocs=%ld\n", peak, tail, g_allocCount);
        CHECK(finite, "chord finite");
        CHECK(peak > 0.01f && peak <= 1.5f, "chord peak sane");
        CHECK(tail < 0.005f, "chord tail silent");
    }

    // ---- [steal] --------------------------------------------------------
    std::printf("[steal]\n");
    {
        synth.prepare(SR);
        synth.killAll();
        auto& p = synth.patch();
        p = sinePatch();
        p.tone[0].tvaR = 0.005;
        synth.updateLive();
        auto renderMs = [&](int ms) {
            for (int i = 0; i < SR * ms / 1000; ++i) { float l = 0, r = 0; synth.process(0, 0, l, r); }
        };
        auto hasNote = [&](int note) {
            for (int i = 0; i < dreamer::DreamSynth::kMaxVoices; ++i)
                if (synth.voiceForTest(i).isActive()
                    && synth.voiceForTest(i).note() == note) return true;
            return false;
        };
        synth.noteOn(40, 0.8f); renderMs(20);
        synth.noteOff(40);      renderMs(200);
        CHECK(!hasNote(40), "note 40 released to idle");
        synth.noteOn(41, 0.8f); renderMs(5);
        for (int n = 42; n < 65; ++n) { synth.noteOn(n, 0.8f); renderMs(1); }
        synth.noteOn(99, 0.8f); renderMs(5);
        CHECK(!hasNote(41), "oldest note (41) stolen");
        CHECK(hasNote(42), "next-oldest (42) survived");
        CHECK(hasNote(99), "new note sounding");
    }

    // ---- [vector] -------------------------------------------------------
    std::printf("[vector]\n");
    {
        auto rms = [&](const std::vector<float>& v) {
            double s = 0; for (size_t i = SR / 5; i < v.size(); ++i) s += (double)v[i] * v[i];
            return std::sqrt(s / (double)(v.size() - SR / 5));
        };
        dreamer::DreamPatch p0 = sinePatch();     // int=0: static regardless of phase
        p0.vec.phase = 0.5;
        dreamer::DreamPatch p1 = sinePatch();
        p1.tone[0].vecInt = 1.0;                  // int=1, dir=0
        p1.vec.phase = 0.0;                       // aligned -> full
        dreamer::DreamPatch p2 = p1;
        p2.vec.phase = 0.25;                      // orthogonal -> silent
        dreamer::DreamPatch p3 = p1;
        p3.vec.phase = 0.5;                       // opposed -> silent
        const double r0 = rms(render1s(p0)), r1 = rms(render1s(p1)),
                     r2 = rms(render1s(p2)), r3 = rms(render1s(p3));
        std::printf("  rms int0=%.4f aligned=%.4f ortho=%.6f opposed=%.6f\n", r0, r1, r2, r3);
        CHECK(r0 > 0.1, "int=0 tone is a static drone");
        CHECK(r1 > 0.1, "aligned scanned tone at full gain");
        CHECK(std::fabs(r1 - r0) < 0.05 * r0, "aligned gain equals static level");
        CHECK(r2 < 0.001, "orthogonal scanned tone silent");
        CHECK(r3 < 0.001, "opposed scanned tone silent");
    }

    // ---- [aux] ----------------------------------------------------------
    std::printf("[aux]\n");
    {
        dreamer::DreamPatch p = sinePatch();
        p.tone[0].auxDest = 0;       // PITCH
        p.tone[0].auxAmt  = 0.5;     // +12 st at env peak (sustain 1)
        p.tone[0].auxA = 0.001; p.tone[0].auxS = 1.0;
        auto r = render1s(p);
        const int c = countCrossings(r, SR / 10);
        std::printf("  aux pitch crossings/0.9s: %d (expect ~792)\n", c);
        CHECK(c > 780 && c < 805, "aux env pitch +12 st");
    }

    // ---- [glfo] ---------------------------------------------------------
    std::printf("[glfo]\n");
    {
        dreamer::DreamPatch pa = sinePatch();
        dreamer::DreamPatch pb = sinePatch();
        pb.tone[0].lfoDepth = 0.0;   // depth 0, exotic settings
        pb.tone[0].lfoDest  = 0;
        pb.glfoShape01 = 4; pb.glfoRate01 = 95.0;
        auto ra = render1s(pa), rb = render1s(pb);
        CHECK(ra == rb, "depth-0 G-LFO tap is a bit-exact no-op");

        dreamer::DreamPatch pt = sinePatch();    // tremolo bounded
        pt.tone[0].lfoDepth = 1.0; pt.tone[0].lfoDest = 3;
        pt.glfoShape01 = 1;                       // SIN
        auto rt = render1s(pt);
        float peakA = 0, peakT = 0;
        for (size_t i = SR / 10; i < ra.size(); ++i) {
            peakA = std::fmax(peakA, std::fabs(ra[i]));
            peakT = std::fmax(peakT, std::fabs(rt[i]));
        }
        CHECK(peakT <= peakA * 1.0001f, "tremolo never exceeds dry");
        CHECK(peakT > 0.0f, "tremolo not silent");
    }

    // ---- [matrix] -------------------------------------------------------
    std::printf("[matrix]\n");
    {
        dreamer::DreamPatch p = sinePatch();
        p.slot[0] = { dreamer::mtx::srcWheel, dreamer::mtx::dstPitch, 1.0 };
        auto r = render1s(p, 69, 1.0f);          // wheel = 1 -> +12 st
        const int c = countCrossings(r, SR / 10);
        std::printf("  wheel->pitch crossings/0.9s: %d (expect ~792)\n", c);
        CHECK(c > 780 && c < 805, "matrix WHEEL->PITCH +12 st");

        // G-LFO -> CUT1 swings the exported octave offset within +/-2
        auto s = std::make_unique<dreamer::DreamSynth>();
        s->prepare(SR);
        s->patch() = sinePatch();
        s->patch().glfoShape01 = 1;              // SIN
        s->patch().glfoRate01  = 80.0;
        s->patch().slot[0] = { dreamer::mtx::srcGlfo, dreamer::mtx::dstCut1, 1.0 };
        s->updateLive();
        s->noteOn(69, 0.8f);
        float mn = 99, mx = -99;
        for (int i = 0; i < SR; ++i) {
            float l = 0, r2 = 0;
            s->process(0, 0, l, r2);
            mn = std::fmin(mn, s->matrixCut1Oct());
            mx = std::fmax(mx, s->matrixCut1Oct());
        }
        std::printf("  cut1 offset range: %.2f .. %.2f oct\n", mn, mx);
        CHECK(mn < -1.5f && mx > 1.5f, "G-LFO->CUT1 swings +/-2 oct");
        CHECK(mn >= -2.01f && mx <= 2.01f, "CUT1 offset bounded");

        // VEC PHS -> VEC PHS self-route clamped: render identical to no slot
        dreamer::DreamPatch q = sinePatch();
        q.tone[0].vecInt = 1.0; q.vec.phase = 0.1;
        dreamer::DreamPatch q2 = q;
        q2.slot[0] = { dreamer::mtx::srcVecPhs, dreamer::mtx::dstVecPhs, 1.0 };
        CHECK(render1s(q) == render1s(q2), "VEC PHS self-route clamped");
    }

    // ---- [pan] ----------------------------------------------------------
    std::printf("[pan]\n");
    {
        auto s = std::make_unique<dreamer::DreamSynth>();
        s->prepare(SR);
        s->patch() = sinePatch();
        s->patch().tone[0].pan = -1.0;
        s->updateLive();
        s->noteOn(69, 0.8f);
        float peakL = 0, peakR = 0;
        for (int i = 0; i < SR / 2; ++i) {
            float l = 0, r = 0;
            s->process(0, 0, l, r);
            if (i > SR / 10) { peakL = std::fmax(peakL, std::fabs(l)); peakR = std::fmax(peakR, std::fabs(r)); }
        }
        std::printf("  hard-left peaks L=%.3f R=%.5f\n", peakL, peakR);
        CHECK(peakL > 0.1f, "hard-left tone audible on L");
        CHECK(peakR < peakL * 0.01f, "hard-left tone silent on R");
    }

    // ---- [render] vector pad demo ---------------------------------------
    std::printf("[render]\n");
    {
        auto s = std::make_unique<dreamer::DreamSynth>();
        s->prepare(SR);
        auto& p = s->patch();
        p = dreamer::DreamPatch{};
        const struct { const char* cat; const char* name; double dir; int coarse; } tset[4] = {
            { "Pad",    "Hollow 3",     0.00,   0 },
            { "String", "StringBox 3",  0.25,   0 },
            { "Vox",    "Voice 1",      0.50,  12 },
            { "Bell",   "FM Bell 4",    0.75,  12 },
        };
        for (int i = 0; i < 4; ++i) {
            auto& t = p.tone[i];
            t.enabled = true;
            t.waveIndex = findWave(tset[i].cat, tset[i].name);
            t.coarse = tset[i].coarse;
            t.dir = tset[i].dir; t.vecInt = 0.85;
            t.level = 0.5; t.pan = (i % 2 ? 0.4 : -0.4) * (i > 1 ? -1.0 : 1.0);
            t.cutoffHz = 3000; t.tvfEnvAmount = 1500;
            t.tvfA = 0.6; t.tvfS = 0.7; t.tvfR = 0.8;
            t.tvaA = 0.4; t.tvaS = 0.9; t.tvaR = 1.0;
            t.shapeMode = (i == 3) ? 3 : 0; t.shapeDepth = 0.3;
            t.lfoDepth = 0.12; t.lfoDest = 0;
        }
        p.vec.orbitOn = true; p.vec.orbitRate01 = 18.0;
        p.glfoShape01 = 1; p.glfoRate01 = 42.0;
        s->updateLive();
        for (int n : {45, 52, 57, 61, 64}) s->noteOn(n, 0.7f);
        std::vector<float> L(SR * 6), R(SR * 6);
        for (int i = 0; i < SR * 6; ++i) {
            float l = 0, r = 0;
            s->process(0, 0, l, r);
            L[(size_t)i] = l * 0.22f; R[(size_t)i] = r * 0.22f;
        }
        bool finite = true; float peak = 0;
        for (size_t i = 0; i < L.size(); ++i) {
            if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false;
            peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
        }
        std::printf("  vector pad peak=%.3f\n", peak);
        CHECK(finite, "vector pad finite");
        CHECK(peak > 0.05f && peak <= 1.0f, "vector pad peak in range");
        if (argc > 1) {
            const std::string dir(argv[1]);
            writeWav((dir + "\\dream_vector_pad.wav").c_str(), L, R, SR);
            std::printf("  wav written to %s\n", dir.c_str());
        }
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
