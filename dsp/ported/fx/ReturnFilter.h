#pragma once

#include <cmath>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Return-path filter for delay/reverb wet signals (addendum 03, B4/B5).

    One instance = one filter (HP or LP), stereo, with a 3-state slope:
      0 = bypass, 1 = 12 dB/oct (one SVF stage), 2 = 24 dB/oct (two stages).
    Butterworth-ish damping (q = 1.414 per stage) at low cutoffs, adaptively
    reduced at high cutoffs to stay stable -- see setParams().

    Frequency mapping is GUI-authoritative (normalised 0..1 -> Hz).
    v1.0.15 (user bug report: "LP/HP frequency ranges are set to the wrong
    filter type"): the two ranges were SWAPPED at birth -- the HP swept up to
    22 kHz (killing the whole wet signal) while the LP topped out at 2 kHz
    (permanently dark). Now each type gets the range that suits it:
      high-pass:  20 * 100^v   ( 20 Hz ..  2 kHz)  -- trims lows off the tail
      low-pass : 200 * 110^v   (200 Hz .. 22 kHz)  -- darkens the tail; max = open
    (Defaults already sit at "no cut" on both: HP norm 0 -> 20 Hz, LP norm 1
    -> 22 kHz.) app.js freqHz() carries the identical mapping for the readout.

    Place post-effect, pre-mix on the wet path. JUCE-free, RT-safe.
*/
class ReturnFilter
{
public:
    enum class Mode { highpass, lowpass };

    void prepare (double sampleRate, Mode m) noexcept
    {
        fs = sampleRate; mode = m; reset();
    }

    void reset() noexcept
    {
        for (auto& s : st) for (auto& c : s) c = {};
    }

    /** slope: 0 bypass / 1 = 12 dB / 2 = 24 dB. norm: 0..1 (GUI map). */
    void setParams (int slopeState, float norm) noexcept
    {
        slope = std::clamp (slopeState, 0, 2);
        norm  = std::clamp (norm, 0.0f, 1.0f);

        const double hz = (mode == Mode::highpass)
                              ? 20.0  * std::pow (100.0, (double) norm)
                              : 200.0 * std::pow (110.0, (double) norm);

        const double fc = std::min (hz, fs * 0.45);
        f = std::min (2.0f * (float) std::sin (3.14159265358979 * fc / fs), 1.2f);

        // Bug (user report, 2026-07-15): "both highpass filters, when
        // enabled and the slider moved toward cut, mute the ENTIRE output,
        // not just the effect." Root cause: this Chamberlin SVF is only
        // stable for damping q < 2-f (same family as the RubberFilter.h
        // "DC hit" bug, 2026-07-11 -- see its own setParams() comment), and
        // this class used a FIXED q=1.414 with no cap at all. The HP side's
        // mapping reaches f > 0.586 (where 1.414 first exceeds 2-f) at only
        // ~66% up its slider at 48 kHz (200*110^0.664 = ~4.5 kHz) -- so the
        // entire top third of the HP range diverged to Inf/NaN. LP never
        // hit this (its map tops out at 2 kHz, f stays well under 0.586),
        // matching the report naming HP specifically. With nothing bounding
        // the state, the NaN propagated into the dry-mixed output
        // (delay: `l = x + wetL*wet`) or into the comp/limiter's persistent
        // gain-reduction envelope downstream of the reverb mix -- either
        // way, once one sample goes NaN, everything multiplied by it stays
        // silent/NaN forever (not just the wet signal), matching "entire
        // output completely mutes."
        // Fix: scale q down as f grows so q < 2-f ALWAYS holds (0.9x
        // margin, not the bare edge -- f is mathematically bounded to
        // ~1.975 by the fc cap above, so this can never go non-positive).
        // Only detunes from the exact Butterworth alignment once cutoff
        // climbs past ~3 kHz (48 kHz host) -- inaudible in practice, since
        // that's already deep into "cuts most of the spectrum" territory
        // for a return-path HP. (v1.0.15 range swap: it's now the LP side
        // that reaches the high-f region; the cap is type-agnostic, so the
        // protection carries over unchanged.)
        q = std::min (1.414f, 0.9f * (2.0f - f));
    }

    float processSample (int ch, float x) noexcept
    {
        if (slope == 0) return x;
        x = tick (st[0][ch & 1], x);
        if (slope == 2) x = tick (st[1][ch & 1], x);
        return x;
    }

private:
    struct Stage { float lp = 0.0f, bp = 0.0f; };

    float tick (Stage& s, float in) noexcept
    {
        s.lp += f * s.bp;
        const float hp = in - s.lp - q * s.bp;
        s.bp += f * hp;
        // Hard backstop, belt-and-braces, SAME ceiling as RubberFilter's own
        // safety() (|4|): a hot legitimate wet signal (e.g. SpringReverb's
        // own worst-case peak, ~5.8) never touches this at all in bypass
        // mode (slope==0 returns x directly, above) -- when filtering IS
        // engaged, this clips the FILTER'S OWN resonant state, which should
        // stay close to the input's amplitude scale for a stable (q<2-f)
        // loop, not the raw wet-signal peak; matching RubberFilter's
        // convention exactly is the more conservative, consistent choice.
        s.lp = safety (s.lp);
        s.bp = safety (s.bp);
        return mode == Mode::lowpass ? s.lp : hp;
    }

    static float safety (float v) noexcept
    {
        const float a = std::abs (v);
        return a <= 4.0f ? v : std::copysign (4.0f + std::tanh (a - 4.0f), v);
    }

    double fs = 48000.0;
    Mode mode = Mode::lowpass;
    int slope = 0;
    float f = 0.1f, q = 1.414f;
    Stage st[2][2];   // [stage][channel]
};

} // namespace dreamer
