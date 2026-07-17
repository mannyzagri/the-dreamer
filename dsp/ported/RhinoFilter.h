#pragma once

#include <cmath>
#include <algorithm>

namespace dreamer
{

//==============================================================================
/** The Rubber filter, v2: 4-pole (24 dB/oct) low pass with the
    "filter modification model" (manual-verified, addendum rev 02).

    Topology: two cascaded Chamberlin SVF stages, each run 2x oversampled.
    Resonance lives in stage 1; stage 2 is lightly damped and adds the
    second 12 dB of slope (manual: "dynamic 4 Pol (24dB) low pass").

    The FILTER MODIFICATION MODEL (all gated by 'character', the original's
    "mod" button -- confirmed hands-on that drive/squelch/tweet/tweak are
    inert when it is off):

      - filtDrive : overdrive INSIDE the resonance path of stage 1
                    ("plastic-like acid sounds"). Pass 5: soft-saturates the
                    resonance FEEDBACK (unity origin slope, tanh(g*fb)/g) so
                    higher drive runs the ring hotter/grittier and it self-
                    limits at the saturator ceiling -- an overdrive that ADDS
                    harmonics + level, per the recovered manual. Replaces the
                    pass-4 state clip, which only ATTENUATED (measured: drive
                    reduced every band). See research/todo/.
      - squelch   : "changes the resonance spectrum". A one-pole low-pass
                    damping filter on the resonance feedback -- higher
                    squelch darkens/hollows the resonance ring. Its corner
                    TRACKS the cutoff (proportional to `f`, not a fixed
                    frequency): conformance audit found the original fixed
                    ~3.8 kHz corner made squelch/tweak nearly inaudible at
                    typical acid-bass cutoffs (200-2000 Hz) -- almost no
                    resonance content sat above the shaping point, so
                    turning the knob did ~nothing exactly where it's used
                    most.                      [CURVE TBD BY MEASUREMENT]
      - tweet     : distortion placed BEFORE the filter ("very nasty").
                    Fixed tanh pre-shaper, PLUS a large cut to the
                    resonance-feedback damping (see tick()) so the extra
                    harmonics it generates get resonantly excited into an
                    extreme squelch instead of just being filtered away.
                                               [CURVE TBD BY MEASUREMENT]
      - tweak     : "raises the high pitch in the resonance part". A large
                    cut to the resonance-feedback damping -- pushes the
                    resonator toward (bounded) self-oscillation for an
                    extreme, screaming brightening. Same cutoff-tracking
                    fix as squelch.            [AMOUNT TBD BY MEASUREMENT]

    When character is OFF the resonance path is linear; a mild safety
    limiter (engaged only beyond |4|) plus a resonance ceiling keep the
    linear filter bounded without colouring normal operation.

    Historical note (v3.6 worklist): the modification model (squelch/
    drive/tweet/tweak) was introduced in the original's v2.0 as a "new
    resonance/filtering model" (period reviews, 1999). Its DSP was never
    publicly documented; our curves are measurement-fit approximations --
    see the [TBD BY MEASUREMENT] tags.

    v3 FILTER TYPES (merged from the v3 reference drop) -- a switch on the
    resonance path that STACKS with the character block:
      - classic : the filter above, bit-identical when selected.
      - liquid  : a TB-303-flavoured voicing. The real 303 is a 4-pole diode
                  ladder whose FIRST pole is mismatched, so it behaves like an
                  ~18 dB/oct filter (gentler, rounder than a 24 dB ladder) and
                  -- because the phase never reaches 360 deg -- does NOT self-
                  oscillate from resonance alone; the resonance is "led to an
                  overdrive" instead. We voice that here by blending the 2-pole
                  and 4-pole low-pass taps for the ~18 dB slope, plus a gentle
                  analog-ish saturation in the resonance path. Kept SIMPLE and
                  deliberately not pristine -- it goes wild under the shared
                  mod/drive/tweet/tweak overdrive engine like any other type.
      - scream  : regenerative feedback -- a saturated portion of the
                  band-pass state re-enters the INPUT, with reduced HF
                  damping. Out-resonates classic by design.
      - plane   : EMU Z-plane-inspired morph "for fun" -- a 4-pole low-pass
                  morphed into a phase-notch COMB (a 4-stage allpass cascade mixed
                  with the LP). v1.0.15 BIPOLAR resonance (user spec): 50% =
                  neutral plain LP (no comb, no ring); toward 0% = NEGATIVE-
                  polarity comb; toward 100% = POSITIVE-polarity comb -- and the
                  resonance FEEDBACK follows a V-curve (zero at 50%, full at
                  either extreme). `cutoff` positions the comb, so the filter
                  envelope sweeping cutoff walks the notches -- the vowel/comb
                  "Z-plane" sweep. Not a hardware model; a characterful extra.
                  SHARES the mod/drive/squelch/tweet/tweak character model with
                  every other type (user report, 2026-07-15: "mod button and
                  controls work on the first 3 filter types but not on plane" --
                  the original bypass comment's premise ("plane has no SVF
                  resonance") doesn't actually hold: plane's tick() runs the
                  SAME stage-1 resonant loop as classic/scream, `q` is set for
                  it identically, bp1 genuinely rings -- only the OUTPUT stage
                  (the comb mix) differs. `resonance` doing double duty as both
                  "comb depth" AND the character model's resoAmt scale is
                  consistent with how liquid/scream already reinterpret
                  resonance for their own voicing while still sharing the same
                  resoAmt-scaled model).

    JUCE-free, real-time safe.
*/
class RubberFilter
{
public:
    // v1.0.19 test types (DSP unconditional; PARAM-exposed only under
    // RHINO_TEST_FEATURES -- variant A's choice list stops at plane):
    //   - ladder : the ORIGINAL RubberDuck's Filter-Modification-OFF path,
    //              verbatim from ghidra-notes Pass 2 -- 4 IDENTICAL one-pole
    //              stages, coefficient smoothed (x+t)/2 per sample, parabolic
    //              cutoff-dependent resonance-feedback compensation
    //              ((c-0.7)^2*7 + 0.3)*reso, feedback-summed input hard-
    //              clipped (original +-32768 int -> +-1.0 in our float
    //              convention). See tickLadder().
    //   - ms20   : Korg MS-20-flavoured Sallen-Key stand-in -- the 12 dB/oct
    //              2-pole core (SVF stage 1) with a hard-ish ASYMMETRIC
    //              diode clipper INSIDE the resonance feedback path, so
    //              rising resonance screeches/distorts instead of staying
    //              clean; bounded self-oscillation allowed. See tickMs20().
    //   - wasp   : EDP Wasp CMOS flavour -- SVF core whose integrator
    //              OUTPUTS pass through an asymmetric CMOS-inverter-style
    //              soft-clip (gentle even-harmonic dirt even at low levels).
    //              See tickWasp().
    // All three SHARE the character model (drive/squelch/tweet/tweak) the
    // same way classic does. For LADDER that is an AUTHENTICITY DEVIATION,
    // flagged deliberately: the original's Mod-OFF path has NONE of the
    // character block -- it applies here anyway for GUI consistency with
    // every other type (v1.0.19 spec).
    enum class Type : int { classic = 0, liquid = 1, scream = 2, plane = 3,
                            ladder = 4, ms20 = 5, wasp = 6 };

