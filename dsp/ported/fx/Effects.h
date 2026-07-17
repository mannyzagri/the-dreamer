#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** Output distortion, control semantics per the original (addendum rev 02):
    "drive" = input strength into the shaper, "gain"/mix = dry/wet blend.

    Six shaper modes; all share the drive mapping (g = 1 + 9*drive) and the
    1/sqrt(g) loudness normalisation, so switching modes at equal settings
    stays comparable in level. DIG drives its quantise grid with the same
    input gain as the others (an ungained grid sat ~10 dB quieter than the
    saturating modes at typical signal levels -- user-reported bug).

    COLOR (0..1, 0.5 = neutral/today's sound) gives each mode a second
    character axis:
      - soft/hard/fuzz/over : dark <-> bright tilt (one-pole pivot ~800 Hz)
      - fold                : fold depth (0.5x .. 4x pre-gain into the folds)
      - dig                 : sample-rate reduction (hold 1..32 samples,
                              engages above 0.5; below stays clean-rate)

    Instance-based, coefficients memoised in set(); tilt/decimator state is
    per-voice. RT-safe.
*/
class Distortion
{
public:
    enum class Mode : int { soft = 0, hard = 1, fold = 2, dig = 3, fuzz = 4, over = 5 };

    void prepare (double sampleRate) noexcept
    {
        tiltCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 800.0 / sampleRate);
        reset();
    }

    void reset() noexcept { tiltLp = 0.0f; held = 0.0f; holdCount = 0; }

    /** Call when mode/drive/color may have changed (cheap no-op if not). */
    void set (Mode newMode, float drive, float color = 0.5f) noexcept
    {
        const float d = std::clamp (drive, 0.0f, 1.0f);
        const float c = std::clamp (color, 0.0f, 1.0f);
        if (newMode == mode && d == lastDrive && c == lastColor)
            return;

        mode      = newMode;
        lastDrive = d;
        lastColor = c;
        g         = 1.0f + 9.0f * d;
        norm      = 1.0f / std::sqrt (g);
        q         = std::exp2 (12.0f - 10.0f * d);   // dig: drive 0 ~12 bit, 1 ~2 bit
        bias      = 0.6f * d;                        // fuzz asymmetry
        tb        = std::tanh (bias);
        // Asymmetric bias means tanh(u+bias) saturates to DIFFERENT limits on
        // each side (-1-tb .. 1-tb before scaling): a single shared scale
        // calibrated to the (larger-magnitude) negative side left the
        // positive side well under the other modes' ceiling -- user-reported
        // "fuzz has a volume drop" (2026-07-14). Split pos/neg scales so BOTH
        // sides independently reach the same +-norm ceiling as every other
        // mode, preserving the asymmetric CLIP SHAPE (fuzz's actual character)
        // while fixing the net loudness deficit.
        fuzzScaleNeg = norm / (1.0f + tb);
        fuzzScalePos = norm / (1.0f - tb);

        tilt      = 2.0f * c - 1.0f;                              // -1 dark .. +1 bright
        foldGain  = std::pow (4.0f, c - 0.5f);                    // 0.5x .. 4x, 1 at neutral
        holdN     = c > 0.5f ? 1 + (int) std::lround ((c - 0.5f) * 2.0f * 31.0f) : 1;
    }

    float processSample (float x, float mix) noexcept
    {
        const float u = g * x;
        bool  tiltable = true;

        float wet;
        switch (mode)
        {
            case Mode::hard:                                   // brutal clip
                // Extra 3x pre-gain before the clamp (2026-07-14, user-
                // reported "soft and hard sound almost the same"): at shared
                // drive settings, tanh(u) and clamp(u,-1,1) are both still
                // near-linear until u exceeds ~1, so soft/hard only diverged
                // right at the very top of the drive range where tanh has
                // ALSO already saturated close to the same ceiling. Clipping
                // 3x earlier makes hard's flat-top character audibly distinct
                // from soft across most of the knob, not just its extreme end.
                // Ceiling unchanged (still clamped to +-1 * norm).
                wet = std::clamp (3.0f * u, -1.0f, 1.0f) * norm;
                break;

            case Mode::fold:                                   // triangle wavefold
            {
                // color = fold depth: more pre-gain -> more folds
                const float t = (u * foldGain) * 0.25f + 0.25f;
                wet = (4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f) * norm;
                tiltable = false;
                break;
            }

            case Mode::dig:                                    // bit + rate crush
                // Driven grid (same gain law as the other modes), clipped at
                // full scale; color > 0.5 adds sample-rate reduction.
                if (++holdCount >= holdN)
                {
                    holdCount = 0;
                    held = std::clamp (std::trunc (u * q) / q, -1.0f, 1.0f) * norm;
                }
                wet = held;
                tiltable = false;
                break;

            case Mode::fuzz:                                   // biased, asymmetric
            {
                const float raw = std::tanh (u + bias) - tb;
                wet = raw * (raw >= 0.0f ? fuzzScalePos : fuzzScaleNeg);
                break;
            }

            case Mode::over:                                   // cubic soft-knee overdrive
            {
                const float c = std::clamp (u, -1.0f, 1.0f);
                wet = (1.5f * c - 0.5f * c * c * c) * norm;
                break;
            }

            case Mode::soft:                                   // the v2 tanh (default)
            default:
                wet = std::tanh (u) * norm;
                break;
        }

        // color tilt for the smooth shapers: below 0.5 blends toward the
        // low-passed wet (dark), above adds HF emphasis (bright)
        if (tiltable)
        {
            tiltLp += tiltCoeff * (wet - tiltLp);
            if (tilt < 0.0f)
                wet += (-tilt) * (tiltLp - wet);
            else if (tilt > 0.0f)
                wet += tilt * 1.2f * (wet - tiltLp);
        }

        const float m = std::clamp (mix, 0.0f, 1.0f);
        return x * (1.0f - m) + wet * m;
    }

private:
    Mode  mode = Mode::soft;
    float lastDrive = -1.0f, lastColor = -1.0f;
    float g = 1.0f, norm = 1.0f, q = 4096.0f, bias = 0.0f, tb = 0.0f;
    float fuzzScalePos = 1.0f, fuzzScaleNeg = 1.0f;
    float tilt = 0.0f, foldGain = 1.0f;
    int   holdN = 1, holdCount = 0;
    float held = 0.0f, tiltLp = 0.0f, tiltCoeff = 0.1f;
};

