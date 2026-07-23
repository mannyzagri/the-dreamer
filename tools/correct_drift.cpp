// correct_drift.cpp -- TD-002 correction twin of tools/measure_drift.cpp.
//
// Removes the baked mid-loop PITCH DRIFT (bake_final.py's shared_drift: a
// Brownian bridge in log-frequency pinned to ~0 at the loop ends, so the
// instantaneous pitch peaks mid-loop by +/-8..11 cents) from the DELIVERED v3
// library WAVs by time-varying resampling, flattening each TONAL loop to a
// CONSTANT 220 Hz (absolute -- so layered loops don't beat). Offline,
// deterministic house tool (no python).
//
// Per loop:
//   1. Read WAV (16-bit mono 44.1k, ~110250 samples).
//   2. INHARMONIC EXEMPTION (METAL_* + MORPH_VOXMETAL/METALAIR/ETHMETAL):
//      copy byte-identical -- these have no measurable pitch.
//   3. Track instantaneous pitch on a fine grid (hop H, ~0.13 s Hann window,
//      loop-WRAP at the window edges since the buffer loops) with the VALIDATED
//      normalized-autocorrelation core reused from measure_drift.cpp.
//   4. d[k]=log2(f[k]/220) octaves; robustly SMOOTH (median-of-3 then a
//      zero-phase circular moving average -- the true drift is slow & periodic
//      across the seam, so per-frame autocorr noise is rejected); m = mean of
//      the smoothed curve; residual r[k]=d[k]-m (deviation from the loop's own
//      mean, what we flatten).
//   5/6/7. Build a per-INPUT-sample residual r(n); time-warp so output pitch is
//      constant. To make the sharp region (r>0) read slower, spend 2^(r(n))
//      output samples per input sample (equiv. read-rate 2^(-r)); accumulate in
//      the INPUT domain and rescale to EXACTLY N output samples (this fixes the
//      length -> loopStart/length invariant preserved; endpoints unwarped ->
//      seam continuity preserved; normalization also removes any residual
//      constant offset -- harmless). Resample with a 4-point cubic (Catmull-Rom)
//      indexed with WRAP so o near N reads across the seam cleanly.
//   8. Requant every TONAL output sample to the 12-bit grid (bake_loops_header
//      idiom: q=lround(s/16); clamp[-2048,2047]; out=q*16 -> low nibble zero).
//   9. Write 16-bit mono 44.1k PCM, same length N.
//
// NOTE (verified on the delivered WAVs): assets/loops/*.wav are FULL 16-bit PCM,
// NOT pre-quantized to the 12-bit grid (~0..0.5% of samples have a zero low
// nibble; on-grid would be 100%). So the two constraints "inharmonic loops
// byte-identical to input" and "every output sample & 0xF == 0" cannot both hold
// for inharmonic loops. Faithful to the spec's algorithm, the grain requant is
// step 8 of the TONAL branch only; inharmonic loops stay byte-identical to their
// 16-bit input (the re-bake requants them like every other loop, so the bank is
// still on-grid). Grain is asserted on tonal outputs; byte-identity is asserted
// on inharmonic outputs.
//
// Build (vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES tools\correct_drift.cpp
//     /Fo:tools\bin\ /Fe:tools\bin\correct_drift.exe
//   tools\bin\correct_drift.exe [inDir] [outDir]
//     defaults: inDir=assets\loops  outDir=build\loops-v4

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <string>
#include <vector>
#include <filesystem>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Loop order = assets/library-src/manifest.json (LOCKED). 130 names. Copied
// verbatim from tools/bake_loops_header.cpp.
// ---------------------------------------------------------------------------
static const char* kLoopNames[] = {
    // PAD (28)
    "PAD_01", "PAD_02", "PAD_03", "PAD_04", "PAD_05", "PAD_06", "PAD_07",
    "PAD_08", "PAD_09", "PAD_10", "PAD_11", "PAD_12", "PAD_13", "PAD_14",
    "PAD_15", "PAD_16", "PAD_17", "PAD_18", "PAD_19", "PAD_20", "PAD_21",
    "PAD_22", "PAD_23", "PAD_24", "PAD_25", "PAD_26", "PAD_27", "PAD_28",
    // AIRY (18)
    "AIRY_01", "AIRY_02", "AIRY_03", "AIRY_04", "AIRY_05", "AIRY_06",
    "AIRY_07", "AIRY_08", "AIRY_09", "AIRY_10", "AIRY_11", "AIRY_12",
    "AIRY_13", "AIRY_14", "AIRY_15", "AIRY_16", "AIRY_17", "AIRY_18",
    // VOX (16)
    "VOX_01", "VOX_02", "VOX_03", "VOX_04", "VOX_05", "VOX_06", "VOX_07",
    "VOX_08", "VOX_09", "VOX_10", "VOX_11", "VOX_12", "VOX_13", "VOX_14",
    "VOX_15", "VOX_16",
    // ETHER (16)
    "ETHER_01", "ETHER_02", "ETHER_03", "ETHER_04", "ETHER_05", "ETHER_06",
    "ETHER_07", "ETHER_08", "ETHER_09", "ETHER_10", "ETHER_11", "ETHER_12",
    "ETHER_13", "ETHER_14", "ETHER_15", "ETHER_16",
    // FM (14)
    "FM_01", "FM_02", "FM_03", "FM_04", "FM_05", "FM_06", "FM_07", "FM_08",
    "FM_09", "FM_10", "FM_11", "FM_12", "FM_13", "FM_14",
    // WIND (12)
    "WIND_01", "WIND_02", "WIND_03", "WIND_04", "WIND_05", "WIND_06",
    "WIND_07", "WIND_08", "WIND_09", "WIND_10", "WIND_11", "WIND_12",
    // METAL (12)
    "METAL_01", "METAL_02", "METAL_03", "METAL_04", "METAL_05", "METAL_06",
    "METAL_07", "METAL_08", "METAL_09", "METAL_10", "METAL_11", "METAL_12",
    // MORPH (14)
    "MORPH_PADAIR", "MORPH_VOXETHER", "MORPH_STRWIND", "MORPH_ETHERFM",
    "MORPH_VOXMETAL", "MORPH_AIRGLASS", "MORPH_FMBELL", "MORPH_PADVOX",
    "MORPH_WINDVOX", "MORPH_METALAIR", "MORPH_PADPAD", "MORPH_VOXVOX",
    "MORPH_ETHMETAL", "MORPH_FMPAD",
};
static const int kNumLoops = (int)(sizeof(kLoopNames) / sizeof(kLoopNames[0]));

