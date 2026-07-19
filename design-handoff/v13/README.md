# Handoff: The Dreamer — 4-Tone Vector ROMpler GUI (v13, per GUI_SPEC.md + loop/play addendum)

> ⚠ SINGLE SOURCE OF TRUTH: this folder contains exactly ONE panel design —
> `The Dreamer GUI.html` / `The Dreamer GUI.png` (identical state). If the target
> codebase already contains an editor built from an earlier handoff, DELETE/REPLACE it
> fully (all layout constants, header code, any cached editor resources) rather than
> merging — do not keep any earlier header layout where the logo sat under the corner
> screw. Rebuild the VST3 clean (wipe the build dir; bump the plugin version so Cubase
> re-scans instead of loading the cached old editor from its plugin cache).

## ⛔ LAYOUT IS LOCKED — DO NOT REFLOW ON REDRAW
Every control's position, size, and z-order in `The Dreamer GUI.html` / `.png` is FINAL and
FROZEN. Treat the layout as fixed absolute geometry, not a responsive/auto-laid-out UI.

- Build ONE 1140×660 content component with **fixed absolute bounds** for every widget
  (hard-code `setBounds(x, y, w, h)` per the master; do NOT use FlexBox/Grid/auto-layout,
  and do NOT recompute positions from font metrics or child counts).
- On repaint/resize, **nothing moves**: the ONLY transform allowed is the single uniform
  `AffineTransform::scale` on the root content component (see "Window resize"). No per-widget
  re-anchoring, re-centering, re-flowing, or "tidy-up" passes. `resized()` sets the root
  transform and returns — it must not touch child bounds.
- Value/state changes (mode swaps, greying, LED on/off, menu overlays, tone reselect) change
  ONLY the pixels of the affected control. They must NEVER shift, resize, or re-order any
  other object. In particular: STRETCH/P.TRIM knobs are **greyed in place, never hidden or
  removed** (no layout collapse); LOOP↔PLAY swap occupies the **same fixed slot**; opening a
  menu/UTIL overlay draws on top and moves nothing beneath it.
- Do not "improve", round, snap-to-grid, or re-align anything vs. the master. If a value
  looks odd (e.g. a 288px column, 46px knob pitch, 22/28px offsets), it is intentional —
  reproduce it verbatim.
- If you must add a control later, give it its own new fixed bounds; never reflow neighbors
  to make room.

Rationale: prior re-draws drifted the layout. Lock it.

## Overview
Plugin editor GUI for **The Dreamer**, a 90s-style 12-bit ROM vector synthesizer VST3 (JUCE).
Fixed-size panel styled as a piece of rack hardware: blue-purple "cosmic" faceplate,
red LEDs, yellow-on-black character LCDs, silkscreen typography. Four tones (A–D) mixed
by a rotating vector phase ("constellation"), dual global filters, mod matrix, and FX chain.

## About the Design Files
The files in this bundle are **design references created in HTML** — interactive prototypes
showing the intended look and behavior, NOT production code. The task is to **recreate this
design in the target codebase** — per the project brief this is **JUCE (C++)**: vector-drawn
components (or filmstrip atlases) mirroring these masters, with the LCD rendered as a drawn
pixel grid. If the target environment differs, use its established patterns instead.

- `The Dreamer GUI.html` — self-contained interactive prototype (open in any browser; all
  controls work: knob/slider drag, menus, constellation animation).
- `The Dreamer GUI.png` — 2× screenshot (2280×1320) of the default state.
- `DESIGN.md` — the original design-vision brief (pillars, references, constraints).

## Fidelity
**High-fidelity.** Colors, typography, spacing, and interactions are final. Recreate
pixel-perfectly. All values below are exact.

## Panel
- Logical size: **1140 × 660 px** control panel (fixed ratio, 2× assets for HiDPI).
  A keyboard/wheels strip (see "Keyboard & wheels") folds out below it; **collapsed by
  default** (panel height 660), expanding to **848** when open. The host window should
  resize with the fold toggle.