//==============================================================================
/** One-pole DC blocker (~10 Hz), ported from the v3 reference drop.

    The envelope-swept Chamberlin cascade traps a quasi-DC charge in its lp
    integrators during fast cutoff sweeps (high env_mod); on note retriggers
    -- most audibly when a host loop restarts -- that charge releases as a
    one-sided "inverse phase" thump. Blocking DC after the distortion kills
    it without touching the filter's character. */
class DCBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        R = 1.0f - (float) (2.0 * 3.14159265358979 * 10.0 / sampleRate);
        xm1 = ym1 = 0.0f;
    }

    void reset() noexcept { xm1 = ym1 = 0.0f; }

    float processSample (float x) noexcept
    {
        const float y = x - xm1 + R * ym1;
        xm1 = x;
        ym1 = y;
        return y;
    }

private:
    float R = 0.9987f, xm1 = 0.0f, ym1 = 0.0f;
};

//==============================================================================
/** Vintage-engine output truncation: 16-bit, round toward zero, no dither.
    (The original rendered/exported at 16-bit 44.1 kHz.) */
struct Truncate16
{
    static float processSample (float x) noexcept
    {
        constexpr float scale = 32767.0f;
        return std::trunc (x * scale) / scale;
    }
};

/** Truncate16 adapted to 12 bits (v1.0.19 test build): the early-80s PCM
    delay converter fingerprint -- see StereoDelay's DIG voicing. Same
    round-toward-zero, no-dither law, coarser grid. */
struct Truncate12
{
    static float processSample (float x) noexcept
    {
        constexpr float scale = 2047.0f;
        return std::trunc (x * scale) / scale;
    }
};

