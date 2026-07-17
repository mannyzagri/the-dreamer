# THE DREAMER — Features Plan

Companion to CLAUDE.md (porting rules & phases) and DESIGN.md (visual).
This document specifies the synthesis features beyond the base 4-tone
ROMpler engine. Everything here obeys the prime directive: *every trick on
this list is something a 1996 engineer could have shipped* — table lookups,
biquads, control-rate math. No FFTs, no granular, no oversampling.

Status legend: [CORE] ships v1 · [V1.1] fast-follow · [V2] own mini-project

---

## 1. Engine recap (baseline, already specified)

4 Tones per voice (JD-990 model), each: PcmOscillator (12-bit ROM bank,
600-sample cycles, linear/drop-sample interp, no band-limiting) → simple
TVF (LP24/LP12/BP/HP + ADSR, key follow) → TVA (ADSR, velocity).
Tone sum → global FILTERS 1/2 (SER/PAR) → FX chain. Poly with oldest-steal.

---

## 2. Per-tone waveshaper — "the 01/W trick"  [CORE]

Korg 01/W-style waveshaping: LUT transfer function on the oscillator
output, before the tone TVF.

- Tables: baked by `bake_shapers.py` (same pattern as the wave bank),
  1024-entry int16, 12-bit-quantized values. Initial set:
  SOFT FOLD, HARD FOLD, SINE FOLD, ASYM, DRIVE (+OFF).
- Signal path: `y = lerp(x, table[drive(x)], depth)` — depth 0..1 is the
  dry/shaped mix; table input index scales with an internal drive so DEPTH
  sweeps feel alive. One table read + 2 lerps per sample per tone.
- SHAPE DEPTH is a mod destination (AUX env, tone LFO, matrix) — this is
  intra-tone timbral motion for near-zero DSP.
- Authenticity: applied to the 12-bit sample stream, output re-quantized
  to 12-bit grain (keep the low-nibble-zero invariant through the shaper).

## 3. Per-tone modulation  [CORE]

Beyond TVF/TVA envelopes, each tone gets:

- **AUX ENV**: one ADSR, one destination per tone:
  PITCH (± range param, default ±2 semi, up to ±24 for bell blips and
  octave drifts) · START (re-strike phase offset — only sampled at
  note-on) · SHAPE (shaper depth) · PAN.
- **TONE LFO DEPTH + DEST**: per-tone depth/destination tap of the single
  GLOBAL LFO (PITCH/CUTOFF/SHAPE/LEVEL). One LFO clock for the whole
  voice; per-tone depths give coherent "breathing" across the stack.
  Deliberately NOT four free-running LFOs (phase mush + panel cost).

## 4. Dream Vector — per-tone directional morphing  [CORE]

Vector synthesis generalized beyond the fixed-corner Prophet VS / SY22
scheme: instead of four tones nailed to compass points with an XY dot,
**each tone owns a position on the vector circle**, and one global phase
sweeps around it.

- **Per-tone params** (in TONE EDIT): **DIR** (angle on the circle,
  0..1 = 0..360°) and **INT** (intensity, 0..1 — how deeply the vector
  motion scans this tone).
- **Gain law** (control rate, per 16-sample block, one-pole smoothed):

      g   = max(0, cos(phi - dir))^2        // power-shaped lobe
      gain = level * ((1 - int) + int * g)

  int=0 → tone is a static drone at its level; int=1 → tone fully
  scanned, silent when phase points away. With DIRs at the four compass
  points and equal INT, classic VS behavior is recovered exactly — the
  scheme is a strict generalization.
- **Musical wins over fixed corners**: paired tones at adjacent angles
  crossfading as a unit; a static bed under three rotating layers;
  asymmetric spacing so the sweep lingers on pads and flicks past bells.
- **Phase (phi) motion sources** (summed, wrapped):
  1. Manual PHASE knob (MIDI-learnable / wheel-mappable).
  2. **ORBIT**: phase LFO, RATE param; global-phase default (ensemble
     coherence), per-voice free-run as option.
  3. **P-ENV**: phase envelope — start angle → end angle over one time
     param, note-on triggered, optional loop. (Simplified from the v3
     3-point XY envelope; strictly easier.)
- **Mod matrix**: VEC X/Y destinations collapse to a single **VEC PHS**
  (source AND destination) — cleaner routing, one wire instead of two.
- **Panel**: the XY pad is replaced by the **TONE CONSTELLATION**
  display — a circle showing each tone as a dot at its DIR angle
  (radius = INT, brightness/size = live gain) with the red phase dot
  sweeping the rim. Display only; editing happens via DIR/INT knobs.
- Cost: 4 cos-lobe evaluations per block. Effectively free.

## 5. Global filter block  [CORE = ports; V1.1 = additions]

Two FilterSlot instances on the voice-sum bus, SER/PAR routing.
Type list (panel order):

| # | Type      | Source                   | Status |
|---|-----------|--------------------------|--------|
| 1 | LP 24     | built-in SVF cascade     | CORE   |
| 2 | LP 12     | built-in SVF             | CORE   |
| 3 | BP        | built-in SVF             | CORE   |
| 4 | HP        | built-in SVF             | CORE   |
| 5 | LIQUID    | Rubber-Rhino port        | CORE   |
| 6 | CLASSIC   | Rubber-Rhino port (18dB) | CORE   |
| 7 | LADDER    | Rubber-Rhino port        | CORE   |
| 8 | NOTCH     | SVF (LP+HP sum)          | V1.1   |
| 9 | COMB +    | feedback comb            | V1.1   |
|10 | COMB −    | feedback comb, inverted  | V1.1   |
|11 | NL3 N+LP  | notch tracking above LP  | V1.1   |
|12 | FORMANT   | 3× parallel BP, vowel
              morph A-E-I-O-U (morph
              uses the MORPH knob)     | V1.1   |