    struct Character
    {
        bool  enabled  = false;
        float drive    = 0.0f;   // 0..1 resonance-path overdrive
        float squelch  = 0.0f;   // 0..1 resonance darkening
        bool  tweet    = false;  // pre-filter distortion
        bool  tweak    = false;  // resonance brightening
    };

    void prepare (double sampleRate) noexcept
    {
        baseFs = sampleRate;
        fs2 = baseFs * osFactor;
        reset();
    }

    /** Vintage (single-rate) vs Modern (2x oversampled) filter core.
        Single-rate is the ERA-FAITHFUL path: the one-sample-delay resonance
        loop runs at the host rate, so the coefficient f = 2*sin(pi*fc/fs) is
        larger and the loop rings closer to instability near cutoff -- the
        cheap, ringy near-Nyquist grit of the original (which ran fixed 44.1 kHz
        with NO oversampling on a Pentium 100). At single-rate, f hits its 1.0
        ceiling around fs/6 (~7.4 kHz at 44.1k), so the top of the cutoff range
        saturates -- itself an authentic period-filter trait. Modern keeps the
        tamed 2x-oversampled path. factor: 1 = single-rate, 2 = oversampled. */
    void setOversample (int factor) noexcept
    {
        factor = factor < 1 ? 1 : (factor > 4 ? 4 : factor);
        if (factor != osFactor)
        {
            osFactor = factor;
            fs2 = baseFs * osFactor;
        }
    }

    void reset() noexcept
    {
        lp1 = bp1 = lp2 = bp2 = damp = dampLo = outLp = 0.0f;
        apZ[0] = apZ[1] = apZ[2] = apZ[3] = 0.0f;   // plane comb allpass state
        lad1 = lad2 = lad3 = lad4 = ladC = 0.0f;    // ladder stages + smoothed coeff
        waspOut = 0.0f;                              // wasp shaped-output tap
    }