- Background `#232850`, border-radius 6, drop shadow `0 12px 40px rgba(0,0,0,.6)` +
  inner top highlight `inset 0 1px 0 rgba(201,205,242,.12)`.
- **Faceplate finish (default ON)**: seeded brushed-metal texture (horizontal grain
  streaks every 4–9px, white/black at .8–3.2% opacity, ~.5–1.2px tall, plus 18 long
  highlight streaks at 3–8% opacity; seed 19971, LCG a=16807), top-light gradient
  (white .10 → black .12 top-to-bottom), 122° diagonal sheen (≤7% white), edge vignette
  (radial, black .16 at corners), and 4 corner screws (r8 radial-gradient head
  `#6b708f→#292b42→#0d0d1a`, 9×1.6 slot at a random angle; centers 22px from corners).
  Optional star-field variant (80 seeded 1–2px dots, `#c9cdf2` at 12–42% opacity),
  off by default.

## Design Tokens
    panel-bg        #232850   faceplate
    panel-header    #1b1f40   top strip
    group-frame     #464e94   function-group borders, button borders
    silkscreen      #9aa1d8   parameter labels
    silkscreen-hi   #c9cdf2   group titles, button legends
    silkscreen-dim  #7d84c0   footnotes
    led-on          #ff2b3e   lit LED (binary; glow 0 0 8px rgba(255,43,62,.85))
    led-off         #4a1020   unlit LED (no glow)
    knob-outer      #14162c   knob skirt
    knob-body       #2a2d48   knob cap (fluted: repeating-conic #2a2d48/#20233c 12° steps)
    pointer-white   #e8eaff   default knob pointers / slider cap lines
    pointer-red     #ff5b6e   filter-section knob pointers, TVF accents
    pointer-yellow  #ffd23f   "special" pointers: SHP DPT, MORPH, PHASE, DIR/INT, matrix AMT
    lcd-bg          #07070a   all displays (inset shadow `inset 0 1px 4px #000`)
    lcd-ink         #ffd23f   all LCD text
    slider-track    #0c0e20
    button-face     #181b34   (active/pressed #10122a; stepper active #2c3160)
    page-bg         #15172e   area around the panel

Rules: red appears ONLY as LEDs and filter pointers; yellow ONLY inside LCD glass and on
the designated special pointers. LEDs are strictly binary. No gradients on the faceplate.

## Typography
- **Silkscreen**: Barlow Semi Condensed (Google Fonts). Group titles 700 10px,
  letter-spacing .18em, uppercase. Parameter labels 600 7.5–8.5px, spacing .08–.14em.
- **LCD**: Doto 800 (dot-matrix font), 10–13px. Never mix LCD and silkscreen fonts.
- **Logo**: Michroma 400 19px, letter-spacing .14em — "THE DREAMER", with
  7.5px/`.34em` sub-line "12-BIT ROM VECTOR SYNTHESIZER".

## Layout
Header (60px, `#1b1f40`, 1px bottom border `#464e94`): logo left; main LCD center
(520×44) with **preset ▲/▼ steppers** (22×19, wrapping) at its left: 12-bar waveform
glyph, line 1 `P<NNN> <PRESET NAME>` (click → preset browser menu page; 24 factory
presets grouped PAD/SPLIT/BELL/STR/VOX/BASS/LEAD/KEY/SFX/INIT) + `TONE <A-D>`,
line 2 last-touched param `NAME <0-127>` (bipolar params read −63…+63) + 14-cell block
meter. Right side, in order: **MIDI LEARN** button + LED, **stereo L/R output meters**
(7×34 LCD-black bars, yellow fill with red top 20%, peak-hold decay ×.82/tick),
**MASTER volume knob (Ø40)**, POWER LED (26px clear of the corner screw). Logo block
sits 26px in from the left screw. **MIDI learn mode**: while ON, all knob pointers dim
to `#464e94`; mapped controls show red pointers; clicking a control assigns/clears a CC
and the LCD echoes `LEARN <PARAM>`.

