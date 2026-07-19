# GUI_SPEC.md — The Dreamer: panel control spec (GUI Claude / Claude Design)

Read with DESIGN.md (visual language: Supernova-2 blue-purple faceplate,
red-only LEDs binary lit/unlit, yellow-on-black Doto LCDs, lavender
silkscreen, knurled knobs, white slider caps; pointer colors: white =
general, red = filter, yellow = morph/mod/character). Reference
implementation: The_Dreamer_Vector_GUI_v5.html — renovate FROM it, do
not start over. Parameter IDs come from DSP_BUILD.md §9 — every control
below names its ID; do not invent, rename, or drop parameters.

Panel: fixed ratio, 1140x660 logical px, 2x assets.

---

## Header strip
- Wordmark THE DREAMER + subtitle "12-BIT ROM VECTOR SYNTHESIZER".
- Main LCD (520px): patch number/name, selected tone ("TONE A"),
  waveform bar graphic, touched-parameter name+value (0-127 display),
  14-cell value meter. Any control touch updates the readout.
- MASTER knob (id: master), 40px, white pointer, right of LCD.
- POWER LED (decorative, always lit).

## TONES strip (left column)
Four columns A-D, each: label; SELECT btn + red LED (UI state, which
tone TONE EDIT addresses — not a DSP param); tall level slider
(id: level_[t], 150px); ON btn + red LED (id: on_[t]). Disabled tone
column dims to 55%.

## TONE EDIT (center; edits selected tone [t])
Row 1: WAVE LCD (category > name; click opens wave-list overlay,
  inverse-video cursor, ESC exits; also < > steppers) (id: wave_[t]).
  SHAPE LCD + < > (id: shape_[t]; OFF/SOFT FOLD/HARD FOLD/SINE FOLD/
  ASYM/DRIVE).
Row 2 knobs: OCTV (oct_[t]) · FINE (fine_[t]) · START (start_[t]) ·
  LEVEL (mirrors level_[t]) · VELO (velo_[t]) · SHP DPT
  (shape_depth_[t], yellow pointer) · NEW: NOISE (noise_[t]) ·
  NOISE COL (noise_color_[t]) · PAN (pan_[t]).
  NEW: small RND button + LED next to START (start_random_[t]).
Row 3: TVF vertical label (red) + CUT/RES/ENV/KF knobs (red pointers)
  (tvf_cut/res/env/kf_[t]) + tvf type mini-stepper (tvf_type_[t]:
  LP24/LP12/BP/HP — NEW, was missing in v5) · TVF ENV sliders A/D/S/R
  (tvf_a/d/s/r_[t]) · TVA ENV sliders (tva_a/d/s/r_[t]) · AUX ENV
  sliders (aux_a/d/s/r_[t]) + AUX DEST stepper (aux_dest_[t]:
  PITCH/START/SHAPE/PAN/NOISE — note NOISE added).
Row 3b (voicing): VOICING stepper (voicing_[t]: SINGLE/OCT/POWER/DREAMY)
  + DREAMY-spread stepper (dreamy_spread_[t]: ADD9/MIN7/SUS2, shown when
  DREAMY) + LOOP MODE toggle (loop_mode_[t]: FWD/PINGPONG, LED). For
  ONESHOT waves: HIT STRETCH knob (hit_stretch_[t]) + optional PITCH
  TRIM knob (hit_pitchtrim_[t]) appear in place of loop mode.
Row 4: TONE LFO — DEPTH knob (lfo_depth_[t]) + DEST stepper
  (lfo_dest_[t]: PITCH/CUTOFF/SHAPE/LEVEL) · VECTOR — DIR + INT knobs,
  yellow pointers (dir_[t], vint_[t]).

## FILTERS (right column)
Slot 1: < > type LCD (flt1_type; full list incl. LIQUID/CLASSIC/LADDER,
  later NOTCH/COMB+/COMB-/NL3 N+LP/FORMANT/ALLPASS/DREAMPLN) +
  CUT/RES/ENV knobs red (flt1_cut/res/env).
Slot 2: same, third knob is MORPH, yellow (flt2_morph) — label stays
  MORPH for all types (acts as type-specific character where relevant;
  primary for DREAMPLN and FORMANT vowel morph).
Routing: SER/PAR LEDs + toggle (flt_route).

