# DSP_BUILD.md — The Dreamer: engine build spec (VM Claude Code)

Read with CLAUDE.md (rules/phases 1-6), ENVIRONMENT.md (VM), FEATURES.md
(synthesis spec). This file turns the decisions made since those docs
into concrete build tasks and the canonical parameter list. Where this
file and FEATURES.md conflict, THIS FILE WINS (it is newer).

The GUI is specified separately in GUI_SPEC.md; the parameter table in
§9 below is the canonical contract between DSP and GUI. Do not rename,
renumber, or re-range parameters without flagging it.

---

## 1. Bank v3 — three entry types

Replaces the v2 CYCLE-only bank. New entry struct:

    enum class WaveType : uint8_t { Cycle, Loop, OneShot };
    struct Waveform {
      const char* category;   // "Pad","String","Vox","Bell","Metal",
                              // "Basic","Ens","Shot"
      const char* name;
      WaveType    type;
      const int16_t* samples; // 12-bit left-justified (low nibble zero)
      uint32_t    length;     // samples. Cycle: always 600
      uint32_t    loopStart;  // Loop: loop point (0 for full-buffer loop,
                              // baked loops are seamless full-buffer).
                              // Cycle/OneShot: 0
      float       rootHz;     // Loop/OneShot: pitch of the baked material
                              // (bake default 220.0). Cycle: unused (600-
                              // sample cycle math as today)
    };

Sources:
- 78 CYCLE entries: unchanged from v2 (bake_bank.py).
- 130 LOOP entries: baked by bake_library.py from survey-selected cycles
  (feature DB akwf_feats.json). Families: PAD/AIRY/VOX/ETHER/FM/WIND/METAL
  + MORPH specials. 12-bit mono 44.1k, seamless, ~2.6-3.2s. All single-note
  (voicing is a runtime DSP feature, NOT baked — see §11). Corrections:
  octave-normalized morph partners, pitch-locked (tonal loops), 2x
  oversampled render -> 44.1k. Total ~21 MB (size budget lifted; <25 MB ok).
  Pack the WAV data into the same constexpr-int16 header pattern
  (extend bake_loops.py with a --emit-header mode, mirroring
  bake_bank.py; keep low-nibble-zero invariant).
- ONESHOT entries: NEW bake script bake_shots.py, ~10 short transients
  (chiff, breath burst, click, mallet tick, noise swell), 30-200 ms,
  synthesized (filtered noise bursts + short cycle fragments), 12-bit.
  Category "Shot".

Total embedded size target: < 5 MB. All bank data remains read-only,
low-nibble-zero asserted in tests.

### 1b. LOOP TUNING — per-loop rootHz (REQUIRED, fixes detune)  [NEW]

AKWF single cycles are 600 samples; at 44.1k a raw cycle is 73.5 Hz —
NOT a musical note (the AKWF author notes cycles must be tuned to a
reference pitch). Our loops were baked at nominal 220 Hz, but pitch-lock
warping + the tail/head crossfade leave each loop's TRUE pitch off by
-36..+45 cents from 220. If the engine assumes rootHz=220 for all loops,
every loop plays slightly out of tune (worst on airy/ethereal loops with
weak fundamentals).

FIX (multisample-correct): each LOOP entry stores its MEASURED rootHz.
The engine computes playback ratio = noteHz / loop.rootHz (NOT / 220).
- loop_roots.json (shipped with the library) maps loop name -> measured
  rootHz. Fold these values into each Loop bank entry's `rootHz` field at
  header-emit time.
- Inharmonic loops (METAL/CHROME/SPECTRAL/ORACLE/VOICE_OF_STEEL/…) have no
  meaningful pitch; rootHz stays 220.0 nominal (they're used as texture,
  not played melodically).
- This is exactly the "tune samples to a reference note" step the AKWF
  README describes, done per-loop and stored, so no runtime cost.

Alternative (NOT preferred): re-bake each loop with a final resample so
true pitch == exactly 220, letting the engine keep a single root. Simpler
engine but adds one resample generation of requality. Prefer stored rootHz.

Validation: play each tonal loop as its root MIDI note; measured output
pitch must be within +-3 cents of the target note frequency.

## 2. PcmOscillator v3 — loop & one-shot playback

Extend (do not rewrite) the v2 oscillator:

- Cycle: exactly as v2 (600-wrap, phase increment = f*600/fs).
- Loop: phase walks 0..length, wraps to loopStart. Pitch ratio =
  noteHz / loop.rootHz  (per-loop measured root; see §1b — NOT a fixed
  220 assumption). Single root stretched across keyboard — chipmunk
  shift is INTENTIONAL, no zone mapping in v1). Fixed-point phase:
  widen accumulator to 64-bit (32.32) to cover long buffers; keep the
  integer linear interp form.
- OneShot: same as Loop but no wrap; `finished()` goes true at end,
  tone output is silence after (TVA may still be in release).
- Interp modes unchanged (Linear default, DropSample option). Still no
  band-limiting, no mip levels.
