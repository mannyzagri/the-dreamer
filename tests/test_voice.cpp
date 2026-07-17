// test_voice.cpp -- phase-4 gate: DreamVoice / DreamSynth behavior
//
// Build (after vcvars64.bat), from C:\the-dreamer:
//   cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES /I.
//     tests\test_voice.cpp /Fo:tests\bin\ /Fe:tests\bin\test_voice.exe
//   tests\bin\test_voice.exe [wav-output-dir]
//
// Checks:
//   [chord]   24-voice chord renders finite; tail silent after releases
//   [alloc]   zero heap allocations inside process() (counting new/delete)
//   [steal]   oldest-NOTE stealing (the DreamSynth stamp fix): a reused voice
//             holding the oldest note is stolen, not the least-reused voice
//   [lfo]     depth-0 renders are bit-identical regardless of LFO settings;
//             key-trigger renders are deterministic; square pitch LFO at
//             depth 1 shifts mean frequency per the d*d*12-semitone law;
//             tremolo bounds output without silencing it
//   [ctrl]    cutoff updates arrive exactly every 16 samples (spy FilterSlot)
//   [bend]    pitch bend +12 st doubles the rendered frequency
//   [render]  bell + string-pad demo patches through the Rhino filter (wavs
//             written when an output dir is given)

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

// ---- allocation guard (must precede the DSP includes is not required;
// global replacement operators apply program-wide) ------------------------
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

// open-filter single-partial sine patch for pitch measurements
static dreamer::DreamPatch sinePatch() {
    dreamer::DreamPatch p;
    p.a.base.waveIndex = findWave("Basic", "Sine");
    p.a.base.cutoffHz = 18000.0; p.a.base.resonance = 0.0;
    p.a.base.tvfEnvAmount = 0.0; p.a.base.tvfKeyFollow = 0.0;
    p.a.base.tvaA = 0.001; p.a.base.tvaS = 1.0; p.a.base.tvaR = 0.05;
    p.a.base.tvfA = 0.001; p.a.base.tvfS = 1.0;
    p.b.base.enabled = false;
    return p;
}

static int countCrossings(const std::vector<float>& s, size_t from = 0) {
    int c = 0;
    for (size_t i = from + 1; i < s.size(); ++i)
        if (s[i - 1] <= 0.0f && s[i] > 0.0f) ++c;
    return c;
}

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

static dreamer::DreamSynth synthA, synthB;   // static: keep test stack small