Row 1 — grid `172px | 1fr | 200px`, gap 10, height 352, padding 14/12:
1. **TONES** (mixer) — 4 columns (A–D): name, select LED + button, 130px level fader,
   **PAN mini-knob Ø24 (bipolar, center detent)**, ON LED + button. Column dims to 55%
   opacity when tone is off.
2. **TONE EDIT <X>** (X = selected tone letter, `#ff5b6e`):
   - Row: WAVE LCD (170×20, "CAT > NAME") + `<` `>` steppers; SHAPE LCD (96×20) + steppers
     (OFF/SOFT FOLD/HARD FOLD/SINE FOLD/ASYM/DRIVE).
   - Row: knobs Ø34 on a uniform 4px-gap grid — OCTAVE, FINE, START, [**RND** btn + LED:
     start_random], 1px divider, LEVEL, VELOCITY; then the right "shape compartment"
     (pushed right, aligned above the FILTER ADSR bank): **STRETCH** + **P.TRIM** knobs
     (greyed/disabled unless a HIT wave is in STRETCH play mode — never hidden), then
     SHAPE DEPTH (yellow), NOISE, NOISE COLOR. Full-word silkscreen labels, grid-aligned.
     (PAN lives in the TONES mixer strip.)
   - The WAVE/SHAPE row also carries the **LOOP/PLAY selector** (right of the SHAPE
     stepper), swapped by the loaded wave's bank type — see "Loop & Play modes" below.
   - Row: vertical red "TVF" label; **TYPE LCD** (36×18: LP24/LP12/BP/HP); 4 knobs Ø34 red
     (CUT RES ENV KF); three 4-slider ADSR banks (62px tall) labeled **FILTER / AMPLITUDE
     / AUX**, separated by 1px vertical dividers (`#464e94` @40%).
   - Rows: **LFO1** and **LFO2** — each RATE + DEPTH knobs Ø26, **SHAPE mini-LCD** (34×16,
     TRI/SIN/SAW/SQR/S+H, click = menu, no arrows), SYNC btn + LED, DEST
     select (PITCH/CUTOFF/SHAPE/LEVEL). All per-tone. When SYNC is lit the RATE knob's
     pointer turns yellow and it quantizes to time divisions shown as its label:
     4/1, 2/1, 1/1, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32.
   - Row: **AUX ENV** — bipolar AMOUNT± knob Ø26 (yellow, center detent, −63…+63 readout) +
     DEST select (PITCH/START/SHAPE/PAN/NOISE) + **VOICING** stepper (SINGLE/OCT/POWER/
     DREAMY) + **DREAMY-spread** stepper (ADD9/MIN7/SUS2, shown only when VOICING=DREAMY).

   **Loop & Play modes** (one shared slot on the WAVE/SHAPE row, swapped by wave bank tag —
   ""=CYCLE, "ENS"=LOOP, "SHOT"=HIT):
   - CYCLE wave → greyed **LOOP · FWD** button (disabled placeholder, neither applies).
   - LOOP wave → **LOOP MODE** latching button + LED (loop_mode: FWD / PINGPONG; LED lit =
     PINGPONG).
   - HIT wave → **PLAY MODE** latching button + LED (hit_play: NORMAL / STRETCH; LED lit =
     STRETCH). When STRETCH is active the STRETCH (0.25×–4× log, center 1.0) and P.TRIM
     (−24…+24 st) knobs in the row above become active; otherwise they stay greyed.
