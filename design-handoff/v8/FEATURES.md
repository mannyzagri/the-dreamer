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
- **Panel**: the vector state renders as the **SIGNAL FLOW** display —
  a network graph: tone nodes A-D on the left, a Σ MIX node on the
  right; each edge's thickness/brightness = that tone's live gain, and
  red signal dots flow along active edges with speed proportional to
  gain (silent tones: edge fades out, dots stop). A red phase bar along
  the bottom shows the vector phase position. Display only; editing
  happens via the per-tone DIR/INT knobs. A MASTER volume knob sits in
  the header strip (post-FX output gain, the final stage before the DAC
  — also the plugin's mapped output-gain parameter).
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

---

## 11. FX v1.5 — expanded effects & the PARAMS model  [V1.5]

Extends §8. The FX bus gains modules and a generic per-slot **PARAMS**
knob so deep parameters are reachable without panel bloat.

### 11.1 The PARAMS knob model (panel + control contract)
Each FX slot (MOD / DELAY / REVERB, and any future slot) exposes, next
to its MIX knob, a single **PARAMS** knob whose target is the currently
*focused* secondary parameter of that slot's selected algorithm. The
focused parameter is chosen by a small stepper/LCD on the slot (the same
yellow-on-black idiom): e.g. MOD=ENSEMBLE shows PARAMS focus list
{SPREAD, PHASE, TONE}; DELAY=TAPE shows {WOW, FLUTTER, SAT, HFLOSS};
REVERB=HALL shows {PREDELAY, DIFFUSION, MODRATE}. The primary knobs
(RATE/DEPTH/MIX, TIME/FB/MIX, SIZE/DAMP/MIX) stay dedicated; PARAMS
covers the algorithm-specific rest.

DSP contract: each effect algorithm publishes an ordered list of
"extra" params (id, label, range, default). The panel PARAMS focus
stepper indexes that list; the PARAMS knob writes the focused one. All
extra params are still individually host-automatable (exposed in APVTS
as fx{slot}_p0..pN); the knob is a convenience view, not the only
access. This keeps automation full-resolution while the panel stays
compact. PARAMS + its focus are also mod-matrix destinations
(dest: FX PARAM, targets the focused param of a chosen slot).

### 11.2 New MOD FX modes (added to CHORUS/FLANGER/PHASER/ENSEMBLE)
- **ENSEMBLE** [now first-class]: 3-phase BBD chorus, LFOs at 120°.
  extras: SPREAD (stereo), PHASE (LFO offset spread), TONE (BBD HF loss).
  The string-machine sound; primary voice for pad/string loops.
- **DIMENSION**: fixed 4-mode stereo chorus (Dim-D style), no rate/depth
  — MODE selects 1..4. Subtle width/shimmer. extras: MODE, TONE.
- **ROTARY**: Leslie — horn+drum rotors, doppler+amplitude, crossover,
  spin-up/down ramp. extras: SPEED (slow/fast toggle via param), ACCEL,
  BALANCE (horn/drum), MIC WIDTH. Medium cost, distinct from choruses.
- **BARBERPOLE**: Shepard phaser, infinite up or down sweep. extras:
  DIR, STAGES, FEEDBACK. Drone specialist.

### 11.3 New standalone stages
- **LO-FI / DEGRADE**  [thematically core]: variable bit depth (below
  the native 12 down to ~6), sample-rate reduction (decimate), optional
  µ-law companding, output re-quantized. extras (on its PARAMS): BITS,
  SRATE, COMPAND, ANTI-ALIAS(on/off — off = authentic ugly). Placed as
  its own slot; see routing switch 11.4.
- **WIDTH / IMAGE**: mid/side gain + optional Haas; makes the mono voice
  bus explicitly stereo before/independent of delay/reverb. extras:
  WIDTH, HAAS, BASS-MONO (keep lows centered).
- **TALK / VOWEL** (FX-bus formant, distinct from the FORMANT filter
  type): envelope- or LFO-driven vowel filter. extras: VOWEL-A, VOWEL-B,
  MORPH, SENS. Reinforces the vox identity on the FX bus.

### 11.4 Routing
- New **FX PRE/POST filter** switch: the LO-FI (and optionally WIDTH)
  stage can sit before or after the global FILTERS block. Pre-filter
  crush into a resonant sweep is a distinct, valuable sound. One bit of
  state, large palette gain.
- Chain order (default): [pre-slot: LO-FI/WIDTH if PRE] -> FILTERS ->
  MOD FX -> DELAY -> REVERB -> [post: LO-FI/WIDTH if POST] -> MASTER.
- Pong delay + reverb remain the guaranteed mono->stereo points; WIDTH
  can promote earlier if enabled.

### 11.5 Deferred (v2 / "modern mode", clearly labelled)
Shimmer reverb (pitch-shift feedback), convolution reverb, multiband
dynamics. Non-period; include only behind an explicit MODERN flag if at
all. Not part of v1.5.

### 11.6 Cost & authenticity
All 11.2-11.4 modules are delay-line + biquad + gain math: block-rate,
Pentium-100-legal. ROTARY is the heaviest (two rotor sims) and still
trivial at one instance on the FX bus. LO-FI is nearly free and is the
most on-theme effect in the whole plan.

### 11.7 Panel implication (see GUI update)
FX section moves from 3 fixed rows to rows with: TYPE stepper · primary
knobs (3) · **PARAMS focus stepper + PARAMS knob** · on/LED. A 4th
"UTILITY" row (LO-FI | WIDTH | TALK, each on/off with its PARAMS) and
the PRE/POST switch. If vertical space is tight, the utility stages
collapse into an FX-page overlay LCD reachable from an FX EDIT button.
