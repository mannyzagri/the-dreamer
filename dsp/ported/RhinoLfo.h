#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Control-rate LFO for the GUI_XL modulation matrix (variant B, RHINO_XL).

    Five shapes (SINE / TRI / RAMP / SQUARE / S&H); value() in [-1, +1];
    per-sample tick(). Free-running -- no tempo sync (per the GUI_XL 2b spec
    and the user's 2026-07-17 decision; tempo-sync is a parked rev-18 design
    item, and per-LFO depth lives in the matrix amounts, not here).

    S&H draws one new value per LFO cycle from the same xorshift stream idiom
    as Instability.h / MorphOscillator's noise, so it is deterministic and
    seedable (no Math.random-style nondeterminism -- important for tests and
    for the static scope preview, which samples LFOs at a fixed 1/8 phase).

    Compiled unconditionally (JUCE-free, header-only, real-time safe): it is a
    plain value source with no side effects until the matrix routes it, so it
    never touches the mode-1 audio path. */
class Lfo
{
public:
    enum class Shape : int { sine = 0, tri = 1, ramp = 2, square = 3, sh = 4 };

    /** Param(0..100) -> rate in Hz: exponential 0.05 Hz .. 30 Hz.
        The ONE canonical map -- shared by the processor (setRateHz), the GUI
        (rate LCD), and the tests, so display and DSP always agree.
        v=0 -> 0.05 Hz, v=50 -> ~1.22474 Hz, v=100 -> 30 Hz. */
    static float rateHzFromParam (float v0to100) noexcept
    {
        return 0.05f * std::pow (600.0f, std::clamp (v0to100, 0.0f, 100.0f) * 0.01f);
    }

    void prepare (double sampleRate, uint32_t seed) noexcept
    {
        fs        = sampleRate;
        phase     = 0.0f;
        rng       = seed | 1u;             // never 0 (xorshift would stick)
        shValue   = nextRand();
        setRateHz (1.0f);
    }

    void reset() noexcept
    {
        phase   = 0.0f;
        shValue = nextRand();
    }

    void setShape (int s)     noexcept { shape = (Shape) std::clamp (s, 0, 4); }
    void setRateHz (float hz) noexcept { inc = (float) (std::clamp (hz, 0.001f, 100.0f) / fs); }

    /** Advance one sample -- call exactly once per sample. A new S&H value is
        latched at each cycle wrap. */
    void tick() noexcept
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            shValue = nextRand();
        }
    }

    /** Current bipolar value in [-1, +1]. */
    float value() const noexcept
    {
        switch (shape)
        {
            case Shape::sine:   return std::sin (kTwoPi * phase);
            case Shape::tri:    return 1.0f - 4.0f * std::abs (phase - 0.5f);  // -1 @0, peak +1 @0.5
            case Shape::ramp:   return 2.0f * phase - 1.0f;                    // -1 -> +1
            case Shape::square: return phase < 0.5f ? 1.0f : -1.0f;
            case Shape::sh:
            default:            return shValue;
        }
    }

    /** Value at an ARBITRARY phase (0..1) for the SAME shape -- used by the
        scope preview, which samples LFOs statically at a fixed 1/8 cycle
        rather than animating them. Does not touch the running phase/state. */
    float valueAtPhase (float ph) const noexcept
    {
        ph = ph - std::floor (ph);
        switch (shape)
        {
            case Shape::sine:   return std::sin (kTwoPi * ph);
            case Shape::tri:    return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case Shape::ramp:   return 2.0f * ph - 1.0f;
            case Shape::square: return ph < 0.5f ? 1.0f : -1.0f;
            case Shape::sh:
            default:            return shValue;   // S&H is not a function of phase
        }
    }

    float phaseNow() const noexcept { return phase; }

private:
    float nextRand() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;   // [-1, +1)
    }

    static constexpr float kTwoPi = 6.28318530717958647692f;

    double   fs      = 48000.0;
    float    phase   = 0.0f;
    float    inc     = 0.0f;
    float    shValue = 0.0f;
    Shape    shape   = Shape::sine;
    uint32_t rng     = 0x2545F491u;
};

} // namespace dreamer