//==============================================================================
/** Stereo feedback delay, send-style: out = in + wet * delayed.

    Three characters (MZ-B52+ GUI):
      - TAPE : one-pole LP (~3 kHz) + gentle tanh saturation inside the
               feedback loop -- repeats darken and compress like worn tape.
      - PING : ping-pong. Input enters the L line; each repeat crosses
               L -> R -> L. First echo left, second right, and so on.
      - DIG  : clean digital repeats (the v2 behaviour).

    v1.0.19 TEST VOICING (RHINO_TEST_FEATURES -- variant C compiles the new
    voicing IN and the old one OUT; variant A is untouched):
      - TAPE -> RE-201 Space Echo mockup (cheapest-mockup by spec: no
        oversampling, no tape physics):
          (a) wow+flutter -- the read position is modulated by a slow
              ~0.7 Hz LFO at +-0.15% of the delay time plus a faster
              ~6.5 Hz one at +-0.05%, fractional read via linear interp;
          (b) feedback-loop shaping -- one-pole HP ~120 Hz + one-pole LP
              ~5 kHz INSIDE the loop, so each repeat darkens AND thins
              progressively;
          (c) gentle level-dependent tanh saturation in the loop.
      - DIG -> early-80s PCM delay mockup: repeats stay EXACT and
        unmodulated, but the signal entering the line passes a fixed
        ~11 kHz converter lowpass (2x one-pole) + 12-bit truncation ONCE
        at entry -- recirculated content is NOT re-processed, so every
        repeat keeps the same era-converter fingerprint without
        compounding darkening.
      - PING unchanged. Delay-time zones / sync math unchanged.

    Two time modes (addendum rev 02):
      - Sync : beat division x host BPM (plugin-native behaviour)
      - Free : 1..1000 ms, matching the original's free-running time knob

    Mono input (the voice) -> stereo output. Buffers allocated once in
    prepare() (1 s + margin at the given rate); time/type changes never
    allocate. Feedback clamped <= 0.9.

    JUCE-free, real-time safe after prepare().
*/
class StereoDelay
{
public:
    enum class Type : int { tape = 0, ping = 1, dig = 2 };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // 4.2 s: the synced 1/1 division is 4 beats (4 s at 60 BPM); longer
        // times clamp to the buffer in setSeconds().
        const auto n = (size_t) std::ceil (fs * 4.2) + 4;
        bufL.assign (n, 0.0f);
        bufR.assign (n, 0.0f);
        writeIdx = 0;
        delaySamples = (int) (fs * 0.25);
        // ~3 kHz feedback damping for TAPE mode
        lpCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 3000.0 / fs);
        lpL = lpR = 0.0f;
#if RHINO_TEST_FEATURES
        // v1.0.19 voicing coefficients/state (see class comment).
        hp120Coeff  = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 120.0 / fs);
        lp5kCoeff   = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 5000.0 / fs);
        conv11kCoeff = 1.0f - (float) std::exp (-2.0 * 3.14159265358979 * 11000.0 / fs);
        wowInc  = (float) (0.7 / fs);
        flutInc = (float) (6.5 / fs);
        wowPh = flutPh = 0.0f;
        hpTrkL = hpTrkR = lp5kL = lp5kR = convLp1 = convLp2 = 0.0f;
#endif
    }

    void reset() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        lpL = lpR = 0.0f;
#if RHINO_TEST_FEATURES
        wowPh = flutPh = 0.0f;
        hpTrkL = hpTrkR = lp5kL = lp5kR = convLp1 = convLp2 = 0.0f;