// Per-family category from filename prefix (copied from bake_loops_header.cpp).
static const char* categoryFor(const std::string& n) {
    if (n.rfind("PAD_",   0) == 0) return "Pad";
    if (n.rfind("AIRY_",  0) == 0) return "Airy";
    if (n.rfind("VOX_",   0) == 0) return "Vox";
    if (n.rfind("ETHER_", 0) == 0) return "Ether";
    if (n.rfind("FM_",    0) == 0) return "FM";
    if (n.rfind("WIND_",  0) == 0) return "Wind";
    if (n.rfind("METAL_", 0) == 0) return "Metal";
    if (n.rfind("MORPH_", 0) == 0) return "Morph";
    return "Loop";
}

// METAL_* + the three metal MORPHs have no measurable pitch -> passed through.
static bool isInharmonic(const std::string& n) {
    return n.rfind("METAL_", 0) == 0
        || n == "MORPH_VOXMETAL" || n == "MORPH_METALAIR" || n == "MORPH_ETHMETAL";
}

// ---------------------------------------------------------------------------
// RIFF reader -- identical to measure_drift.cpp / bake_loops_header.cpp.
// ---------------------------------------------------------------------------
static bool readWav(const std::string& path, std::vector<int16_t>& out, uint32_t& sr) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char id[4]; uint32_t sz;
    if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4)) { std::fclose(f); return false; }
    std::fread(&sz, 4, 1, f); std::fread(id, 1, 4, f);
    if (std::memcmp(id, "WAVE", 4)) { std::fclose(f); return false; }
    uint16_t fmt = 0, ch = 0, bits = 0; sr = 0;
    while (std::fread(id, 1, 4, f) == 4 && std::fread(&sz, 4, 1, f) == 1) {
        if (!std::memcmp(id, "fmt ", 4)) {
            uint16_t ba; uint32_t br;
            std::fread(&fmt, 2, 1, f); std::fread(&ch, 2, 1, f);
            std::fread(&sr, 4, 1, f);  std::fread(&br, 4, 1, f);
            std::fread(&ba, 2, 1, f);  std::fread(&bits, 2, 1, f);
            if (sz > 16) std::fseek(f, (long)sz - 16, SEEK_CUR);
        } else if (!std::memcmp(id, "data", 4)) {
            out.resize(sz / 2); std::fread(out.data(), 2, out.size(), f);
            std::fclose(f); return fmt == 1 && ch == 1 && bits == 16;
        } else std::fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
    }
    std::fclose(f); return false;
}