3. **FILTERS** — two strips: type LCD (click = menu; `<` `>` steppers) + 3 knobs Ø36:
   F1 CUT/RES/ENV (red), F2 CUT/RES/**MORPH** (MORPH yellow). 1px divider. Bottom:
   SER/PAR routing toggle with two binary LEDs.
   Types (14): LP 24, LP 12, BP, HP, LIQUID, CLASSIC, LADDER, NOTCH, COMB +, COMB −,
   N+LP, FORMANT, ALLPASS, DREAMPLN. Next to SER/PAR: **BAL 1↔2 knob Ø26
   (bipolar, yellow, center detent)** — filter output balance.

Row 2 — grid `420px | 296px | 1fr`, gap 10, height 212:
4. **DREAM VECTOR** — 150×132 LCD "SIGNAL FLOW · PHASE", neural-net style: four source
   nodes A–D on the left (x=22, y=22/52/82/112; radius 6+gain×3, `#ffd23f`, dim `#3a3520`
   when off; letter inside, selected = `#ff5b6e`), cubic-bezier connections
   (`M x0+9 y C x0+45 y, x1−40 66, x1 66` with x0=22, x1=117) converging into a **Σ MIX**
   node (circle r9 `#464e94` at 126,66; yellow Σ glyph). Connection stroke-width
   .5+gain×2.5 and opacity .08+gain×.8. A red dot (r 1.5+gain×2, `#ff2b3e`) flows along
   each active connection (speed .01+gain×.05 per 40ms tick; hidden when gain≤.05 or tone
   off). Phase readout: 138×3 track at bottom with a 10px red segment at x=6+phase×128.
   Knobs: PHASE (yellow) + RATE, Ø32. **SHAPE** stepper below the display
   (vec_orbit_shape: SAW/TRI/SIN/SQR/S+H). Toggles: ORBIT, P-ENV (LED + button) +
   **EDIT** btn → P-ENV mini page (modal): START/END angle knobs (yellow) + TIME knob +
   LOOP btn/LED, ESC exits. Right of a 1px divider: **per-tone DIR + INT knob rows**
   (Ø22, yellow) for all four tones A–D, selected tone letter red.
   Gain law: `g = max(0, cos(2π·phase − 2π·DIR))²`, `gain = (1−INT) + INT·g`.
5. **MOD MATRIX** — 3 slots: number, SRC button, ">", DST button, Ø26 yellow AMT knob —
   **bipolar** (center detent; −63…+63 readout), amount bar (8px, `#0c0e20` track) shows
   |amt| from left, **yellow when positive, red (`#ff2b3e`) when negative**.
   SRC: G-LFO, VEC PHS, AUX, VELO, WHEEL. DST: PITCH, CUT 1, CUT 2, MORPH, SHAPE,
   VEC PHS, PAN, NOISE, **FX PARAM** (targets the focused PARAMS of an FX slot).
   Bottom: GLOBAL LFO — RATE Ø30 + shape button (TRI/SIN/SAW/SQR/S+H).
6. **FX** — three rows separated by 1px dividers, each: name (28px), type LCD (46px, Doto,
   click = menu), 3 primary knobs Ø26, a **PARAMS FOCUS** mini-LCD (44px, click cycles the
   algorithm's secondary-parameter list), a yellow **PARAMS** knob Ø26 that edits the
   focused secondary param, and an LED + on/off button:
   MOD (CHORUS/FLANGER/PHASER/ENSEMBLE): RATE, DEPTH, MIX · focus DELAY/WIDTH/FEEDBK/TONE.
   DELAY (DIGITAL/TAPE/PONG): TIME, FB, MIX · focus WOW/FLUTTER/TONE/DUCK + **SYNC btn +
   LED** — when lit the TIME knob's pointer turns yellow and its label shows the tempo
   division (4/1…1/32, same table as LFO sync); dly_sync is an APVTS parameter.
   REVERB (ROOM/HALL/PLATE): SIZE, DAMP, MIX · focus PREDLY/WIDTH/LO CUT/HI CUT.
   Header **UTIL ▸** button (top-right of the FX group) opens the **UTILITY overlay**
   (modal, same idiom as the wave menu): LO-FI (on + focus BITS/SRATE/COMPAND/ALIAS +
   PARAMS knob), WIDTH (on + WIDTH + HAAS knobs + BASS MONO led-button), TALK (on + focus
   VOWEL-A/B/MORPH/SENS + PARAMS knob), and a **PRE/POST** switch (fx_prepost) placing
   LO-FI/WIDTH before or after the FILTERS. ESC exits.
   Vector SHAPE and GLOBAL LFO shape buttons carry < > arrows.
   Tone-edit rows share a fixed 288px left column; the shape compartment (STRETCH/P.TRIM/
   SHAPE DEPTH/NOISE/NOISE COLOR) is pushed right to sit above the FILTER/AMPLITUDE/AUX
   env banks; the TVF group is vertically centered in its 88px row.

## Components
- **Knob**: outer skirt circle `#14162c` (drop shadow `0 2px 4px rgba(0,0,0,.5)`); fluted
  cap at 76% of skirt Ø (repeating-conic 12° flutes); solid `#2a2d48` center (inset 5px);
  pointer 2px wide, ~34% of cap Ø long, rotates −135°…+135° over the 0–1 range.
  Drag: vertical, full range over 140px, pointer capture; label beneath (silkscreen 8px).
- **Slider**: 9px-wide track `#0c0e20` (radius 2, inset shadow); cap 19×9 `#2a2d48`
  with 2px `#e8eaff` top line. Same vertical drag law.
- **Stepper**: 20×18 `#181b34`, 1px `#464e94` border, `<`/`>` glyph, active `#2c3160`.
- **Push button**: `#181b34`, 1px `#464e94` border, radius 3, shadow, pressed `#10122a`;
  paired 6–8px round LED above/left (binary).
- **LCD readout**: `#07070a`, radius 2, inset shadow, Doto 800 yellow text. Clicking any
  select-type LCD (WAVE, SHAPE, TVF TYPE, FILTER 1/2) or any type button (matrix SRC/DST,
  FX types) opens the **menu overlay**.
- **Menu overlay**: full-panel scrim `rgba(7,7,10,.82)`; 430px LCD page (max-height 520);
  wave entries may carry a bank-type tag: no tag = cycle, `[ENS]` = loop, `[SHOT]` =
  one-shot:
  title row + "ESC=EXIT", rows `NN  CAT  NAME` in Doto 12px; current row inverse-video
  (yellow bg, black text, ▸ prefix). Click row = select + close; ESC or scrim click = close.
  Slim dark scrollbar (6px, thumb `#3a3410`).

## Window resize  ⚠️ READ THIS — three shipped bugs came from getting it wrong
**The window (editor) size is the ONE source of truth. The content transform is DERIVED
from it — never the other way around.** There is exactly one number that drives scale:
`getWidth() / 1140.0`. Everything follows from it.

The whole plugin — faceplate border, all controls, AND the key bed — is one content
Component built at its **logical** size (1140×660 collapsed, 1140×848 with keys open). The
editor is a thin wrapper that does nothing but size that component to fill itself:

```cpp
// Editor ctor
setResizable (true, true);
getConstrainer()->setFixedAspectRatio (logicalW / logicalH);   // ← recompute on keybed toggle
setResizeLimits (570, 330, 2280, 1696);                        // 0.5×…2× of the tallest logical size
setSize (1140, 660);                                           // start collapsed

void resized() override {
    const float k = getWidth() / 1140.0f;      // uniform scale, width-driven
    content.setTransform (AffineTransform::scale (k));
    content.setBounds (0, 0, roundToInt (getWidth()/k), roundToInt (getHeight()/k));
}
```

**Do NOT** add a separate "GUI scale" combo/menu that scales `content` while the editor
stays the same size. If the user wants preset zoom steps (100/150/200 %), each step must
call `setSize (1140*f, logicalH*f)` on the **editor** — i.e. resize the window — and let
`resized()` derive the transform. Scale is never set from anywhere but `resized()`.

**Keybed toggle** = a window resize, nothing more. On toggle: pick the new logical height
(660 or 848), update the aspect-ratio constrainer, then `setSize` the editor to
`(currentWidth, round(currentWidth / newAspect))`. The content component's own height
changes 660↔848; the transform recomputes automatically in `resized()`.

The in-prototype corner grip (15×15 diagonal lines, double-click = 100 %) previews this;
in the plugin the host window border replaces it.

### The three bugs to avoid (all one root cause: window size and content scale got out of sync)
1. **Buttons/knobs overflow the panel edge.** Cause: content scaled up (e.g. a 150 % GUI
   menu, or width dragged without a locked aspect ratio) while the window/faceplate stayed
   1140 wide, so controls spill past the border. Fix: aspect ratio locked + scale derived
   only from `getWidth()`; the faceplate is INSIDE `content`, so it scales with everything
   else and can never be smaller than the controls.
2. **Frame only grows when the key bed opens.** Cause: `setSize` is called on the keybed
   path but not on the scale/resize path. Fix: every size change (keybed AND zoom) goes
   through the same `setSize`-on-the-editor route above.
3. **Window frame stays fixed while the layout grows/shrinks inside it.** Cause: a GUI-scale
   factor transforms `content` directly instead of resizing the editor. Fix: never
   transform `content` outside `resized()`; zoom = resize the window, and the transform
   follows.

## Keyboard & wheels (from the Figma faceplate file — values exact)
**Rubber band**: full-width 1140×18 strip at y=640 (just under the bottom screws),
gradient `#101018 → #1c1c26 30% → #14141c 70% → #08080e`, drop shadow + 1px top
highlight — separates metal panel from key bed. Centered on it: the **KEYS fold button**
(56×12, pill, `#181b34`/`#464e94` border, "▼ KEYS"/"▲ KEYS") — toggles the whole
keyboard strip. Collapsed logical height 660, expanded 848. The toggle is a WINDOW resize
(see "Window resize": update the aspect constrainer to the new logical height, then
`setSize` the editor); the key bed lives inside the same content Component and scales with
everything else — it is not a separately-sized panel.
Key-bed strip at y=658, 1140×190, radius 0 0 8 8, bg `#1a1c38`, inset shadows
`0 1px 1px rgba(255,255,255,.1)` + `0 4px 8px rgba(0,0,0,.6)`.
- Wheel bay 128×170 at (8,10): radius 6, gradient `#0d0d1c→#171a2e`, inset `0 3px 6px rgba(0,0,0,.7)`.
- Two wheel slots 40×140 (radius 20, `#05050d`, inset `0 2px 5px rgba(0,0,0,.9)`) at x=24 and x=76, y=25.
- PITCH and MOD wheels 32×118 at (28,36) and (80,36): radius 16, gradient
  `#121426 0% → #383d61 35% → #454a73 50% → #383d61 65% → #121426 100%`, drop shadow
  `0 2px 4px rgba(0,0,0,.6)`, 18–19 horizontal ribs (2px, rgba(0,0,0,.55), ~6.2–6.5px pitch)
  that scroll with wheel position. PITCH additionally carries a red center groove stripe
  (32×3, `#ff2b3e`, glow `0 0 4px rgba(255,43,62,.7)`, at wheel center, travels with
  bend). Drag DOWN = bend up (inverted mouse response, hardware-style); PITCH springs
  back to center on release; MOD holds.
  Both echo on the main LCD (PITCH BEND bipolar, MOD WHEEL 0–127). Silkscreen labels above.
- Key bed from x=144: white keys 32.662×170, radius 0 0 3px 3px, gradient
  `#c7c9d6 0% → #e0e3ed 75% → #f7f7fc 94% → #b2b5c7 100%`, shadows
  `1px 0 1px rgba(0,0,0,.35)` + inset `0 -3px 3px rgba(0,0,0,.25)`; black keys 20×104,
  gradient `#1a1a24 → #0a0a12 60% → #383b52 92% → #05050a`, shadow `0 3px 5px rgba(0,0,0,.5)`,
  offset +22.5px within their white key. ~4 octaves (30 white / 21 black in the mock).
  Keys darken while pressed; clicking sends note-on (wire to MIDI keyboard state /
  juce::MidiKeyboardComponent with custom LookAndFeel matching these values).

## Version label
Silkscreen `VER 1.0` (Barlow 600 7.5px, letter-spacing .16em, `#7d84c0`) at bottom-right
of the faceplate, above the rubber-band strip (right: 34px, top: 626px). Bump with releases.

## Interactions & Behavior
- Any control drag updates the main LCD "touched" line (name + 0–127 value, coarse
  integer steps) and the 14-cell meter.
- Selecting a tone (strip button) rebinds the whole TONE EDIT group and LCD tone label.
- ORBIT on: phase advances `(.003 + RATE×.012)` per 40ms tick, wrapping 0–1; constellation
  dots pulse per the gain law. PHASE knob sets phase manually.
- LED states are binary — never dimmed (halo glow allowed on lit state only).
- Mode/value changes are instant (hardware page-flip feel); no crossfades or eased
  transitions anywhere.

## State Management (parameter map)
All controls map 1:1 to APVTS parameters; the only UI-only state is the menu overlay,
tone selection, and preview phase animation.
- Per tone ×4: wave (0–77), on, oct, fine, start, start_random, level, pan (bipolar), velo,
  noise, noise_color, shape (0–5), shape_depth, tvf_type (LP24/LP12/BP/HP),
  tvf_cut/res/env/kf, TVF ADSR ×4, TVA ADSR ×4, AUX ADSR ×4 + aux_amt (bipolar) +
  aux_dest (0–4 incl NOISE), lfo1_rate/depth/sync/dest, lfo2_rate/depth/sync/dest,
  dir, vint, **voicing (SINGLE/OCT/POWER/DREAMY), dreamy_spread (ADD9/MIN7/SUS2),
  loop_mode (FWD/PINGPONG), hit_play (NORMAL/STRETCH), hit_stretch (0.25×–4× log),
  hit_pitchtrim (−24…+24 st)**. Parameter IDs per DSP_BUILD.md §9/§12/§13 / GUI_SPEC.md.
- Global: **master**, flt1_type (0–13), flt1_cut/res/env, flt2_type, flt2_cut/res/morph,
  flt_route (ser/par), flt_bal (bipolar), vec_phase, vec_orbit_rate,
  vec_orbit_shape (SAW/TRI/SIN/SQR/S+H), vec_orbit_on, vec_penv_on,
  vec_penv_start/end/time/loop, matrix ×3 (src, dst incl **FX PARAM**, amount **bipolar**),
  lfo_rate, lfo_shape, modfx_type/rate/depth/mix/on + **modfx_pfocus/modfx_param**,
  dly_mode/time/fb/mix/on + **dly_pfocus/dly_param**, rev_type/size/damp/mix/on +
  **rev_pfocus/rev_param**, **lofi_on/pfocus/param, width_on/width/haas/bassmono,
  talk_on/pfocus/param, fx_prepost**, preset index (browser is UI-side).
- LFO sync: lfoN_sync toggles rate from Hz to the 12 tempo divisions above.
- MIDI learn map and GUI scale are UI/editor-side state, not APVTS parameters.
- Output meters read the processor's stereo output RMS/peak (UI shows peak-hold).
- Wave ROM: **all 78 waves** (PAD 16, STR 10, VOX 10, BELL 14, MTL 12, BAS 16) — full
  list in the prototype source (`WAVES` array).

## Assets
No bitmap assets. Everything is vector/programmatic. Fonts from Google Fonts:
Barlow Semi Condensed (400/600/700), Doto (800), Michroma (400) — embed or substitute
equivalent condensed grotesque / 5×7 dot-matrix faces in JUCE.

## Files
- `The Dreamer GUI.html` — interactive prototype (single file, works offline)
- `The Dreamer GUI.png` — 2× reference screenshot
- `DESIGN.md` — original design brief (pillars, hardware references, constraints)