## DREAM VECTOR block
- SIGNAL FLOW display (from v5): tone nodes A-D left, Σ MIX right;
  edge thickness/brightness = live gain; red dots flow along edges,
  speed ∝ gain; red phase bar at bottom = vec_phase. Display only.
- PHASE knob yellow (vec_phase) · RATE knob (vec_orbit_rate).
- NEW: SHAPE stepper (vec_orbit_shape: SAW/TRI/SIN/SQR/S+H).
- ORBIT btn + LED (vec_orbit_on) · P-ENV btn + LED (vec_penv_on).
- NEW: P-ENV mini page (opens on long-press or small EDIT btn):
  START/END angle knobs + TIME knob + LOOP btn
  (vec_penv_start/end/time + loop flag).

## MOD MATRIX block
3 rows: SRC stepper (mtx*_src: G-LFO/VEC PHS/AUX/VELO/WHEEL) > DST
stepper (mtx*_dst: PITCH/CUT1/CUT2/MORPH/SHAPE/VEC PHS/PAN/NOISE —
note NOISE added) + AMT mini-knob yellow, BIPOLAR (center detent;
display -63..+63) + amount bar (bar shows |amt|, colored red when
negative, yellow when positive — NEW).
GLOBAL LFO: RATE knob (lfo_rate) + SHAPE stepper (lfo_shape:
TRI/SIN/SAW/SQR/S+H).

## FX block (see FEATURES.md §11 FX v1.5)
Each effect row layout: TYPE/MODE stepper . primary knobs (3) .
PARAMS FOCUS mini-stepper (LCD) + PARAMS knob (yellow) . on LED/btn.
The PARAMS knob edits the algorithm's currently-focused secondary
parameter; the focus stepper picks which (list is per-algorithm).

MOD FX: type stepper (modfx_type: CHORUS/FLANGER/PHASER/ENSEMBLE/
  DIMENSION/ROTARY/BARBERPOLE) + RATE/DEPTH/MIX + PARAMS focus+knob
  (modfx_pfocus, modfx_p*) + on (modfx_on).
DELAY: mode stepper (DIGITAL/TAPE/PONG) + TIME/FB/MIX + PARAMS
  focus+knob (dly_pfocus, dly_p*) + on (dly_on).
REVERB: type stepper (ROOM/HALL/PLATE) + SIZE/DAMP/MIX + PARAMS
  focus+knob (rev_pfocus, rev_p*) + on (rev_on).
UTILITY row: LO-FI (on + PARAMS focus/knob: BITS/SRATE/COMPAND/ALIAS) .
  WIDTH (on + WIDTH/HAAS knobs + BASS-MONO led-btn) .
  TALK (on + PARAMS: VOWEL-A/B/MORPH/SENS) .
  PRE/POST switch (fx_prepost) for LO-FI/WIDTH placement vs FILTERS.
If vertical space is tight, UTILITY collapses into an FX-page overlay
LCD opened by an FX EDIT button (same idiom as the wave overlay).

Add to matrix DEST list: FX PARAM (targets focused PARAMS of a slot).

## Behavior rules
- LEDs strictly binary red (#ff2b3e lit / #4a1020 unlit), no dimming.
- Yellow text only inside LCD glass; red only for LEDs + filter
  pointers; yellow pointers only for morph/mod/character knobs.
- Steppers wrap. Every value change updates the main-LCD touched
  readout (name + 0-127, bipolar params -63..+63).
- Wave list overlay groups by category, shows entry type tag for new
  bank types: no tag = cycle, [ENS] = loop, [SHOT] = one-shot.
- No animation easing on mode/page changes — instant hardware flips.
  The signal-flow dots/edges are the only continuously animated things.

## Deltas vs v5 (renovation checklist)
1. Add: tvf type mini-stepper per tone; NOISE + NOISE COL + PAN knobs;
   START RND button; AUX DEST + matrix DEST gain NOISE entry.
2. Add: vector SHAPE stepper; P-ENV edit mini page.
3. Add: ENSEMBLE in MOD FX stepper.
4. Change: matrix AMT is bipolar w/ center detent; amount bar red/yellow.
5. Add: [ENS]/[SHOT] tags in wave overlay.
6. Add: per-tone VOICING stepper + DREAMY-spread stepper + LOOP MODE
   toggle; HIT STRETCH + PITCH TRIM knobs for one-shot waves.
7. Keep: everything else from v6 (brushed metal, master knob, signal-flow
   display, delay modes, tone strip) unchanged.