// Canonical 44-byte-header 16-bit mono PCM writer.
static bool writeWav(const std::string& path, const std::vector<int16_t>& s, uint32_t sr) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t dataBytes = (uint32_t)(s.size() * 2);
    const uint32_t riffSize  = 36 + dataBytes;
    const uint16_t fmt = 1, ch = 1, bits = 16, blockAlign = 2;
    const uint32_t fmtSize = 16, byteRate = sr * blockAlign;
    bool ok = true;
    ok &= std::fwrite("RIFF", 1, 4, f) == 4;
    ok &= std::fwrite(&riffSize, 4, 1, f) == 1;
    ok &= std::fwrite("WAVE", 1, 4, f) == 4;
    ok &= std::fwrite("fmt ", 1, 4, f) == 4;
    ok &= std::fwrite(&fmtSize, 4, 1, f) == 1;
    ok &= std::fwrite(&fmt, 2, 1, f) == 1;
    ok &= std::fwrite(&ch, 2, 1, f) == 1;
    ok &= std::fwrite(&sr, 4, 1, f) == 1;
    ok &= std::fwrite(&byteRate, 4, 1, f) == 1;
    ok &= std::fwrite(&blockAlign, 2, 1, f) == 1;
    ok &= std::fwrite(&bits, 2, 1, f) == 1;
    ok &= std::fwrite("data", 1, 4, f) == 4;
    ok &= std::fwrite(&dataBytes, 4, 1, f) == 1;
    ok &= std::fwrite(s.data(), 2, s.size(), f) == s.size();
    std::fclose(f);
    return ok;
}

// ---------------------------------------------------------------------------
// Pitch core (measurement). SR / windows / lag range identical to
// measure_drift.cpp so the acceptance numbers this tool prints match the
// independent measure_drift.exe the parent gates on.
// ---------------------------------------------------------------------------
static const int    SR   = 44100;
static const double kRef = 220.0;
static const int    kWin = (int)(0.3 * SR);        // 13230-sample acceptance window
static const int    kMinLag = 150, kMaxLag = 267;  // 165..294 Hz search

static inline long wrapIdx(long i, long n) { i %= n; if (i < 0) i += n; return i; }

// Verbatim measure_drift.cpp pitchAt: clamped (non-wrapped) Hann-windowed NAC,
// used ONLY for the 5-position acceptance measurement (pre/post).
static double pitchAt(const std::vector<int16_t>& s, int start) {
    if (start < 0) start = 0;
    if (start + kWin > (int)s.size()) start = (int)s.size() - kWin;
    if (start < 0) return 0.0;
    std::vector<double> w(kWin);
    double mean = 0.0;
    for (int n = 0; n < kWin; ++n) mean += s[(size_t)(start + n)];
    mean /= kWin;
    for (int n = 0; n < kWin; ++n) {
        const double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * n / (kWin - 1));
        w[(size_t)n] = ((double)s[(size_t)(start + n)] - mean) * h;
    }
    double bestNac = -1e9; int bestLag = 0;
    double e0 = 0.0; for (int n = 0; n < kWin; ++n) e0 += w[(size_t)n] * w[(size_t)n];
    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        double acc = 0.0, el = 0.0;
        for (int n = 0; n + lag < kWin; ++n) {
            acc += w[(size_t)n] * w[(size_t)(n + lag)];
            el  += w[(size_t)(n + lag)] * w[(size_t)(n + lag)];
        }
        const double nac = acc / (std::sqrt(e0 * el) + 1e-12);
        if (nac > bestNac) { bestNac = nac; bestLag = lag; }
    }
    double lag = bestLag;
    if (bestLag > kMinLag && bestLag < kMaxLag) {
        auto nacAt = [&](int L) {
            double acc = 0.0, el = 0.0;
            for (int n = 0; n + L < kWin; ++n) { acc += w[(size_t)n]*w[(size_t)(n+L)]; el += w[(size_t)(n+L)]*w[(size_t)(n+L)]; }
            return acc / (std::sqrt(e0 * el) + 1e-12);
        };
        const double ym1 = nacAt(bestLag - 1), y0 = bestNac, yp1 = nacAt(bestLag + 1);
        const double denom = (ym1 - 2*y0 + yp1);
        if (std::fabs(denom) > 1e-12) lag = bestLag + 0.5 * (ym1 - yp1) / denom;
    }
    return lag > 0 ? (double)SR / lag : 0.0;
}

