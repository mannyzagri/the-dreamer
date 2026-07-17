// ToneSvf -- the per-tone TVF (FEATURES.md section 1): era-simple Chamberlin
// state-variable filter with LP24 / LP12 / BP / HP modes. The math follows
// the verified bank's Tvf2Pole (two half-steps per sample, sr*0.22 stability
// ceiling, q = 1 - 0.95*res) so the LP12 mode is that placeholder exactly;
// LP24 cascades a second, lightly-damped stage; BP/HP are the standard
// Chamberlin taps. The character filters (Rhino ports) are GLOBAL by design
// -- this stays plain, as the JD tone TVFs were.
//
// C++17, header-only, JUCE-free, real-time safe.

#pragma once
#include <cmath>

namespace dreamer {

class ToneSvf {
public:
    enum class Mode : int { lp24 = 0, lp12 = 1, bp = 2, hp = 3 };

    void setSampleRate(double sr) noexcept { sr_ = sr; }
    void setMode(int m) noexcept {
        if (m < 0) m = 0; if (m > 3) m = 3;
        mode_ = (Mode)m;
    }
    void setCutoffRes(double hz, double res01) noexcept {
        hz = std::min(hz, sr_ * 0.22);   // Chamberlin stability margin (bank convention)
        f_ = 2.0 * std::sin(3.14159265358979323846 * hz / sr_);
        q_ = 1.0 - 0.95 * std::min(1.0, std::max(0.0, res01));
    }
    void reset() noexcept { low1_ = band1_ = high1_ = low2_ = band2_ = 0.0; }

    float process(float in) noexcept {
        // stage 1: two half-steps (Tvf2Pole idiom)
        for (int i = 0; i < 2; ++i) {
            low1_ += f_ * 0.5 * band1_;
            high1_ = in - low1_ - q_ * band1_;
            band1_ += f_ * 0.5 * high1_;
        }
        switch (mode_) {
        case Mode::lp12: return (float)low1_;
        case Mode::bp:   return (float)band1_;
        case Mode::hp:   return (float)high1_;
        case Mode::lp24:
        default:
            for (int i = 0; i < 2; ++i) {          // stage 2, lightly damped
                low2_ += f_ * 0.5 * band2_;
                const double high = low1_ - low2_ - kStage2Damp * band2_;
                band2_ += f_ * 0.5 * high;
            }
            return (float)low2_;
        }
    }

private:
    static constexpr double kStage2Damp = 1.2;   // resonance lives in stage 1
    Mode mode_ = Mode::lp24;
    double sr_ = 44100.0, f_ = 0.5, q_ = 1.0;
    double low1_ = 0.0, band1_ = 0.0, high1_ = 0.0, low2_ = 0.0, band2_ = 0.0;
};

} // namespace dreamer