#endif
    }

    void setType (Type t) noexcept { type = t; }

    /** Sync mode (MZ-D03+ rev 3): 8 divisions across the time knob.
        0=1/32, 1=1/16, 2=1/8, 3=3/16, 4=1/4, 5=3/8, 6=1/2, 7=1/1. */
    void setTimeSynced (int division, double bpm) noexcept
    {
        static constexpr double beats[] { 0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0 };
        bpm = std::clamp (bpm, 20.0, 999.0);
        setSeconds (beats[std::clamp (division, 0, 7)] * 60.0 / bpm);
    }

    /** Free mode: 1..1000 ms (original's knob range). */
    void setTimeFree (float ms) noexcept
    {
        setSeconds (std::clamp (ms, 1.0f, 1000.0f) * 0.001);
    }

    void processSample (float in, float feedback, float wet,
                        float& outL, float& outR) noexcept
    {
        float delL, delR;
        tickLine (in, feedback, delL, delR);

        const float w = std::clamp (wet, 0.0f, 1.0f);
        outL = in + delL * w;
        outR = in + delR * w;
    }

    /** Wet-tap variant (v3.7 cherry-pick, prerequisite for the return-filter
        + Instability wiring): advances the SAME delay line as processSample
        (identical read/write/feedback-shaping), but returns the raw wet
        tap BEFORE the dry/wet mix instead of mixing it in -- so a caller
        can run e.g. HP/LP return filters on the wet signal and mix it in
        manually. Call this INSTEAD OF processSample for a given sample,
        never both (each call advances the line once). */
    void processSampleWet (float in, float feedback, float& wetL, float& wetR) noexcept
    {
        tickLine (in, feedback, wetL, wetR);
    }

    /** Scale the current delay time by `factor` (~1 +/- a few %), re-derived
        from the last knob-set base time every call so repeated per-block
        nudges (STABILITY drift, v3.7 cherry-pick) never compound/drift away
        from the knob setting -- each call is a fresh, bounded offset from
        base, not a multiply-on-multiply. factor clamped defensively; the
        caller's drift range is +/-2%. */
    void nudgeTime (float factor) noexcept
    {
        const double s = (double) baseDelaySamples * (double) std::clamp (factor, 0.5f, 1.5f);
        delaySamples = std::clamp ((int) s, 1, (int) bufL.size() - 1);
    }