|13 | ALLPASS   | 4-stage AP + feedback
              (phaser-as-filter)       | V1.1   |
|14 | DREAMPLN  | DreamPlane engine (§6)   | V2     |

Rules: Rhino ports are verbatim (CLAUDE.md rule 1) behind FilterSlot
adapters. Simple types share one SVF core. WASP and PLANE from Rhino are
deferred (curated out for v1; trivial to add later since the adapter
pattern is uniform). Tone-level filter stays the simple LP24/LP12/BP/HP
set — the character filters are global by design (CPU: 2 instances vs
4×poly; authenticity: JD tone TVFs were plain; musicality: character
sweeps belong on the summed stack).

## 6. DreamPlane — morphing pole-zero filter  [V2]

Working name (between us): **DreamPlane**. E-MU Z-plane lineage
(Morpheus/UltraProteus/Audity), original implementation, no E-MU data.

- **Structure**: 12th-order = 6 cascaded biquads. A *frame* = 6 sections
  × coefficients. MORPH interpolates between frame pairs (later: 2D frame
  grids) in real time at control rate.
- **Stability**: never interpolate raw b/a coefficients. Frames are
  stored and interpolated in pole/zero polar form per section
  (radius ≤ 0.9995 clamp, angle), converted to biquad coefficients per
  control block. Guarantees stable intermediate states.
- **Frame authoring**: `bake_frames.py` — frames designed from formant
  tables (vowel morphs), comb-like zero fans, resonant tilts. Output:
  constexpr frame arrays, same header pattern as the wave bank. Initial
  frame pairs: VOWEL AE→OO, CHOIR AIR, FLANGE FAN, GLASS TILT.
- **Controls**: MORPH knob (Filter 2 slot when DREAMPLN selected) +
  MORPH as mod-matrix destination (G-LFO and AUX env are the money
  routings). Optional FREQ-TRACK axis later (v2.1) for the second
  Morpheus dimension.
- **Cost**: 6 biquads/sample + control-rate coefficient regen. Fine at
  global-filter position; NOT offered per tone.

## 7. Mod matrix  [CORE]

3 slots (panel), each SRC → DEST with bipolar AMT:

- Sources: G-LFO · VEC X · VEC Y · AUX (selected tone's aux env — or
  voice-max; decide in implementation review) · VELO · WHEEL.
- Destinations: PITCH · CUT 1 · CUT 2 · MORPH · SHAPE · VEC X · VEC Y ·
  PAN.
- Summing: destinations accumulate matrix contributions at control rate;
  VEC X/Y as *both* source and destination is allowed (vector modulating
  filters; LFO modulating vector) but self-routing (VEC X → VEC X) is
  clamped, not feedback.
- Global LFO: TRI/SIN/SAW/SQR/S+H, RATE (with later tempo-sync option),
  single instance, global phase.

## 8. FX chain (Rubber-Rhino port)  [CORE]

Verbatim ports per CLAUDE.md rule 1, post-filter bus, fixed order:
MOD FX (chorus | flanger | phaser, selectable) → DELAY
(digital | tape | pong, selectable — all three Rhino delay modes ported)
→ REVERB (room/hall/plate). Per-block on/off. Panel params: MOD FX
rate/depth/mix; DELAY mode + time/fb/mix; REVERB type + size/damp/mix.
Delay-mode notes: DIGITAL = clean repeats; TAPE = Rhino's wow/saturation/
HF-loss feedback path, ported verbatim including its character stages;
PONG = L/R alternating taps (mono-sum input, stereo out — the delay and
reverb are the points where the mono voice bus becomes stereo). Any Rhino
parameters beyond these get sensible fixed defaults in v1 (documented in
the port notes) rather than panel controls.

## 9. Implementation order (extends CLAUDE.md phases)

Phase 7  — Waveshaper (bake_shapers.py + per-tone LUT stage) + AUX env +
           tone-LFO taps. Small, testable, immediate sonic payoff.
Phase 8  — Dream Vector (mix law, orbit, v-env) + mod matrix + global LFO.
Phase 9  — V1.1 filters (notch, combs, NL3 N+LP, formant, allpass).
Phase 10 — DreamPlane: bake_frames.py, polar-interp engine, MORPH wiring.
Each phase: Sonnet 4.6 executes; null/unit tests per CLAUDE.md; escalate
per the standard policy. Phases 7–8 have no ported-code dependency and can
run parallel to the Rhino filter/FX porting phases.

## 10. Cost budget sanity (per voice, 44.1k)

Tones: 4× (osc ~5 ops + shaper ~4 + SVF ~10 + 3 env ~9) ≈ 110 ops
Vector: ~2 ops/sample amortized · Global: 2 filters (worst: DreamPlane
~40 ops) + FX (block-level). 8 voices ≈ well under 1% of one modern core
before FX. The Pentium-100 would have sweated but shipped it. 
