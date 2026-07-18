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
- 16 LOOP entries: baked by bake_loops.py (recipes committed; loops are
  12-bit mono 44.1k, seamless, ~2.3-3.1 s each, category "Ens").
  Pack the WAV data into the same constexpr-int16 header pattern
  (extend bake_loops.py with a --emit-header mode, mirroring
  bake_bank.py; keep low-nibble-zero invariant).
- ONESHOT entries: NEW bake script bake_shots.py, ~10 short transients
  (chiff, breath burst, click, mallet tick, noise swell), 30-200 ms,
  synthesized (filtered noise bursts + short cycle fragments), 12-bit.
  Category "Shot".

Total embedded size target: < 5 MB. All bank data remains read-only,
low-nibble-zero asserted in tests.

## 2. PcmOscillator v3 — loop & one-shot playback

Extend (do not rewrite) the v2 oscillator:

- Cycle: exactly as v2 (600-wrap, phase increment = f*600/fs).
- Loop: phase walks 0..length, wraps to loopStart. Pitch ratio =
  noteHz / rootHz (single root stretched across keyboard — chipmunk
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
  modfx_type 0..3(CHO/FLG/PHA/ENS) modfx_rate/depth/mix 0..1 modfx_on
  dly_mode 0..2(DIG/TAPE/PONG)     dly_time/fb/mix 0..1     dly_on
  rev_type 0..2(ROOM/HALL/PLATE)   rev_size/damp/mix 0..1   rev_on
  drift             0..1 (global humanize depth)
  [T] wave 0..(numWaves-1)   [T] on bool      [T] level 0..1
  [T] oct -2..+2             [T] fine -50..+50c
  [T] start 0..1             [T] start_random bool
  [T] velo 0..1              [T] pan -1..1
  [T] shape 0..5             [T] shape_depth 0..1
  [T] noise 0..1             [T] noise_color 0..1
  [T] dir 0..1               [T] vint 0..1
  [T] lfo_depth 0..1         [T] lfo_dest 0..3
  [T] aux_dest 0..4 (PITCH/START/SHAPE/PAN/NOISE)
  [T] tvf_type 0..3          [T] tvf_cut/res/env/kf 0..1
  [T] tvf_a/d/s/r 0..1       [T] tva_a/d/s/r 0..1
  [T] aux_a/d/s/r 0..1

Matrix dest enum: PITCH/CUT1/CUT2/MORPH/SHAPE/VECPHS/PAN/NOISE.
Matrix src enum: GLFO/VECPHS/AUX/VELO/WHEEL.

## 10. Rules reminders

CLAUDE.md rules apply in full: no DSP math changes in ported files,
authenticity over cleanliness, 12-bit invariant, C++17, RT-safe,
null tests immutable, Sonnet 4.6 default w/ escalation policy.