    void setParams (float cutoffHz, float resonance, const Character& c,
                    Type t = Type::classic) noexcept
    {
        type    = t;
        resoAmt = std::clamp (resonance, 0.0f, 1.0f);

        const float fc = std::clamp (cutoffHz, 20.0f, (float) (fs2 * 0.22));
        // Ceiling 1.0, NOT the original 1.2: above ~1.0 the cascade is only
        // marginally damped, and an accent+env_mod sweep PINS f at the
        // ceiling for ~20 ms -- long enough for the state to grow into the
        // safety() soft-clip, which rectifies one-sidedly and dumps a
        // sub-audio lurch into the integrators (the loop-restart "DC hit").
        // Verified: tap-point repro drops 0.91 -> baseline with 1.0.
        // f = 1.0 still reaches ~16 kHz at a 48 kHz host; the CUTOFF knob
        // itself (<= 12 kHz) never touches the ceiling -- only the very top
        // of boosted envelope sweeps, where the difference is inaudible.
        f = std::min (2.0f * std::sin (kPi * fc / (float) fs2), 1.0f);

        // Linear mode gets a slightly higher resonance floor for stability;
        // with the character block on, the drive nonlinearity is the limiter.
        const float resoCeil = c.enabled ? 1.0f : 0.985f;
        q = 2.0f * (1.0f - 0.995f * std::clamp (resonance, 0.0f, resoCeil));

        // tweak/tweet cut Q DIRECTLY (sharper resonance, closer to
        // self-oscillation) rather than only reshaping the feedback
        // spectrum: a q-based lever is deterministic -- it doesn't depend
        // on the momentary phase/magnitude of the resonance state the way
        // shaping fb's spectrum does, which measured as inconsistent
        // (sometimes barely moved, sometimes went the wrong direction
        // depending on env_mod). Halving-ish Q is what "extreme squelch"
        // actually requires; the stability cap below is the safety net.
        if (c.enabled)
        {
            // tweak is scaled BY the resonance setting (osc-config-v3.2,
            // photo-evidence discrimination test): "only audible when the
            // resonance is ringing -- at low resonance it does nearly
            // nothing." At resonance=0 the multiplier is 1 (no cut); it
            // ramps to the full self-oscillation-adjacent cut by
            // resonance=1. Distinguishes it from tweet, whose defining
            // trait is being audible even at LOW resonance (it's an
            // input-path effect via the pre-filter shaper below, always
            // active) -- so tweet's cut stays flat, not reso-gated.
            // Pass 3 (user ear test 2026-07-12: "tweak has no effect"):
            // a purely multiplicative cut was inaudible in every regime --
            // at high reso q is already ~0 so cutting it changes nothing,
            // at low reso it's gated off, in between it only halved q.
            // Deepened to 0.9 AND paired with a direct output brightening
            // in processSample() (the audible half of the fix).
            if (c.tweak) q *= 1.0f - 0.9f * std::clamp (resonance, 0.0f, 1.0f);
            // tweet is ONLY the pre-filter distortion (processSample's tanh
            // pre-shaper), per the manual: "a distortion module before the
            // filter". The former `q *= 0.35f` here was an UNDOCUMENTED second
            // job -- resonance-sharpening the manual attributes to tweak, not
            // tweet -- so it is removed; tweet's harmonics now come purely from
            // the pre-filter shaper and get resonantly excited by the ordinary
            // resonance path. (Manual-faithfulness cleanup, 2026-07-13.)
        }

        // Chamberlin stability needs damping < 2 - f. At LOW resonance
        // (q -> 2) with the envelope pinning f near the ceiling, the
        // unclamped q violates that and the filter lurches sub-audio (same
        // family as the accent "DC hit"). Cap both stages' damping.
        q     = std::min (q, 2.0f - f);
        damp2 = std::min (1.3f, 2.0f - f);

        // Two cutoff-tracking spectral split points:
        //   dampCoeff (3f)    -- scream's reduced-HF-damping term only.
        //   dampLoCoeff (1.2f) -- squelch/tweak. Pass 3: the old shared 3f
        //     split sat ABOVE the resonance ring (at ~1f), so squelch only
        //     ever damped the ring's 3rd+ harmonics -- audibly near-nothing
        //     at 100% (user ear test). At 1.2f the ring itself is inside
        //     the shaped band, so squelch at 100% genuinely chokes/darkens
        //     the ring, and tweak's brightening tap carries real ring
        //     content.                          [CURVE TBD BY MEASUREMENT]
        dampCoeff   = std::clamp (f * 3.0f, 0.02f, 0.9f);
        dampLoCoeff = std::clamp (f * 1.2f, 0.02f, 0.9f);
        // Output-tilt split for squelch's audible half (processSample).
        // Corner ~0.3x the INSTANTANEOUS cutoff (per-sample one-pole at
        // fs: coeff ~ 2f x 0.3): with envelope modulation the swept fc
        // rides kHz above the knob setting during the loud part of every
        // note, so a corner AT fc only shaved inaudible sheen (measured:
        // gesture mid-band 1.04x). Well below fc, the tilt shaves the
        // sweep's body + ring -- the knob audibly "squelches down" the
        // line at any drive setting. Upper cap ~0.12 rad/sample (~900 Hz
        // at 48k): the corner must stay BELOW the 1-3 kHz body no matter
        // how far the envelope sweeps fc, or full squelch shaves only
        // sheen (measured 1.6x; with the cap the whole body ducks).
        //                                       [TBD BY MEASUREMENT]
        outLpCoeff  = std::clamp (f * 0.6f, 0.02f, 0.10f);

        ch = c;
        // Pass 5 drive (see tick()): pre-gain into the resonance-FEEDBACK
        // saturator. driveGain feeds a unity-origin-slope shaper (tanh(g x)/g),
        // so raising it makes the ring saturate sooner -> the resonance runs
        // hotter and grittier (overdrive that ADDS harmonics + level), not the
        // pass-4 state clip that only attenuated. Linear curve is a starting
        // point; unlike the pass-4 state clip, a linear pre-gain does NOT crush
        // quiet-signal character here because the shaper's origin slope is 1.
        // Curve/endpoint TBD BY MEASUREMENT (capture C5); the DIRECTION is
        // manual-dictated ("overdrive in the resonance part").
        const float dk = std::clamp (ch.drive, 0.0f, 1.0f);
        driveGain = 1.0f + 8.0f * dk;

        // Plane (EMU-style morph LP -> phase-notch comb), v1.0.15 BIPOLAR
        // redesign (user spec: "plane sounds like LP; it should be LP with
        // comb the more it opens -- resonance at 0% = minus comb, 50% =
        // neutral, 100% = plus comb; feedback full at 0 and 100, zero at 50"):
        // the resonance knob is now V-SHAPED around its centre --
        //   50%       : plain clean 4-pole LP (no comb, max damping, no ring)
        //   toward 0% : NEGATIVE-polarity comb (0.5*(lp - ap) notch set) +
        //               resonance feedback ramping in (q law = classic's,
        //               driven by |reso-0.5|*2)
        //   toward 100%: POSITIVE-polarity comb (0.5*(lp + ap), the old
        //               behaviour) + the same feedback ramp
        // The two comb polarities put their notches at complementary
        // positions, so 0% and 100% are audibly different combs, not mirror
        // volume changes. The comb allpass corners stay comb-spaced
        // ((k+1)*fc) and track the cutoff -- the envelope still walks the
        // notches ("Z-plane" motion), now with a resonant ring under it at
        // the knob's extremes.
        if (type == Type::plane)
        {
            const float v = std::abs (std::clamp (resonance, 0.0f, 1.0f) - 0.5f) * 2.0f;
            q = std::min (2.0f * (1.0f - 0.995f * v), 2.0f - f);
            for (int k = 0; k < 4; ++k)
            {
                const float fk = std::min ((float) ((k + 1) * fc), (float) (baseFs * 0.45));
                const float t  = std::tan (kPi * fk / (float) baseFs);
                apC[k] = (t - 1.0f) / (t + 1.0f);
            }
        }

        // Ladder (v1.0.19): coefficient TARGET per the original's Mod-OFF law
        // (ghidra Pass 2) -- tickLadder() does the per-sample (x+t)/2 smoothing.
        if (type == Type::ladder)
            ladTarget = ladderCoeffTarget (cutoffHz);
    }