int main(int argc, char** argv) {
    _mm_setcsr(_mm_getcsr() | 0x8040);   // FTZ + DAZ

    // ---- [chord] + [alloc] ---------------------------------------------
    std::printf("[chord+alloc]\n");
    {
        synthA.prepare(SR);
        auto& p = synthA.patch();
        p = sinePatch();
        p.a.base.tvaR = 0.05; p.a.base.level = 0.5;
        p.b = p.a;
        p.b.base.enabled = true;
        p.b.base.fineCents = 6.0;
        p.b.base.waveIndex = findWave("Pad", "Hollow 2");
        synthA.updateLive();
        for (int i = 0; i < 24; ++i) synthA.noteOn(36 + i * 2, 0.8f);

        std::vector<float> out;
        out.reserve(SR * 4);              // allocate BEFORE the guard window
        g_allocCount = 0; g_countAllocs = true;
        for (int i = 0; i < SR; ++i) out.push_back(synthA.process(0.0f, 0.0f) * 0.03f);
        g_countAllocs = false;
        CHECK(g_allocCount == 0, "zero allocations during 24-voice process()");

        for (int i = 0; i < 24; ++i) synthA.noteOff(36 + i * 2);
        for (int i = 0; i < SR * 2; ++i) out.push_back(synthA.process(0.0f, 0.0f) * 0.03f);
        bool finite = true; float peak = 0, tail = 0;
        for (float s : out) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        for (size_t i = out.size() - 4800; i < out.size(); ++i) tail = std::fmax(tail, std::fabs(out[i]));
        std::printf("  chord peak=%.3f tail=%.6f allocs=%ld\n", peak, tail, g_allocCount);
        CHECK(finite, "24-voice chord finite");
        CHECK(peak > 0.01f && peak <= 1.5f, "24-voice chord peak sane");
        CHECK(tail < 0.005f, "24-voice chord tail silent");
    }

    // ---- [steal] --------------------------------------------------------
    std::printf("[steal]\n");
    {
        synthA.prepare(SR);
        auto& p = synthA.patch();
        p = sinePatch();
        p.a.base.tvaR = 0.005;            // fast release so a voice can go idle
        synthA.updateLive();

        auto renderMs = [&](int ms) {
            for (int i = 0; i < SR * ms / 1000; ++i) (void)synthA.process(0.0f, 0.0f);
        };
        auto hasNote = [&](int note) {
            for (int i = 0; i < dreamer::DreamSynth::kMaxVoices; ++i)
                if (synthA.voiceForTest(i).isActive()
                    && synthA.voiceForTest(i).note() == note) return true;
            return false;
        };

        synthA.noteOn(40, 0.8f); renderMs(20);          // stamp 1 -> some voice V
        synthA.noteOff(40);      renderMs(200);         // V idle
        CHECK(!hasNote(40), "note 40 released to idle");
        synthA.noteOn(41, 0.8f); renderMs(5);           // stamp 2, reuses a free voice
        for (int n = 42; n < 65; ++n) { synthA.noteOn(n, 0.8f); renderMs(1); }  // stamps 3..25
        // all 24 voices active now; stamp 2 (note 41) is the oldest note
        synthA.noteOn(99, 0.8f); renderMs(5);           // stamp 26 -> must steal note 41
        CHECK(!hasNote(41), "oldest note (41) was stolen");
        CHECK(hasNote(42), "next-oldest note (42) survived");
        CHECK(hasNote(99), "new note (99) is sounding");
        // (the verified bank's per-voice serial would have stolen note 42's
        //  never-reused voice instead -- this asserts the DreamSynth fix)
    }

    // ---- [lfo] ----------------------------------------------------------
    std::printf("[lfo]\n");
    {
        // depth 0 => LFO settings must not affect the render (bit-identity).
        // Hermetic: every render uses a FRESH synth (heap; kill() silences but
        // deliberately leaves envelope levels frozen, like a stolen voice).
        auto render1s = [&](dreamer::DreamSynth& /*unused*/, const dreamer::DreamPatch& patch) {
            auto s = std::make_unique<dreamer::DreamSynth>();
            s->prepare(SR);
            s->patch() = patch;
            s->updateLive();
            s->noteOn(69, 0.8f);
            std::vector<float> out(SR);
            for (int i = 0; i < SR; ++i) out[i] = s->process(0.0f, 0.0f);
            return out;
        };
        dreamer::DreamPatch p0 = sinePatch();
        dreamer::DreamPatch p1 = p0;
        p1.a.lfo[0] = { 3, 90.0f, 0.0f, 0, true };   // square, fast, depth 0
        p1.a.lfo[1] = { 4, 10.0f, 0.0f, 2, false };  // s&h, slow, depth 0
        auto r0 = render1s(synthA, p0), r1 = render1s(synthB, p1);
        CHECK(r0 == r1, "depth-0 LFOs are a bit-exact no-op");

        // key-trigger determinism: same patch twice -> identical
        dreamer::DreamPatch pk = sinePatch();
        pk.a.lfo[0] = { 0, 60.0f, 0.5f, 0, true };
        auto k0 = render1s(synthA, pk), k1 = render1s(synthB, pk);
        CHECK(k0 == k1, "key-trigged LFO render deterministic");

        // square pitch LFO depth 1: halves at +/-12 st -> mean freq ~ (880+220)/2
        dreamer::DreamPatch ps = sinePatch();
        ps.a.lfo[0] = { 3, 100.0f, 1.0f, 0, true };  // square, 30 Hz, depth 1
        auto rs = render1s(synthA, ps);
        const int c = countCrossings(rs, SR / 10);   // skip attack
        std::printf("  square pitch LFO crossings/0.9s: %d (expect ~495)\n", c);
        CHECK(c > 445 && c < 545, "square pitch LFO mean freq per d*d*12 law");

        // tremolo: bounded, not silent, quieter than dry
        dreamer::DreamPatch pt = sinePatch();
        pt.a.lfo[0] = { 0, 80.0f, 1.0f, 2, true };
        auto rt = render1s(synthB, pt);
        float peakDry = 0, peakTrem = 0, rmsTrem = 0;
        for (int i = SR / 10; i < SR; ++i) {
            peakDry  = std::fmax(peakDry,  std::fabs(r0[i]));
            peakTrem = std::fmax(peakTrem, std::fabs(rt[i]));
            rmsTrem += rt[i] * rt[i];
        }
        CHECK(peakTrem <= peakDry * 1.0001f, "tremolo never exceeds dry level");
        CHECK(rmsTrem > 0.0f, "tremolo does not silence the voice");
    }

    // ---- [ctrl] ----------------------------------------------------------
    std::printf("[ctrl]\n");
    {
        struct SpySlot final : rompler::FilterSlot {
            int calls = 0;
            void setSampleRate(double) override {}
            void setCutoffRes(double, double) override { ++calls; }
            float process(float in) override { return in; }
            void reset() override {}
        };
        static SpySlot spyA, spyB;
        synthA.prepare(SR);
        synthA.killAll();
        synthA.patch() = sinePatch();
        synthA.updateLive();
        synthA.voiceForTest(0).setFilters(&spyA, &spyB);
        synthA.noteOn(69, 0.8f);                    // goes to voice 0 (all idle)
        spyA.calls = 0;
        for (int i = 0; i < 1600; ++i) (void)synthA.process(0.0f, 0.0f);
        std::printf("  cutoff updates over 1600 samples: %d (expect 100)\n", spyA.calls);
        CHECK(spyA.calls == 100, "control-rate law: exactly every 16 samples");
        synthA.voiceForTest(0).setFilters(nullptr, nullptr);   // restore Rhino slots
        synthA.killAll();
    }

    // ---- [bend] ----------------------------------------------------------
    std::printf("[bend]\n");
    {
        synthA.prepare(SR);
        synthA.killAll();
        synthA.setPitchBend(0.0f);
        synthA.patch() = sinePatch();
        synthA.updateLive();
        synthA.noteOn(69, 0.8f);
        std::vector<float> flat(SR), bent(SR);
        for (int i = 0; i < SR; ++i) flat[i] = synthA.process(0.0f, 0.0f);
        synthA.setPitchBend(12.0f);
        for (int i = 0; i < SR; ++i) bent[i] = synthA.process(0.0f, 0.0f);
        const int cf = countCrossings(flat, SR / 10);
        const int cb = countCrossings(bent, 0);
        std::printf("  crossings flat=%d bent(+12)=%d\n", cf, cb);
        CHECK(cf > 391 && cf < 401, "flat pitch ~440 over 0.9 s");
        CHECK(cb > 872 && cb < 888, "bend +12 st doubles frequency");
    }

    // ---- [render] demo patches through the Rhino filter ------------------
    std::printf("[render]\n");
    std::vector<float> bell, pad;
    {
        synthA.prepare(SR);
        synthA.killAll();
        synthA.setPitchBend(0.0f);
        auto& p = synthA.patch();
        p = dreamer::DreamPatch{};
        p.a.base.waveIndex = findWave("Bell", "FM Bell 4");
        p.a.base.level = 0.8; p.a.base.cutoffHz = 9000; p.a.base.tvfEnvAmount = 3000;
        p.a.base.tvaA = 0.002; p.a.base.tvaD = 0.9; p.a.base.tvaS = 0.0; p.a.base.tvaR = 0.4;
        p.a.base.tvfA = 0.001; p.a.base.tvfD = 0.5; p.a.base.tvfS = 0.2; p.a.base.tvfR = 0.4;
        p.b.base.waveIndex = findWave("Bell", "EP Tine 1");
        p.b.base.level = 0.7; p.b.base.fineCents = 4.0; p.b.base.startOffset = 150;
        p.b.base.cutoffHz = 4000;
        p.b.base.tvaA = 0.003; p.b.base.tvaD = 2.2; p.b.base.tvaS = 0.0; p.b.base.tvaR = 0.6;
        synthA.updateLive();
        for (int n : {72, 76, 79, 84}) {
            synthA.noteOn(n, 0.9f);
            for (int i = 0; i < SR * 3 / 20; ++i) bell.push_back(synthA.process(0, 0) * 0.5f);
            synthA.noteOff(n);
            for (int i = 0; i < SR * 11 / 20; ++i) bell.push_back(synthA.process(0, 0) * 0.5f);
        }
        synthA.noteOn(60, 0.8f); synthA.noteOn(64, 0.8f); synthA.noteOn(67, 0.8f);
        for (int i = 0; i < SR; ++i) bell.push_back(synthA.process(0, 0) * 0.26f);
        for (int n : {60, 64, 67}) synthA.noteOff(n);
        for (int i = 0; i < SR * 3; ++i) bell.push_back(synthA.process(0, 0) * 0.26f);
        bool finite = true; float peak = 0;
        for (float s : bell) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        std::printf("  bell (Rhino filter) peak=%.3f\n", peak);
        CHECK(finite, "bell render finite");
        CHECK(peak > 0.05f && peak <= 1.0f, "bell peak in range");
    }
    {
        synthB.prepare(SR);
        synthB.killAll();
        auto& q = synthB.patch();
        q = dreamer::DreamPatch{};
        q.a.base.waveIndex = findWave("String", "StringBox 3");
        q.a.base.fineCents = -7; q.a.base.cutoffHz = 2500;
        q.a.base.tvfEnvAmount = 1500; q.a.base.resonance = 0.1;
        q.a.base.tvaA = 0.5; q.a.base.tvaD = 0.1; q.a.base.tvaS = 0.9; q.a.base.tvaR = 1.2;
        q.a.base.tvfA = 0.8; q.a.base.tvfD = 0.5; q.a.base.tvfS = 0.6; q.a.base.tvfR = 1.0;
        q.a.lfo[0] = { 0, 45.0f, 0.15f, 0, false };   // gentle free-run vibrato
        q.b = q.a;
        q.b.base.waveIndex = findWave("String", "Cello 2");
        q.b.base.fineCents = +7; q.b.base.startOffset = 300; q.b.base.level = 0.8;
        q.b.filterType = 1;                            // liquid on partial B
        synthB.updateLive();
        for (int n : {57, 60, 64, 69}) synthB.noteOn(n, 0.7f);
        for (int i = 0; i < SR * 3; ++i) pad.push_back(synthB.process(0, 0) * 0.19f);
        for (int n : {57, 60, 64, 69}) synthB.noteOff(n);
        for (int i = 0; i < SR * 12; ++i) pad.push_back(synthB.process(0, 0) * 0.19f);
        bool finite = true; float peak = 0, tail = 0;
        for (float s : pad) { if (!std::isfinite(s)) finite = false; peak = std::fmax(peak, std::fabs(s)); }
        for (size_t i = pad.size() - 4800; i < pad.size(); ++i) tail = std::fmax(tail, std::fabs(pad[i]));
        std::printf("  pad (Rhino filter + vibrato) peak=%.3f tail=%.6f\n", peak, tail);
        CHECK(finite, "pad render finite");
        CHECK(peak > 0.05f && peak <= 1.0f, "pad peak in range");
        CHECK(tail < 0.005f, "pad tail silent");
    }

    if (argc > 1) {
        const std::string dir(argv[1]);
        writeWav((dir + "\\dream_bell_rhino.wav").c_str(), bell, SR);
        writeWav((dir + "\\dream_stringpad_rhino.wav").c_str(), pad, SR);
        std::printf("wavs written to %s\n", dir.c_str());
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