static double cents(double f, double ref) { return f > 0 ? 1200.0 * std::log2(f / ref) : 0.0; }

// Same NAC pitch core, but WRAP-indexed and with a caller-chosen (shorter)
// window -- used for the fine mid-loop tracking grid. Loop-wrap is exact here:
// the buffer is a seamless loop, so a window straddling the seam is legitimate
// signal (not zero-pad), which keeps tracking stable right to the ends.
static double pitchAtWrap(const std::vector<int16_t>& s, long center, int win) {
    const long N = (long)s.size();
    long start = center - win / 2;
    std::vector<double> w((size_t)win);
    double mean = 0.0;
    for (int n = 0; n < win; ++n) mean += s[(size_t)wrapIdx(start + n, N)];
    mean /= win;
    for (int n = 0; n < win; ++n) {
        const double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * n / (win - 1));
        w[(size_t)n] = ((double)s[(size_t)wrapIdx(start + n, N)] - mean) * h;
    }
    double e0 = 0.0; for (int n = 0; n < win; ++n) e0 += w[(size_t)n] * w[(size_t)n];
    double bestNac = -1e9; int bestLag = 0;
    for (int lag = kMinLag; lag <= kMaxLag; ++lag) {
        double acc = 0.0, el = 0.0;
        for (int n = 0; n + lag < win; ++n) {
            acc += w[(size_t)n] * w[(size_t)(n + lag)];
            el  += w[(size_t)(n + lag)] * w[(size_t)(n + lag)];
        }
        const double nac = acc / (std::sqrt(e0 * el) + 1e-12);
        if (nac > bestNac) { bestNac = nac; bestLag = lag; }
    }
    double lag = bestLag;
    if (bestLag > kMinLag && bestLag < kMaxLag) {
        auto nacAt = [&](int L) {
            double acc = 0.0, el = 0.0;
            for (int n = 0; n + L < win; ++n) { acc += w[(size_t)n]*w[(size_t)(n+L)]; el += w[(size_t)(n+L)]*w[(size_t)(n+L)]; }
            return acc / (std::sqrt(e0 * el) + 1e-12);
        };
        const double ym1 = nacAt(bestLag - 1), y0 = bestNac, yp1 = nacAt(bestLag + 1);
        const double denom = (ym1 - 2*y0 + yp1);
        if (std::fabs(denom) > 1e-12) lag = bestLag + 0.5 * (ym1 - yp1) / denom;
    }
    return lag > 0 ? (double)SR / lag : 0.0;
}

// 5-position (0/25/45/65/85%, 0.3 s) max deviation from the loop's own mean, in
// cents -- the v4 acceptance metric, computed exactly as measure_drift.cpp.
static double maxDevCents(const std::vector<int16_t>& s) {
    const double pos[5] = { 0.0, 0.25, 0.45, 0.65, 0.85 };
    double c[5], mean = 0.0;
    for (int p = 0; p < 5; ++p) {
        const int start = (int)(pos[p] * ((int)s.size() - kWin));
        c[p] = cents(pitchAt(s, start), kRef);
        mean += c[p];
    }
    mean /= 5.0;
    double dev = 0.0;
    for (int p = 0; p < 5; ++p) dev = std::fmax(dev, std::fabs(c[p] - mean));
    return dev;
}

static inline double median3(double a, double b, double c) {
    return std::fmax(std::fmin(a, b), std::fmin(std::fmax(a, b), c));
}