private:
    /** Shared line advance for processSample/processSampleWet: reads the
        current delay taps, writes this sample's (feedback-shaped, per-type)
        content, advances the write head. Returns the pre-mix wet taps. */
    void tickLine (float in, float feedback, float& delL, float& delR) noexcept
    {
        int readIdx = writeIdx - delaySamples;
        if (readIdx < 0)
            readIdx += (int) bufL.size();

        delL = bufL[(size_t) readIdx];
        delR = bufR[(size_t) readIdx];
        const float fb = std::clamp (feedback, 0.0f, 0.9f);

        switch (type)
        {
            case Type::tape:
#if RHINO_TEST_FEATURES
            {
                // v1.0.19 RE-201 mockup. (a) wow+flutter: both heads share
                // the motor, so ONE modulator drives both channels' read
                // position -- fractional, linear-interpolated. The taps
                // read here REPLACE the integer taps above (the wobble is
                // audible on the wet output, not just inside the loop).
                wowPh  += wowInc;  if (wowPh  >= 1.0f) wowPh  -= 1.0f;
                flutPh += flutInc; if (flutPh >= 1.0f) flutPh -= 1.0f;
                const float mod = 0.0015f * std::sin (6.2831853f * wowPh)     // +-0.15% @ ~0.7 Hz
                                + 0.0005f * std::sin (6.2831853f * flutPh);   // +-0.05% @ ~6.5 Hz
                const float dist = std::clamp ((float) delaySamples * (1.0f + mod),
                                               1.0f, (float) bufL.size() - 2.0f);
                float rp = (float) writeIdx - dist;
                if (rp < 0.0f) rp += (float) bufL.size();
                const int   r0 = (int) rp;
                const float frc = rp - (float) r0;
                const int   r1 = r0 + 1 >= (int) bufL.size() ? 0 : r0 + 1;
                delL = bufL[(size_t) r0] + frc * (bufL[(size_t) r1] - bufL[(size_t) r0]);
                delR = bufR[(size_t) r0] + frc * (bufR[(size_t) r1] - bufR[(size_t) r0]);

                // (b) loop shaping: HP ~120 Hz thins + LP ~5 kHz darkens
                // each pass; (c) gentle tanh saturation, level-dependent.
                hpTrkL += hp120Coeff * (delL - hpTrkL);
                hpTrkR += hp120Coeff * (delR - hpTrkR);
                lp5kL  += lp5kCoeff * ((delL - hpTrkL) - lp5kL);
                lp5kR  += lp5kCoeff * ((delR - hpTrkR) - lp5kR);
                const float satL = std::tanh (1.2f * lp5kL) * (1.0f / 1.2f);
                const float satR = std::tanh (1.2f * lp5kR) * (1.0f / 1.2f);
                bufL[(size_t) writeIdx] = in + satL * fb;
                bufR[(size_t) writeIdx] = in + satR * fb;
                break;
            }
#else
            {
                lpL += lpCoeff * (delL - lpL);
                lpR += lpCoeff * (delR - lpR);
                const float satL = std::tanh (1.2f * lpL) * (1.0f / 1.2f);
                const float satR = std::tanh (1.2f * lpR) * (1.0f / 1.2f);
                bufL[(size_t) writeIdx] = in + satL * fb;
                bufR[(size_t) writeIdx] = in + satR * fb;
                break;
            }
#endif

            case Type::ping:
                // Input feeds L only; repeats cross L -> R -> L.
                bufL[(size_t) writeIdx] = in + delR * fb;
                bufR[(size_t) writeIdx] =      delL * fb;
                break;

            case Type::dig:
            default:
#if RHINO_TEST_FEATURES
            {
                // v1.0.19 early-80s PCM mockup: the converter chain
                // (~11 kHz 2x one-pole LP + 12-bit truncation) applies ONCE
                // to the signal ENTERING the line; the recirculated taps
                // are written back untouched, so repeat N is an exact
                // fb^N-scaled copy of repeat 1 -- same fingerprint, no
                // compounding darkening. No modulation, by spec.
                convLp1 += conv11kCoeff * (in - convLp1);
                convLp2 += conv11kCoeff * (convLp1 - convLp2);
                const float w = Truncate12::processSample (convLp2);
                bufL[(size_t) writeIdx] = w + delL * fb;
                bufR[(size_t) writeIdx] = w + delR * fb;
                break;
            }
#else
                bufL[(size_t) writeIdx] = in + delL * fb;
                bufR[(size_t) writeIdx] = in + delR * fb;
                break;
#endif
        }

        if (++writeIdx >= (int) bufL.size())
            writeIdx = 0;
    }

    void setSeconds (double s) noexcept
    {
        // Both the live read length AND its nudge base reset together: a
        // fresh knob-driven time always wins over any earlier STABILITY
        // nudge (see nudgeTime), which re-applies (if still wanted) on the
        // caller's next per-block call.
        baseDelaySamples = std::clamp ((int) (s * fs), 1, (int) bufL.size() - 1);
        delaySamples = baseDelaySamples;
        // Time changes jump the read head -> possible click; crossfade is v1.1.
    }

    double fs = 48000.0;
    std::vector<float> bufL, bufR;
    int   writeIdx = 0, delaySamples = 12000, baseDelaySamples = 12000;
    Type  type = Type::dig;
    float lpCoeff = 0.3f, lpL = 0.0f, lpR = 0.0f;
#if RHINO_TEST_FEATURES
    // v1.0.19 voicing state (see class comment). TAPE: shared-motor wow/
    // flutter phases + per-channel HP/LP loop-shaping trackers. DIG: the
    // mono entry converter's 2x one-pole state.
    float wowPh = 0.0f, flutPh = 0.0f, wowInc = 0.0f, flutInc = 0.0f;
    float hp120Coeff = 0.02f, lp5kCoeff = 0.5f, conv11kCoeff = 0.76f;
    float hpTrkL = 0.0f, hpTrkR = 0.0f, lp5kL = 0.0f, lp5kR = 0.0f;
    float convLp1 = 0.0f, convLp2 = 0.0f;
#endif
};

} // namespace dreamer