- START param semantics: Cycle = phase offset 0..599 (as today);
  Loop/OneShot = start offset as fraction of length (0..1 of buffer).
  START RANDOM mode (per-tone flag): note-on start position randomized
  (Cycle: 0..599; Loop: 0..1) — kills machine-gun identity.

## 3. Per-voice humanization

- Per-voice slow pitch drift: S&H random walk per voice, rate ~0.2 Hz,
  depth param DRIFT (global, 0..1 -> 0..±3 cents), applied to all tones
  of the voice equally (coherent, like one oscillator board warming up).
  One-pole slewed. Control rate.

## 4. Per-tone NOISE source

- White noise generator, 12-bit quantized output, mixed into the tone
  signal PRE-shaper/PRE-TVF. Per-tone params: NOISE level (0..1),
  NOISE COLOR (one-pole LP amount, 0 = white .. 1 = dark).
  AUX env with dest=NOISE modulates the noise level (add NOISE to the
  AUX/matrix destination lists).

## 5. Tone signal chain (final order)

  PcmOscillator (+START/RANDOM) -> + Noise -> Waveshaper (SHAPE table,
  SHP DPT) -> re-quantize 12-bit -> Tone TVF (LP24/LP12/BP/HP + ADSR,
  keyfollow) -> TVA (ADSR, velocity) -> vector gain -> pan -> tone sum

## 6. Vector engine (per FEATURES.md §4, plus decided additions)

- Per-tone DIR (0..1 = 0..360°), INT (0..1). Gain law:
  g = max(0,cos(phi-dir))^2 ; gain = (1-int) + int*g. Control rate,
  one-pole smoothed.
- Phase sources summed & wrapped: PHASE knob, ORBIT LFO, P-ENV, matrix.
- ORBIT SHAPE selectable: SAW (continuous rotation, default) / TRI /
  SIN / SQR / S+H (slewed random jumps). Shaping applied to the raw
  phase ramp. ORBIT RATE 0.02-8 Hz. Global-phase default; PER-VOICE
  free-run flag (non-panel param, host-exposed).
- P-ENV: startAngle, endAngle, time, loop flag.

## 7. Global section

- FILTERS 1/2 on tone-sum bus, SER/PAR. v1 types: LP24/LP12/BP/HP +
  Rhino LIQUID/CLASSIC/LADDER (ported, FilterSlot adapters).
  V1.1: NOTCH, COMB+/-, NL3 N+LP, FORMANT, ALLPASS. V2: DREAMPLANE
  (MORPH knob = Filter 2 knob 3 when selected).
- FX bus (all Rhino ports): MODFX (CHORUS/FLANGER/PHASER + NEW
  ENSEMBLE mode: if the Rhino chorus is <3 voices, implement ensemble
  as 3-tap modulated delay w/ spread LFO phases, 12-bit-era friendly)
  -> DELAY (DIGITAL/TAPE/PONG - all three Rhino modes, TAPE with its
  full character path) -> REVERB (ROOM/HALL/PLATE). Pong/reverb are
  the mono->stereo points.
- MASTER: post-FX output gain, smoothed, the plugin's mapped output
  parameter. Default 0.78 (-2 dB-ish).

## 8. Build order (continues CLAUDE.md phase numbering)

  Phase 11 — Bank v3: entry struct, bake_loops.py --emit-header,
             bake_shots.py, osc Loop/OneShot + 32.32 phase, START
             RANDOM. Tests: loop-wrap continuity (no click at wrap:
             max sample delta across wrap < quantization step*2),
             pitch-ratio accuracy, one-shot termination, low-nibble
             invariant across all types.
  Phase 12 — Noise source + humanization drift + chain-order refactor
             per §5. Test: noise level/color statistics; drift bounded.
  Phase 13 — Vector engine w/ ORBIT SHAPE + P-ENV + smoothing.
             Test: compass-config equivalence (DIRs at 0/.25/.5/.75,
             INT=1 reproduces 4-corner VS gains within 1e-3).
  Phase 14 — ENSEMBLE mode + MASTER + parameter plumbing per §9.
  (Phases 7-10 from FEATURES.md renumber where already done; keep git
  history as source of truth, this ordering is advisory.)

## 9. Canonical parameter table (contract with GUI_SPEC.md)

