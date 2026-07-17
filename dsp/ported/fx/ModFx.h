#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Shared engine for CHORUS and FLANGER (rev 7 GUI "Mod-FX" block): a
    stereo pair of LFO-modulated delay lines, L/R at 90 degrees of LFO
    phase for width. Chorus uses it with no feedback; flanger adds
    feedback for the resonant comb "jet" sound. Neither effect is modeled
    on the original hardware (rev 7 is a new addition beyond the RD-H3O+
    architecture) -- the depth/rate mappings below are reasonable starting
    points, tunable by ear, not measurements.

    Buffers sized once in prepare() for the largest delay either caller
    will ask for; process() never allocates. JUCE-free, real-time safe.
*/
class ModDelayFx
{
public:
    void prepare (double sampleRate, float maxDelayMs) noexcept
    {
        fs = sampleRate;
        const auto n = (size_t) std::ceil (fs * (maxDelayMs * 0.001 + 0.005)) + 4;
        bufL.assign (n, 0.0f);
        bufR.assign (n, 0.0f);
        writeIdx = 0;
        lfoPhase = 0.0f;
    }

    void reset() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        lfoPhase = 0.0f;
    }

    /** baseMs/rangeMs: modulated delay = baseMs + rangeMs*(0.5+0.5*sin(lfo)).
        rateHz: LFO speed. fb: feedback into the delay line (0 for chorus).
        mix: wet blend applied to the (already stereo) l/r pair in place. */
    void process (float& l, float& r, float baseMs, float rangeMs, float rateHz,
                  float fb, float mix) noexcept
    {
        lfoPhase += (float) (rateHz / fs);
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        const float lfoL = std::sin (kTwoPi * lfoPhase);
        const float lfoR = std::sin (kTwoPi * lfoPhase + 1.57079633f);   // +90 deg: stereo width

        const float wetL = readDelayed (bufL, baseMs + rangeMs * (0.5f + 0.5f * lfoL));
        const float wetR = readDelayed (bufR, baseMs + rangeMs * (0.5f + 0.5f * lfoR));

        bufL[writeIdx] = l + wetL * fb;
        bufR[writeIdx] = r + wetR * fb;
        if (++writeIdx >= bufL.size()) writeIdx = 0;

        // Equal-power (constant-power) crossfade, not a plain linear one
        // (2026-07-14 fix): wet is a delayed copy of dry, so they're highly
        // correlated -- a linear (1-m)+m blend can never exceed unity even
        // when in phase, so ANY destructive interference (comb-filtering,
        // inherent to mixing a signal with a delayed copy of itself) pulls
        // perceived loudness below the dry level. Worst at m=0.5, exactly
        // where "depth" sits at 50% (user-reported volume drop). Equal-power
        // keeps both legs at ~0.707 instead of 0.5 at the midpoint, so the
        // combined level stays close to unity instead of a guaranteed <=1
        // ceiling -- fixes the root cause, no separate makeup-gain needed.
        const float m = std::clamp (mix, 0.0f, 1.0f);
        const float dryGain = std::cos (m * kHalfPi);
        const float wetGain = std::sin (m * kHalfPi);
        l = l * dryGain + wetL * wetGain;
        r = r * dryGain + wetR * wetGain;
    }

private:
    float readDelayed (const std::vector<float>& buf, float ms) const noexcept
    {
        const float samplesBack = std::clamp ((float) (ms * 0.001 * fs), 0.0f, (float) buf.size() - 2.0f);
        float rp = (float) writeIdx - samplesBack;
        while (rp < 0.0f) rp += (float) buf.size();

        const size_t i0 = (size_t) rp;
        const size_t i1 = (i0 + 1) % buf.size();
        const float  frac = rp - (float) i0;
        return buf[i0] + frac * (buf[i1] - buf[i0]);
    }

    static constexpr float kTwoPi  = 6.28318530717958647692f;
    static constexpr float kHalfPi = 1.57079632679489661923f;
    double fs = 48000.0;
    std::vector<float> bufL, bufR;
    size_t writeIdx = 0;
    float  lfoPhase = 0.0f;
};

//==============================================================================
/** PHASER (rev 7 GUI "Mod-FX" block): classic LFO-swept cascade of
    first-order allpass stages, applied independently to L and R (no
    cross-channel delay needed -- the allpass sweep itself gives the
    moving-notch character). depth widens the sweep range and the wet
    mix; rate is the LFO speed. New addition, not hardware-modeled --
    constants are a reasonable starting point, tunable by ear. */
class Phaser
{
public:
    static constexpr int kStages = 6;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        lfoPhase = 0.0f;
        std::fill (std::begin (zL), std::end (zL), 0.0f);
        std::fill (std::begin (zR), std::end (zR), 0.0f);
    }

    void process (float& l, float& r, float depth, float rateHz, float mix) noexcept
    {
        lfoPhase += (float) (rateHz / fs);
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        const float sweep = 0.5f + 0.5f * std::sin (kTwoPi * lfoPhase);
        const float fMin = 200.0f, fMax = 200.0f + std::clamp (depth, 0.0f, 1.0f) * 3000.0f;
        const float a = allpassCoeff (fMin + (fMax - fMin) * sweep);

        float wl = l, wr = r;
        for (int i = 0; i < kStages; ++i)
        {
            const float yl = a * wl + zL[i]; zL[i] = wl - a * yl; wl = yl;
            const float yr = a * wr + zR[i]; zR[i] = wr - a * yr; wr = yr;
        }

        // Equal-power crossfade (2026-07-14 fix, same reasoning as ModDelayFx
        // above): dry combined with an allpass-shifted copy of itself is
        // exactly as correlated/comb-prone as a delay line is, and "depth"
        // drives this mix to 0.5 at 50% -- the linear blend's weakest point.
        const float m = std::clamp (mix, 0.0f, 1.0f);
        const float dryGain = std::cos (m * kHalfPi);
        const float wetGain = std::sin (m * kHalfPi);
        l = l * dryGain + wl * wetGain;
        r = r * dryGain + wr * wetGain;
    }

private:
    float allpassCoeff (float fc) const noexcept
    {
        const float t = std::tan ((float) (3.14159265358979 * fc / fs));
        return (t - 1.0f) / (t + 1.0f);
    }

    static constexpr float kTwoPi  = 6.28318530717958647692f;
    static constexpr float kHalfPi = 1.57079632679489661923f;
    double fs = 48000.0;
    float  lfoPhase = 0.0f;
    float  zL[kStages] {}, zR[kStages] {};
};

} // namespace dreamer