    //==========================================================================
    // Ladder law helpers (public statics so test_dsp can spot-check the
    // recovered laws directly, same convention as MorphOscillator's shapes).

    /** The original's Mod-OFF coefficient law, clamp(((cut+10)/255)*envCurve, <=1).
        DEVIATION (flagged): our cut arrives as one env-swept Hz value (Voice
        bakes its envelope into cutoffHz as an octave sweep), so the knob's
        0..1 position is reconstructed through the inverse of the cutoff
        parameter's skew-0.25 map (60..12000 Hz) and envCurve reaches the
        coefficient through that sweep rather than as the original's direct
        multiply. Exact 1:1 is the faithful-1017 path's job, not this type's. */
    static float ladderCoeffTarget (float cutoffHz) noexcept
    {
        const float lin  = std::clamp ((cutoffHz - 60.0f) / 11940.0f, 0.0f, 1.0f);
        const float norm = std::sqrt (std::sqrt (lin));            // ^(1/4): skew-0.25 inverse
        return std::min (1.0f, (norm * 255.0f + 10.0f) / 255.0f);  // ((cut+10)/255), clamped <= 1
    }

    /** The original's parabolic cutoff-dependent resonance compensation:
        feedback gain = ((c-0.7)^2 * 7 + 0.3) * (Resonance/255) -- our
        resonance is already the normalised 0..1 = Reso/255. (ghidra Pass 2) */
    static float ladderFeedbackGain (float coeff, float resonance) noexcept
    {
        const float d = coeff - 0.7f;
        return (d * d * 7.0f + 0.3f) * std::clamp (resonance, 0.0f, 1.0f);
    }

