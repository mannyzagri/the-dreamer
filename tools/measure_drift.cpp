// measure_drift.cpp -- TD-002 measurement (dsp-pass step 2). Measures the
// mid-loop pitch drift baked into the v3 library loops (bake_final.py's
// shared_drift Brownian bridge). Normalized autocorrelation, 0.3 s windows at
// 0/25/45/65/85% of each loop, reports Hz + cents vs 220 and the max deviation
// from the loop's own mean pitch (the v4 acceptance criterion: < +/-2 cents for
// tonal families; METAL_* + metal MORPHs are inharmonic -> flagged, not judged).
//
// Validation target (mac-opus, delivered v3 WAVs): PAD_02 +8.3c @65%,
// AIRY_01 +10.7c @25%. If this tool reproduces those, its correction twin is
// trustworthy.
//
// Build (vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES tools\measure_drift.cpp
//     /Fo:tools\bin\ /Fe:tools\bin\measure_drift.exe
//   tools\bin\measure_drift.exe [assets\loops] [NAME ...]   (no names -> all 130)

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

// identical RIFF reader to bake_loops_header.cpp
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

static const int    SR   = 44100;
static const double kRef = 220.0;
static const int    kWin = (int)(0.3 * SR);        // 13230-sample window
// search 165..294 Hz (period 150..267) -> +/-5 semitones around 220, no octave errors
static const int    kMinLag = 150, kMaxLag = 267;

// normalized-autocorrelation pitch of a Hann-windowed segment [start, start+kWin)
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
        w[(size_t)n] = ((double)s[(size_t)(start + n)] - mean) * h;   // DC-removed, windowed
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
    // parabolic interpolation around the peak for sub-sample period
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

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "assets\\loops";
    std::vector<std::string> names;
    for (int a = 2; a < argc; ++a) names.push_back(argv[a]);
    if (names.empty()) names = { "PAD_02", "AIRY_01", "VOX_01", "FM_01", "PAD_01", "ETHER_01" };

    const double pos[5] = { 0.0, 0.25, 0.45, 0.65, 0.85 };
    std::printf("TD-002 drift measure (0.3s windows, cents vs 220 Hz)\n");
    std::printf("%-16s %8s %8s %8s %8s %8s | maxdev-from-mean\n",
                "loop", "0%", "25%", "45%", "65%", "85%");
    double worst = 0.0; std::string worstName;
    for (const auto& nm : names) {
        std::vector<int16_t> s; uint32_t sr = 0;
        if (!readWav(dir + "\\" + nm + ".wav", s, sr) || sr != 44100) {
            std::printf("%-16s  READ FAIL\n", nm.c_str()); continue;
        }
        double c[5], mean = 0.0;
        for (int p = 0; p < 5; ++p) {
            const int start = (int)(pos[p] * (s.size() - kWin));
            c[p] = cents(pitchAt(s, start), kRef);
            mean += c[p];
        }
        mean /= 5.0;
        double dev = 0.0; for (int p = 0; p < 5; ++p) dev = std::fmax(dev, std::fabs(c[p] - mean));
        const bool inharm = nm.rfind("METAL_", 0) == 0 || nm == "MORPH_VOXMETAL"
                          || nm == "MORPH_METALAIR" || nm == "MORPH_ETHMETAL";
        std::printf("%-16s %+8.2f %+8.2f %+8.2f %+8.2f %+8.2f | %5.2f%s\n",
                    nm.c_str(), c[0], c[1], c[2], c[3], c[4], dev,
                    inharm ? "  (inharmonic-exempt)" : (dev > 2.0 ? "  DRIFT" : ""));
        if (!inharm && dev > worst) { worst = dev; worstName = nm; }
    }
    std::printf("\nworst tonal drift: %.2f cents (%s)\n", worst, worstName.c_str());
    return 0;
}
