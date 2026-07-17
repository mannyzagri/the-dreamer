#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** CONDITION + STABILITY trim pots (addendum 03, B7/B8).

    Models unit wear and supply instability. Both default to 1.0 (pristine):
    at 1.0 this class is bit-transparent (no hiss, zero drift).

    condition (0..1): as lowered, broadband hiss is injected into the
    output; amplitude = ((1-c)^2) * 0.02 (~ -34 dBFS at 0). Per-channel
    decorrelated noise streams.

    stability (0..1): as lowered, ~50 Hz sample-and-hold random drift
    (one-pole smoothed, ~8 ms) is generated on three independent streams:
      drift.tune    -> apply as +/-20 cents  * depth to osc pitch
      drift.cutoff  -> apply as +/-0.15 oct  * depth to filter cutoff
      drift.delay   -> apply as +/-2 %       * depth to delay time
    depth = 1 - stability. Values returned in [-1, 1] * depth; the caller
    applies the musical scaling above.

    JUCE-free, RT-safe. Call tick() once per sample.
*/
class Instability
{
public:
    struct Drift { float tune = 0.0f, cutoff = 0.0f, delay = 0.0f; };

    void prepare (double sampleRate, uint32_t seed) noexcept
    {
        fs = sampleRate;
        holdSamples = std::max (1, (int) (fs / 50.0));         // ~50 Hz
        smooth = 1.0f - (float) std::exp (-1.0 / (fs * 0.008)); // ~8 ms
        for (int i = 0; i < 5; ++i) rng[i] = seed * (uint32_t) (2 * i + 1) | 1u;
        counter = 0;
        target = cur = Drift {};
    }

    void setAmounts (float condition01, float stability01) noexcept
    {
        hissAmp = 0.02f * sq (1.0f - std::clamp (condition01, 0.0f, 1.0f));
        depth   = 1.0f - std::clamp (stability01, 0.0f, 1.0f);
    }

    void tick() noexcept
    {
        if (depth <= 0.0f) { cur = Drift {}; return; }

        if (--counter <= 0)
        {
            counter = holdSamples;
            target.tune   = bip (rng[0]);
            target.cutoff = bip (rng[1]);
            target.delay  = bip (rng[2]);
        }
        cur.tune   += smooth * (target.tune   - cur.tune);
        cur.cutoff += smooth * (target.cutoff - cur.cutoff);
        cur.delay  += smooth * (target.delay  - cur.delay);
    }

    Drift drift() const noexcept
    {
        return { cur.tune * depth, cur.cutoff * depth, cur.delay * depth };
    }

    float hiss (int ch) noexcept
    {
        if (hissAmp <= 0.0f) return 0.0f;
        return bip (rng[3 + (ch & 1)]) * hissAmp;
    }

private:
    static float sq (float x) noexcept { return x * x; }

    static float bip (uint32_t& s) noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (s >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    double fs = 48000.0;
    int holdSamples = 960, counter = 0;
    float smooth = 0.01f, depth = 0.0f, hissAmp = 0.0f;
    uint32_t rng[5] { 1, 2, 3, 4, 5 };
    Drift target, cur;
};

} // namespace dreamer