IDs are APVTS parameter IDs. [T] = per-tone (4 instances, suffix _a.._d).

  master            0..1
  vec_phase         0..1        vec_orbit_on      bool
  vec_orbit_rate    0..1(->Hz)  vec_orbit_shape   0..4 SAW/TRI/SIN/SQR/SH
  vec_penv_on       bool        vec_penv_start    0..1
  vec_penv_end      0..1        vec_penv_time     0..1
  flt_route         0/1 SER/PAR
  flt1_type 0..N    flt1_cut 0..1  flt1_res 0..1  flt1_env 0..1
  flt2_type 0..N    flt2_cut 0..1  flt2_res 0..1  flt2_morph 0..1
  lfo_rate 0..1     lfo_shape 0..4
  mtx{1,2,3}_src 0..4  mtx{1,2,3}_dst 0..7  mtx{1,2,3}_amt -1..1
  modfx_type 0..6(CHO/FLG/PHA/ENS/DIM/ROT/BARB) modfx_rate/depth/mix 0..1
  modfx_on   modfx_pfocus 0..N  modfx_p0..pN (algo extras, host-automatable)
  dly_mode 0..2(DIG/TAPE/PONG)  dly_time/fb/mix 0..1  dly_on
  dly_pfocus 0..N  dly_p0..pN
  rev_type 0..2(ROOM/HALL/PLATE) rev_size/damp/mix 0..1 rev_on
  rev_pfocus 0..N  rev_p0..pN
  lofi_on  lofi_bits/srate/compand 0..1  lofi_alias bool  lofi_pfocus  lofi_p*
  width_on width/haas 0..1  width_bassmono bool
  talk_on  talk_va/vb/morph/sens 0..1
  fx_prepost 0/1 (LO-FI/WIDTH before vs after global filters)
  drift             0..1 (global humanize depth)
  [T] wave 0..(numWaves-1)   [T] on bool      [T] level 0..1
  [T] oct -2..+2             [T] fine -50..+50c
  [T] start 0..1             [T] start_random bool
  [T] voicing 0..3 (SINGLE/OCT/POWER/DREAMY)  [T] dreamy_spread 0..2
  [T] loop_mode 0..1 (FWD/PINGPONG)
  [T] hit_play 0..1 (NORMAL/STRETCH)  [T] hit_stretch 0..1
  [T] hit_pitchtrim -24..+24
  [T] velo 0..1              [T] pan -1..1
  [T] shape 0..5             [T] shape_depth 0..1
  [T] noise 0..1             [T] noise_color 0..1
  [T] dir 0..1               [T] vint 0..1
  [T] lfo_depth 0..1         [T] lfo_dest 0..3
  [T] aux_dest 0..4 (PITCH/START/SHAPE/PAN/NOISE)
  [T] tvf_type 0..3          [T] tvf_cut/res/env/kf 0..1
  [T] tvf_a/d/s/r 0..1       [T] tva_a/d/s/r 0..1
  [T] aux_a/d/s/r 0..1

Matrix dest enum: PITCH/CUT1/CUT2/MORPH/SHAPE/VECPHS/PAN/NOISE/FXPARAM
(FXPARAM targets the focused PARAMS of a selectable slot).
Matrix src enum: GLFO/VECPHS/AUX/VELO/WHEEL.

## 11. Per-tone VOICING (multi-tap oscillator)  [NEW]

Voicing is generated at PLAYBACK by fanning the tone's oscillator into
multiple read-taps of the SAME single-note loop — nothing is baked.

  voicing param (per tone): SINGLE | OCT | POWER | DREAMY
    SINGLE  -> 1 tap  (root)
    OCT     -> 2 taps (root, +12)
    POWER   -> 3 taps (root, +7, +12)
    DREAMY  -> 4 taps (root, +7, +12, +14)  [add9 default]
  dreamy_spread param: ADD9 (root+7+12+14) | MIN7 (root+3+7+10) |
                       SUS2 (root+2+7+12)   [selectable when DREAMY]

Implementation: each tap is an independent phase accumulator over the
same loop buffer at the interval's pitch ratio; taps are summed with
equal-power gain (1/sqrt(ntaps)) then fed as ONE tone signal into the
tone's shaper/TVF/TVA/vector — i.e. voicing fans only the oscillator,
everything downstream is shared. Intervals are equal-tempered ratios
applied to the tap's phase increment. Cost: worst case 4 taps x 4 tones
x 8 voices = 128 loop reads; trivial (index + lerp each). Taps inherit
the tone's START/RANDOM, drift, and loop mode.

## 12. Per-tone LOOP MODE  [NEW]

  loop_mode param (per tone): FORWARD | PINGPONG
    FORWARD  -> phase wraps loopStart..length (current behavior)
    PINGPONG -> phase reflects at both ends (increment sign flips),
                traversing the buffer forward then backward.
  Applies to LOOP-type waveforms only (ignored for CYCLE/ONESHOT).
  On morph loops, PINGPONG makes the baked morph sweep back and forth
  rather than hard-cut at the loop seam. Per-tap for voicing (all taps
  share the tone's loop_mode).

## 13. HIT STRETCH (one-shot varispeed)  [NEW]

  Per-tone hit_play selects NORMAL (native one-shot) or STRETCH.
  In STRETCH, hit_stretch scales
  playback speed (varispeed): pitch follows speed (authentic sampler
  behavior, NOT pitch-preserved).
    hit_stretch 0..1 -> speed ratio, mapped e.g. 0.25x .. 4x (log),
    center = 1.0 (unmodified). Slower = longer + lower (a CHIFF becomes
    a low wash); faster = shorter + higher (a tick).
  Optional hit_pitchtrim (semitones) to offset the pitch shift if the
  user wants length change with less pitch movement (still varispeed,
  just re-tuned — no time-stretch DSP). hit_stretch is a mod-matrix
  destination.

## 10. Rules reminders

CLAUDE.md rules apply in full: no DSP math changes in ported files,
authenticity over cleanliness, 12-bit invariant, C++17, RT-safe,
null tests immutable, Sonnet 4.6 default w/ escalation policy.
