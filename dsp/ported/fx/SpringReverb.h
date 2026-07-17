#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Lo-fi SPRING reverb (v3.7 cherry-pick, rev_type index 3 -- Room=0/Plate=1/
    Hall=2/Spring=3, this order chosen specifically to avoid Fable5's
    breaking Spring=0 reorder; see research/AUDIT-3.7.1-vs-main-2026-07-14.md).

    Not a physical spring-tank model -- a cheap, deliberately-lo-fi emulation
    of the mechanical "boing": a short allpass dispersion chain gives the
    metallic chirp, one short feedback delay loop gives the "sprong" decay
    (damped + 16-bit-truncated in the loop, for lo-fi grit consistent with
    this project's other feedback DSP), a transient "bounce" excites the
    allpasses harder on note attacks, and a golden-ratio stereo tap decorrelates
    L/R from the one underlying mono loop.

    No Fable5 3.7.1 source was available on this VM to port from (only ever
    extracted to an ephemeral scratch dir on a different machine) -- this is
    a fresh implementation from the resolved spec text (AUDIT doc, "Decisions
    -- RESOLVED" section): "a 3-5 stage allpass dispersion chain ... + one
    short feedback delay loop (~25-50 ms) with a one-pole LP damper + 16-bit
    feedback truncation + transient bounce ... + golden-ratio stereo tap."

    Bounded by construction: the Schroeder allpass stages (|c| < 1) are
    unconditionally stable, and the feedback loop's write is tanh-saturated
    (naturally bounded to (-1,1) for any input, no unity-slope preservation
    needed here -- this is a character effect, not a linearizable one) before
    it re-enters the loop, so recirculating energy can never run away
    regardless of the feedback coefficient.

    Mono in, stereo out. JUCE-free, RT-safe after prepare(). */
class SpringReverb
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;

        // Allpass dispersion chain: 4 stages (spec range 3-5), short and
        // INCOMMENSURATE delays (a few ms each, not integer-related) so the
        // cascade disperses/chirps rather than reading as a comb filter.
        static constexpr float msTable[kStages] = { 3.7f, 5.9f, 8.3f, 11.1f };
        static constexpr float cTable[kStages]  = { 0.68f, 0.63f, 0.60f, 0.58f };
        for (int i = 0; i < kStages; ++i)
        {
            apLen[i] = std::max (1, (int) (msTable[i] * 0.001 * fs));
            apBuf[i].assign ((size_t) apLen[i], 0.0f);
            apIdx[i] = 0;
            apC[i]   = cTable[i];
        }

        // Feedback loop: ~38 ms, mid of the spec's 25-50 ms range.
        loopLen = std::max (1, (int) (0.038 * fs));
        loopBuf.assign ((size_t) loopLen, 0.0f);
        loopIdx = 0;
        // Golden-ratio stereo tap: a second read point inside the loop,
        // offset by the golden ratio fraction of its length, so L/R
        // decorrelate from the one underlying mono loop.
        tapR = (int) (loopLen * 0.6180339887f) % loopLen;

        dampCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 4500.0 / fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : apBuf) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (loopBuf.begin(), loopBuf.end(), 0.0f);
        for (auto& z : apIdx) z = 0;
        loopIdx   = 0;
        damp      = 0.0f;
        attackEnv = 0.0f;
    }

    /** size 0..1 -> loop feedback 0.55..0.92 (short "boing" .. longer
        sustain, kept well under runaway even before the tanh safety net);
        damp 0..1 -> damper corner (higher = darker, faster-decaying
        repeats). Reuses the existing rev_size/rev_damp knobs -- no new
        params needed for Spring's own tone shaping. */
    void setParams (float size01, float damp01) noexcept
    {
        feedback = 0.55f + 0.37f * std::clamp (size01, 0.0f, 1.0f);
        const float d = std::clamp (damp01, 0.0f, 1.0f);
        dampCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * (7000.0 - 5500.0 * d) / fs);
    }

    void processSample (float in, float& outL, float& outR) noexcept
    {
        // Transient bounce: fast-attack/slow-release envelope follower on
        // the input excites the allpass chain harder right on note attacks
        // (the spring's mechanical "bounce"), settling back down under
        // sustained input.
        const float a = std::abs (in);
        attackEnv += (a > attackEnv ? 0.35f : 0.01f) * (a - attackEnv);
        float x = in * (1.0f + 1.5f * attackEnv);

        for (int i = 0; i < kStages; ++i)
            x = tickAllpass (i, x);
        // Allpass dispersion chains can produce transient peaks above the
        // input's own amplitude (phase-aligned reflections under adversarial
        // input) even though each stage is individually stable -- a safety
        // net on the FRESH contribution entering the loop (same shape as
        // RubberFilter's safety(), transparent in normal use, only engages
        // under extreme/continuous full-scale stress).
        x = safety (x);

        // Feedback loop: one-pole LP damper on the tap that's about to be
        // recirculated, then a saturating (bounded) write with 16-bit
        // feedback truncation (lo-fi grit, matching this project's other
        // vintage-character DSP).
        const int   readIdx = loopIdx;
        const float loopOut = loopBuf[(size_t) readIdx];
        damp += dampCoeff * (loopOut - damp);
        float fed = std::tanh (damp * feedback);
        fed = std::trunc (fed * 32767.0f) / 32767.0f;
        loopBuf[(size_t) loopIdx] = x + fed;
        if (++loopIdx >= loopLen) loopIdx = 0;

        outL = loopOut;                                            // this sample's pre-write tap
        outR = loopBuf[(size_t) ((readIdx + tapR) % loopLen)];      // golden-ratio-spaced tap
    }

private:
    static constexpr int kStages = 4;

    /** Transparent below |4|, softly limits beyond -- same pure stability
        net pattern as RubberFilter::safety(). */
    static float safety (float v) noexcept
    {
        const float a = std::abs (v);
        return a <= 4.0f ? v : std::copysign (4.0f + std::tanh (a - 4.0f), v);
    }

    /** Schroeder one-delay-line allpass: v = x - c*delayed; y = delayed +
        c*v; write v. Stable for |c| < 1 (our table is 0.58-0.68). */
    float tickAllpass (int i, float x) noexcept
    {
        auto&       buf     = apBuf[i];
        const float delayed = buf[(size_t) apIdx[i]];
        const float v       = x - apC[i] * delayed;
        const float y       = delayed + apC[i] * v;
        buf[(size_t) apIdx[i]] = v;
        if (++apIdx[i] >= apLen[i]) apIdx[i] = 0;
        return y;
    }

    double fs = 48000.0;

    std::vector<float> apBuf[kStages];
    int   apLen[kStages] {}, apIdx[kStages] {};
    float apC[kStages] {};

    std::vector<float> loopBuf;
    int   loopLen = 1, loopIdx = 0, tapR = 0;
    float feedback = 0.7f, dampCoeff = 0.3f, damp = 0.0f, attackEnv = 0.0f;
};

} // namespace dreamer