// 4-point cubic (Catmull-Rom) interpolation; p1..p2 bracket the fraction t.
static inline double catmull(double p0, double p1, double p2, double p3, double t) {
    return 0.5 * ((2.0 * p1)
        + (-p0 + p2) * t
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
}

// ---------------------------------------------------------------------------
// Correction pipeline (tonal loops). in -> flattened, 12-bit-grid out (len N).
// ---------------------------------------------------------------------------
static std::vector<int16_t> correct(const std::vector<int16_t>& in) {
    const long N = (long)in.size();
    const int  H = 256;                       // fine-grid hop
    const int  winF = (int)(0.13 * SR);       // 5733-sample tracking window
    const int  K = (int)(N / H);              // number of frames (>=1)

    // 1. Fine pitch -> log-octave deviation d[k] at frame centers k*H (wrap).
    std::vector<double> d((size_t)K);
    for (int k = 0; k < K; ++k) {
        const double f = pitchAtWrap(in, (long)k * H, winF);
        d[(size_t)k] = (f > 0.0) ? std::log2(f / kRef) : 0.0;   // octaves vs 220
    }

    // 2. Median-of-3 (circular) to kill single-frame autocorr outliers.
    std::vector<double> dm((size_t)K);
    for (int k = 0; k < K; ++k)
        dm[(size_t)k] = median3(d[(size_t)((k - 1 + K) % K)], d[(size_t)k],
                                d[(size_t)((k + 1) % K)]);

    // 3. Zero-phase circular moving average (radius R). The true drift is very
    //    slow and periodic across the seam, so a wide centered average recovers
    //    the smooth curve while rejecting frame-rate noise.
    const int R = 8;                          // 17-tap ~= 0.10 s smoothing
    std::vector<double> ds((size_t)K);
    for (int k = 0; k < K; ++k) {
        double acc = 0.0;
        for (int j = -R; j <= R; ++j) acc += dm[(size_t)(((k + j) % K + K) % K)];
        ds[(size_t)k] = acc / (2 * R + 1);
    }

    // 4. Residual r[k] = deviation from ABSOLUTE 220 Hz (log-octave). The drift
    //    is a Brownian bridge pinned to ~220 at the loop ends, so flattening to
    //    220 (not own-mean) keeps the seam AND lands every loop dead on 220 --
    //    so layering 2+ loops no longer beats (the reported "baked together"
    //    detune). Length floats +/-~0.5% to absorb the mean offset.
    std::vector<double> r((size_t)K);
    for (int k = 0; k < K; ++k) r[(size_t)k] = ds[(size_t)k];

    // 5. Per-INPUT-sample residual r(n): linear interp of the frame curve.
    //    Frames sit at k*H for k in [0,K-1]; the segment from the last frame
    //    back to frame 0 spans the seam (positions [(K-1)*H, N) -> r[0]).
    const long base = (long)(K - 1) * H;
    auto rAt = [&](long n) -> double {
        if (n < base) {
            const long k = n / H;
            const double frac = ((double)n - (double)k * H) / (double)H;
            return r[(size_t)k] + (r[(size_t)(k + 1)] - r[(size_t)k]) * frac;
        }
        const double segLen = (double)(N - base);
        const double frac = ((double)(n - base)) / segLen;
        return r[(size_t)(K - 1)] + (r[0] - r[(size_t)(K - 1)]) * frac;
    };

    // 6. Time-warp in the INPUT domain: spend 2^(r(n)) output samples per input
    //    sample (read the sharp region, r>0, slower -> pitch drops to 220). C[]
    //    is cumulative output-position vs input. Output length M = round(T) is
    //    the NATURAL length that centers the mean pitch on 220 (scale = 1); the
    //    ends are unwarped (r~0 -> seam continuity), loopStart stays 0.
    std::vector<double> C((size_t)N + 1);
    C[0] = 0.0;
    for (long n = 0; n < N; ++n) C[(size_t)(n + 1)] = C[(size_t)n] + std::pow(2.0, rAt(n));
    const double T = C[(size_t)N];
    const long M = (long)std::llround(T);      // natural length -> pitch on 220

    // Invert C to get the input read position P[o] for each output sample o.
    // C is strictly increasing (2^x>0), so a single monotonic march suffices.
    std::vector<int16_t> out((size_t)M);
    long n = 0;
    for (long o = 0; o < M; ++o) {
        const double target = (double)o;                   // C-domain: C^-1(o)
        while (n < N - 1 && C[(size_t)(n + 1)] <= target) ++n;
        const double span = C[(size_t)(n + 1)] - C[(size_t)n];
        const double frac = span > 0.0 ? (target - C[(size_t)n]) / span : 0.0;
        const double P = (double)n + frac;                 // input position

        // 7. Cubic resample at P with wrap (loop-continuous across the seam).
        const long i = (long)std::floor(P);
        const double t = P - (double)i;
        const double p0 = (double)in[(size_t)wrapIdx(i - 1, N)];
        const double p1 = (double)in[(size_t)wrapIdx(i,     N)];
        const double p2 = (double)in[(size_t)wrapIdx(i + 1, N)];
        const double p3 = (double)in[(size_t)wrapIdx(i + 2, N)];
        const double v  = catmull(p0, p1, p2, p3, t);

        // 8. Requant to the 12-bit grid (bake_loops_header idiom).
        int q = (int)std::lround(v / 16.0);
        if (q > 2047) q = 2047;
        if (q < -2048) q = -2048;
        out[(size_t)o] = (int16_t)(q * 16);
        assert((out[(size_t)o] & 0xF) == 0);
    }
    return out;
}

int main(int argc, char** argv) {
    const std::string inDir  = argc > 1 ? argv[1] : "assets\\loops";
    const std::string outDir = argc > 2 ? argv[2] : "build\\loops-v4";

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (!std::filesystem::is_directory(outDir)) {   // tolerate "already exists" ec on MSVC
        std::printf("cannot create outDir %s: %s\n", outDir.c_str(), ec.message().c_str());
        return 1;
    }

    std::printf("TD-002 drift correction  in=%s  out=%s\n", inDir.c_str(), outDir.c_str());
    std::printf("(pre/post = 5-pos max-dev-from-mean cents, 0.3s, same as measure_drift)\n\n");
    std::printf("%-16s %-6s %-11s %8s %8s\n", "loop", "cat", "type", "pre", "post");

    double worst = 0.0; std::string worstName;
    int nFail = 0, nTonal = 0, nInharm = 0;

    for (int i = 0; i < kNumLoops; ++i) {
        const std::string nm = kLoopNames[i];
        const std::string cat = categoryFor(nm);
        std::vector<int16_t> in; uint32_t sr = 0;
        if (!readWav(inDir + "\\" + nm + ".wav", in, sr) || sr != 44100 || in.empty()) {
            std::printf("%-16s %-6s  READ FAIL\n", nm.c_str(), cat.c_str());
            ++nFail; continue;
        }

        std::vector<int16_t> out;
        if (isInharmonic(nm)) {
            out = in;                                   // byte-identical passthrough
            assert(out.size() == in.size());
            assert(std::memcmp(out.data(), in.data(), in.size() * 2) == 0);
            ++nInharm;
        } else {
            const double pre = maxDevCents(in);
            out = correct(in);
            const long dLen = (long)out.size() - (long)in.size();
            assert(std::llabs(dLen) < (long)in.size() / 20);   // < 5% length float
            const double post = maxDevCents(out);
            std::printf("%-16s %-6s %-11s %+8.2f %+8.2f  len%+ld%s\n",
                        nm.c_str(), cat.c_str(), "tonal", pre, post, dLen,
                        post > 2.0 ? "  OVER" : "");
            if (post > worst) { worst = post; worstName = nm; }
            ++nTonal;

            if (!writeWav(outDir + "\\" + nm + ".wav", out, sr)) {
                std::printf("  WRITE FAIL %s\n", nm.c_str()); ++nFail;
            }
            continue;
        }

        // inharmonic write + report
        std::printf("%-16s %-6s %-11s %8s %8s\n",
                    nm.c_str(), cat.c_str(), "inharmonic", "  -", "  - (byte-id)");
        if (!writeWav(outDir + "\\" + nm + ".wav", out, sr)) {
            std::printf("  WRITE FAIL %s\n", nm.c_str()); ++nFail;
        }
    }

    std::printf("\n%d tonal corrected, %d inharmonic passthrough, %d failures\n",
                nTonal, nInharm, nFail);
    std::printf("worst residual tonal drift: %.2f cents (%s)%s\n",
                worst, worstName.c_str(), worst > 2.0 ? "  *** OVER 2c ***" : "  (all < 2c)");
    return nFail ? 1 : 0;
}
