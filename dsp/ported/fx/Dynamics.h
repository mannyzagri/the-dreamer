#pragma once

#include <cmath>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Brickwall output clipper (v1.0.16, GUI_12's CLIP latch, param `clip_on`,
    default OFF). Ceiling -0.1 dBFS -- the output can NEVER exceed it:
    linear (bit-transparent) below the -3 dB knee, then a tanh ease that
    approaches the ceiling asymptotically. Stateless (no lookahead, no
    latency, no envelope), placed as the FINAL stage after output gain.
    Knee is C1-continuous (value and slope match at the knee point). */
struct BrickwallClip
{
    static float processSample (float x) noexcept
    {
        constexpr float c = 0.98855309f;   // 10^(-0.1/20) ceiling
        constexpr float t = 0.70794578f;   // 10^(-3/20)   knee start
        const float a = std::abs (x);
        if (a <= t) return x;
        const float y = t + (c - t) * std::tanh ((a - t) / (c - t));
        return std::copysign (y, x);
    }
};

//==============================================================================
/** Stereo-linked compressor / limiter (MZ-B52+ GUI "comp / limit" plate).

    Feed-forward peak design: one envelope follower on max(|L|, |R|) so the
    stereo image never tilts, log-domain gain computer, hard-knee.

      - COMP: attack 5 ms / release 80 ms, thresh -60..0 dB, ratio 1:1..20:1.
      - LIM : the LIM latch overrides to attack 0.1 ms / release 50 ms and
              ratio 20:1 -- a crude, grabby 90s-rack brickwall. Intentional.

    Makeup gain 0..+24 dB. Publishes the current block's worst gain
    reduction (positive dB) for the GUI meter via grDbMax()/resetGrMeter().

    JUCE-free, real-time safe after prepare().
*/
class CompLimiter
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        atkComp = coeffFor (5.0);
        relComp = coeffFor (80.0);
        atkLim  = coeffFor (0.1);
        relLim  = coeffFor (50.0);
        reset();
    }

    void reset() noexcept
    {
        env = 0.0f;
        grBlockMax = 0.0f;
    }

    /** Per-block settings; call once before the sample loop.
        Either latch engages processing: ON alone = compressor, LIM alone or
        with ON = the limiter preset (a LIM button that does nothing until
        ON is also lit would read as broken hardware). */
    void setParams (bool on, bool limiter_, float threshDb_,
                    float ratio_, float makeupDb_) noexcept
    {
        enabled   = on || limiter_;
        limiter   = limiter_;
        threshDb  = std::clamp (threshDb_, -60.0f, 0.0f);
        threshLin = std::pow (10.0f, threshDb * (1.0f / 20.0f));
        ratio     = limiter ? 20.0f : std::clamp (ratio_, 1.0f, 20.0f);
        makeup    = std::pow (10.0f, std::clamp (makeupDb_, 0.0f, 24.0f) * (1.0f / 20.0f));
    }

    void processSample (float& l, float& r) noexcept
    {
        if (! enabled)
            return;

        const float level = std::max (std::abs (l), std::abs (r));
        const float atk   = limiter ? atkLim : atkComp;
        const float rel   = limiter ? relLim : relComp;
        env += (level > env ? atk : rel) * (level - env);

        // Below threshold: gain is exactly makeup (hard knee) -- skip the
        // per-sample log10/pow on the common quiet path.
        if (env <= threshLin)
        {
            l *= makeup;
            r *= makeup;
            return;
        }

        const float envDb = 20.0f * std::log10 (env + 1.0e-9f);
        const float grDb  = (envDb - threshDb) * (1.0f - 1.0f / ratio);

        grBlockMax = std::max (grBlockMax, grDb);

        const float gain = std::pow (10.0f, -grDb * (1.0f / 20.0f)) * makeup;
        l *= gain;
        r *= gain;
    }

    /** Worst gain reduction (positive dB) since the last reset -- the
        processor copies this to an atomic for the GUI meter, per block. */
    float grDbMax() const noexcept { return grBlockMax; }
    void  resetGrMeter() noexcept  { grBlockMax = 0.0f; }

private:
    float coeffFor (double ms) const noexcept
    {
        return 1.0f - (float) std::exp (-1.0 / (fs * ms * 0.001));
    }

    double fs = 48000.0;
    float  atkComp = 0.01f, relComp = 0.001f, atkLim = 0.5f, relLim = 0.002f;

    bool  enabled = false, limiter = false;
    float threshDb = -12.0f, threshLin = 0.25f, ratio = 4.0f, makeup = 1.0f;

    float env = 0.0f, grBlockMax = 0.0f;
};

} // namespace dreamer