    float processSample (float x) noexcept
    {
        if (ch.enabled && ch.tweet)
            // Pre-filter distortion (tanh(8x)/tanh(8), normalised near unity),
            // RESONANCE-SCALED (ground truth, user ear test 2026-07-13: the whole
            // character block is inert at resonance=0). Drive constant raised
            // 4->8 (user ear test 2026-07-14: tweet was the weakest of the four,
            // barely audible) for a harder pre-filter edge.  [TBD BY MEASUREMENT]
            x = (1.0f - resoAmt) * x + resoAmt * (std::tanh (8.0f * x) / std::tanh (8.0f));

        if (type == Type::scream)                 // regenerative feedback
        {
            // "Scream" screams at 85%+ resonance (user spec): below 0.85 it's
            // strong-but-controlled regeneration; from 0.85->1.0 a screamAmt
            // ramp pushes the regenerative gain past unity so the filter
            // SELF-OSCILLATES -- a sustained screaming tone -- bounded by the
            // tanh saturation rather than running away. The knob's top 15% is
            // the "into the red" zone.
            const float screamAmt = std::clamp ((resoAmt - 0.85f) / 0.15f, 0.0f, 1.0f);
            x += (0.4f + 1.3f * screamAmt) * resoAmt * std::tanh (1.5f * bp1);
        }

        for (int i = 0; i < osFactor; ++i)        // 1 = vintage single-rate, 2 = modern
            tick (x);

        float out = lp2;

        // v1.0.19 test types: each takes its own output tap (12 dB cores for
        // ms20/wasp, 4x one-pole for ladder). Old types fall through
        // untouched -- these branches are false for classic/liquid/scream/
        // plane, keeping the v1.0.18 arithmetic bit-exact.
        // v1.0.20 LOUDNESS PARITY (user report: "classic and ladder 8-10 dB
        // lower than wasp etc" in 1017 mode): voice-level RMS measurement
        // (parity_probe2, 6 configs, char on+off, accent) showed ladder
        // genuinely -0.8..-3.3 dB vs classic (mean -2.1 dB -- the 4x
        // one-pole cascade closes earlier than the resonant cores), so it
        // gets a measured +2.1 dB makeup here. (The other half of the
        // user's gap was WASP being +7.8..+12 dB hot with character ON --
        // fixed in the tweak ring tap below, not with gain.)
        if (type == Type::ladder)      out = kLadderMakeup * lad4;
        else if (type == Type::ms20)   out = lp1;
        else if (type == Type::wasp)   out = waspOut;

        if (type == Type::liquid)                 // TB-303-flavoured ~18 dB/oct
            // Blend the 2-pole (lp1, 12 dB) and 4-pole (lp2, 24 dB) taps so the
            // rolloff sits ~18 dB/oct -- gentler and rounder than classic's full
            // 24 dB, the 303's "mismatched first pole" character. Unity in the
            // passband (lp1==lp2==x there). Mix TBD by ear (blend fraction sets
            // exactly where between 12 and 24 dB the slope lands).
            out = 0.28f * lp1 + 0.72f * lp2;

        if (type == Type::plane)                  // EMU-style morph LP -> comb
        {
            // v1.0.15 bipolar comb (see setParams' plane comment): c in [-1,1]
            // from the resonance knob (0% -> -1, 50% -> 0, 100% -> +1).
            //   out = (1 - |c|/2)*lp + (c/2)*ap
            // algebraically equals lp at c=0 (plain LP), 0.5*(lp+ap) at c=+1
            // (positive comb) and 0.5*(lp-ap) at c=-1 (negative comb, the
            // complementary notch set). Cutoff (env-swept) moves apC[] -> the
            // notches sweep (Z-plane feel) in either polarity.
            float ap = lp2;
            for (int k = 0; k < 4; ++k)
            {
                const float y = apC[k] * ap + apZ[k];
                apZ[k] = ap - apC[k] * y;
                ap = y;
            }
            const float c = (resoAmt - 0.5f) * 2.0f;
            out = (1.0f - 0.5f * std::abs (c)) * lp2 + 0.5f * c * ap;
        }

        // tweak, the audible half (pass 3): "raises the high pitch in the
        // resonance part" -- blend the BRIGHT part of the resonance ring
        // (bp minus its low-passed self) straight into the output, scaled
        // by the resonance knob. Deterministic, unmissable at high reso,
        // and still ~silent at reso~0 (osc-config-v3.2 discrimination
        // test: no ring, nothing to brighten). Gain raised 1.2->2.2 (user ear
        // test 2026-07-14: tweak's effect was barely noticeable).
        //                                        [AMOUNT TBD BY MEASUREMENT]
        if (ch.enabled && ch.tweak)
        {
            // Ladder has no bp state -- its ring lives in the stage-4 tap
            // that also drives the resonance feedback (tickLadder feeds
            // damp/dampLo from it too; deliberately UNSCALED by
            // kLadderMakeup -- scaling both the output tap and this ring
            // measured +2.4..+3.0 dB hot with character on, double-dipping
            // the makeup). WASP must use the SHAPED bp tap (v1.0.20, user
            // loudness report): its output is CMOS-compressed to ~+-0.5
            // ceilings while raw bp1 is unshaped and several times hotter,
            // so ringing tweak on raw bp1 blew wasp up to +7.8..+12 dB over
            // the cohort with character on (measured, parity_probe2).
            // Classic/liquid/scream/plane keep bp1 verbatim -- bit-exact
            // v1.0.18 arithmetic.
            const float ring = type == Type::wasp ? waspBpS
                             : type == Type::ladder ? lad4
                             :                        bp1;
            out += 2.2f * resoAmt * (ring - dampLo);
        }

        // squelch, the audible half (pass 4, mirroring tweak's fix): the
        // feedback damping above is real but perceptually marginal in
        // phrases (the ring it shortens is already transient). Darken the
        // OUTPUT directly: subtract the output's own high-passed content
        // above a cutoff-tracking split. Subtracting out's OWN HF is
        // phase-coherent by construction (subtracting bp-derived HF would
        // blend a phase-shifted ring copy and could brighten instead).
        // Depth is fully RESONANCE-SCALED (was a 0.35 floor): user ear test on
        // the original (2026-07-13) found squelch -- like the whole character
        // block -- does nothing at resonance=0, only acting as resonance rises.
        // The one-pole state updates every sample regardless of the knob so
        // engaging squelch never clicks.        [AMOUNT TBD BY MEASUREMENT]
        outLp += outLpCoeff * (out - outLp);
        if (ch.enabled)
            out -= ch.squelch * resoAmt * (out - outLp);

        return out;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    void tick (float in) noexcept
    {
        // v1.0.19 test types dispatch FIRST so the body below (classic/
        // liquid/scream/plane) executes exactly the v1.0.18 instruction
        // stream for the old types -- an enum compare is not arithmetic.
        if (type == Type::ladder) { tickLadder (in); return; }
        if (type == Type::ms20)   { tickMs20 (in);   return; }
        if (type == Type::wasp)   { tickWasp (in);   return; }

        // ---- stage 1: resonant, with the modification model ---------------
        // The q*fb term is what DAMPS the resonator, so shaping fb's spectrum
        // shapes the resonance spectrum:
        //   squelch : ADD the high-passed part of bp to fb -> extra damping
        //             of high-frequency ringing -> darker, hollow resonance.
        //   tweak/tweet's brightening now lives ENTIRELY in the direct Q
        //             cut in setParams() (deterministic, resonance-aware
        //             for tweak) -- NOT here. An earlier version also
        //             subtracted hpPart for tweak/tweet in this fb term,
        //             but that's resonance-INDEPENDENT (fires even at
        //             reso~0), which fought the Q-cut's "tweak does
        //             nearly nothing at low resonance" requirement
        //             (osc-config-v3.2 discrimination test) and measured
        //             an 80% RMS change at reso=0.02 -- clearly audible
        //             when it should be silent. Squelch only, here.
        damp   += dampCoeff   * (bp1 - damp);         // 3f split: scream only
        dampLo += dampLoCoeff * (bp1 - dampLo);       // 1.2f split: squelch/tweak
        const float hpPart = bp1 - damp;              // HP of bp above 3f
        const float hpLo   = bp1 - dampLo;            // HP of bp above 1.2f

        float fb = bp1;
        if (ch.enabled)
        {
            // squelch ADDS the bright part of the ring back into fb ->
            // more q*fb damping torque exactly where the ring lives ->
            // darker, choked resonance. Gain 6.0 (pass 4, was 2.4): with
            // the drive clip compressing the loop, squelch needs more
            // torque to stay audible at mid drive settings; the gesture
            // test asserts it. RESONANCE-SCALED (* resoAmt) per the user ear
            // test -- inert at reso=0.           [TBD BY MEASUREMENT]
            fb += 6.0f * ch.squelch * resoAmt * hpLo;

            // Drive (pass 5): OVERDRIVE IN THE RESONANCE FEEDBACK path -- the
            // manual's "an overdrive in the resonance part of the filter that
            // will generate plastic like acid sounds". Soft-saturate the
            // feedback with a drive-scaled pre-gain and UNITY origin slope
            // (tanh(g x)/g): quiet resonance is unchanged (origin slope 1), but
            // as the ring grows the feedback saturates -> its ceiling drops to
            // 1/g -> the q*fb damping term saturates -> effective damping falls
            // at signal peaks -> the resonance is driven HOTTER and grittier and
            // self-limits at the saturator ceiling. The saturator is itself the
            // amplitude limiter, so the resonance can reach the manual's
            // self-oscillation ("screaming acid") without the safety net having
            // to rectify. This REPLACES the pass-4 clip of the resonator STATE,
            // which only ever ATTENUATED (small-signal slope sech^2(bias) < 1,
            // full-drive ceiling ~0.089): measured as drive REDUCING every band
            // -- perceptually "no effect / duller", the opposite of an overdrive.
            // Curve/endpoint TBD BY MEASUREMENT (capture C5); the direction is
            // manual-dictated.
            // RESONANCE-SCALED (user ear test: character inert at reso=0):
            // blend the driven feedback in by resoAmt, so drive does nothing
            // at reso=0 and is full overdrive at reso=1.
            if (ch.drive > 0.001f)
                fb += resoAmt * (std::tanh (driveGain * fb) / driveGain - fb);
        }

        if (type == Type::liquid)                 // gentle-but-audible squash
            fb = std::tanh (fb * 1.4f) * (1.0f / 1.4f);
        else if (type == Type::scream)            // less HF damping = wilder
            fb -= 0.3f * hpPart;

        lp1 += f * bp1;
        const float hp1 = in - lp1 - q * fb;
        bp1 += f * hp1;
        bp1 = safety (bp1);

        // ---- stage 2: light damping (stability-capped), adds 12 dB ---------
        lp2 += f * bp2;
        const float hp2 = lp1 - lp2 - damp2 * bp2;
        bp2 += f * hp2;
        bp2 = safety (bp2);
    }

    //==========================================================================
    // v1.0.19 test-type cores. Each carries its OWN copy of the character
    // fb-shaping block (squelch torque + drive saturator) rather than
    // factoring tick()'s into a helper: tick()'s classic body must stay
    // byte-for-byte untouched (variant-A bit-exactness standard), so the
    // canonical copy lives there and these mirror it.

    /** LADDER -- the original's Filter-Modification-OFF path (ghidra Pass 2):
        coefficient smoothed (x+t)/2 per sample toward ladTarget; resonance
        feedback ((c-0.7)^2*7 + 0.3)*reso (parabolic cutoff-dependent
        compensation); feedback-summed input HARD-clipped at +-1.0 (the
        original clipped at +-32768 in its 16-bit int domain; full scale
        maps to 1.0 in our float convention); then 4 IDENTICAL one-pole
        stages (y += c*(x - y) == the k = 2f-1 mapped form). The clip before
        the stages makes the cascade unconditionally bounded (each one-pole
        is a convex average). NB (x+t)/2 runs per tick() call: per sample at
        Vintage single-rate -- the original's granularity -- and 2x as fast
        in Modern's oversampled loop.
        AUTHENTICITY DEVIATION (deliberate): the original's Mod-OFF path has
        NO character block at all; drive/squelch/tweet/tweak apply here the
        same way they do for classic so the GUI's character section works
        uniformly across every type (v1.0.19 spec). */
    void tickLadder (float in) noexcept
    {
        ladC = 0.5f * (ladC + ladTarget);              // (x+t)/2 per-sample smoothing

        // Shared character trackers, fed by the ladder's resonance tap
        // (stage 4) in place of classic's bp1.
        damp   += dampCoeff   * (lad4 - damp);
        dampLo += dampLoCoeff * (lad4 - dampLo);
        const float hpLo = lad4 - dampLo;

        float fb = lad4;
        if (ch.enabled)
        {
            fb += 6.0f * ch.squelch * resoAmt * hpLo;
            if (ch.drive > 0.001f)
                fb += resoAmt * (std::tanh (driveGain * fb) / driveGain - fb);
        }

        const float u = std::clamp (in - ladderFeedbackGain (ladC, resoAmt) * fb,
                                    -1.0f, 1.0f);      // original: +-32768 hard clip
        lad1 += ladC * (u    - lad1);
        lad2 += ladC * (lad1 - lad2);
        lad3 += ladC * (lad2 - lad3);
        lad4 += ladC * (lad3 - lad4);
    }

    /** MS20 -- Sallen-Key flavour on the SVF stage-1 machinery: 12 dB/oct
        2-pole core whose resonance FEEDBACK passes a hard-ish asymmetric
        diode clipper. Quiet resonance is transparent (unity origin slope);
        as the ring grows the clipped feedback saturates the damping torque
        q*fb, so resonance screeches/distorts as it rises instead of staying
        clean, and at max resonance the near-undamped loop self-oscillates --
        BOUNDED by the clipper ceiling + the house safety() net.
        [CURVE TBD BY MEASUREMENT -- flavour model, not a circuit sim] */
    void tickMs20 (float in) noexcept
    {
        damp   += dampCoeff   * (bp1 - damp);
        dampLo += dampLoCoeff * (bp1 - dampLo);
        const float hpLo = bp1 - dampLo;

        float fb = diode (bp1);                        // clipper INSIDE the reso path
        if (ch.enabled)
        {
            fb += 6.0f * ch.squelch * resoAmt * hpLo;
            if (ch.drive > 0.001f)
                fb += resoAmt * (std::tanh (driveGain * fb) / driveGain - fb);
        }

        lp1 += f * bp1;
        lp1 = safety (lp1);
        const float hp1 = in - lp1 - q * fb;
        bp1 += f * hp1;
        bp1 = safety (bp1);
    }

    /** WASP -- EDP Wasp CMOS flavour on the SVF stage-1 machinery: the
        integrator STATES integrate cleanly, but every USE of an integrator
        output (the lp/bp taps, the resonance feedback, the output) passes
        the asymmetric CMOS-inverter soft-clip once -- gentle even-harmonic
        dirt at ANY level, noisy/dirty character at any resonance. Shaping
        the taps (not the states) avoids a per-tick recompression fixed
        point; states stay safety()-bounded, the shaped output is bounded
        by the inverter ceilings by construction.
        [CURVE TBD BY MEASUREMENT -- flavour model, not a circuit sim] */
    void tickWasp (float in) noexcept
    {
        const float bpS = waspShape (bp1);
        waspBpS = bpS;      // shaped ring tap for tweak (see processSample --
                            // raw bp1 sits far above the shaped output scale)

        damp   += dampCoeff   * (bpS - damp);
        dampLo += dampLoCoeff * (bpS - dampLo);
        const float hpLo = bpS - dampLo;

        float fb = bpS;                                // dirt in the reso path too
        if (ch.enabled)
        {
            fb += 6.0f * ch.squelch * resoAmt * hpLo;
            if (ch.drive > 0.001f)
                fb += resoAmt * (std::tanh (driveGain * fb) / driveGain - fb);
        }

        lp1 += f * bpS;
        lp1 = safety (lp1);
        const float lpS = waspShape (lp1);
        const float hp1 = in - lpS - q * fb;
        bp1 += f * hp1;
        bp1 = safety (bp1);
        waspOut = lpS;                                 // processSample's wasp tap
    }

    /** MS20 diode pair: unity slope at the origin (transparent quiet ring),
        asymmetric conduction knees -- the positive side clips later than
        the negative (ceilings ~+0.45 / ~-0.28), so a hot ring lands hard,
        asymmetric, even-harmonic-rich. */
    static float diode (float v) noexcept
    {
        return v >= 0.0f ? std::tanh (2.2f * v) * (1.0f / 2.2f)
                         : std::tanh (3.6f * v) * (1.0f / 3.6f);
    }

    /** WASP CMOS inverter curve: a BIASED tanh renormalised to unity slope
        at the origin -- y = (tanh(1.7*(x+0.15)) - tanh(0.255)) / (1.7 *
        sech^2(0.255)). The bias puts x^2 curvature at the operating point
        (even harmonics even at low levels); ceilings are asymmetric
        (~+0.47 / ~-0.78 before the 1/0.6 sensitivity scale). */
    static float waspShape (float v) noexcept
    {
        constexpr float kSense = 0.6f;                 // drive into the inverter
        return (std::tanh (1.7f * (kSense * v + 0.15f)) - 0.24967879f)
               * (0.62734515f / kSense);               // 1/(1.7*sech^2(0.255)) / kSense
    }

    /** Transparent below |4|, softly limits beyond -- pure stability net for
        the linear (character-off) path; effectively never engaged in
        normal operation. */
    static float safety (float v) noexcept
    {
        const float a = std::abs (v);
        return a <= 4.0f ? v : std::copysign (4.0f + std::tanh (a - 4.0f), v);
    }

    double baseFs = 48000.0;
    int    osFactor = 2;          // 1 = vintage single-rate, 2 = modern oversampled
    double fs2 = 96000.0;
    float f = 0.1f, q = 1.0f, damp2 = 1.3f, dampCoeff = 0.25f, dampLoCoeff = 0.1f,
          outLpCoeff = 0.2f;
    float lp1 = 0, bp1 = 0, lp2 = 0, bp2 = 0;
    float damp = 0, dampLo = 0, outLp = 0, driveGain = 1.0f, resoAmt = 0.0f;
    float apZ[4] = { 0, 0, 0, 0 };   // plane comb: one-multiply allpass state
    float apC[4] = { 0, 0, 0, 0 };   // plane comb: allpass coefficients
    // v1.0.20 measured makeup (parity_probe2 voice-level RMS, mean of 6
    // configs: ladder -2.06 dB vs classic) -- see processSample's parity
    // comment for the basis and the [parity] test for the regression lock.
    static constexpr float kLadderMakeup = 1.27f;   // +2.1 dB

    float lad1 = 0, lad2 = 0, lad3 = 0, lad4 = 0;   // ladder: 4 one-pole stages
    float ladC = 0, ladTarget = 0.5f;               // ladder: smoothed coeff + target
    float waspOut = 0;                              // wasp: shaped output tap
    float waspBpS = 0;                              // wasp: shaped bp (tweak ring tap)
    Type  type = Type::classic;
    Character ch;
};


} // namespace dreamer
