# Handoff: The Dreamer — 4-Tone Vector ROMpler GUI (v8, per GUI_SPEC.md)

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
- Logical size: **1140 × 660 px** fixed ratio (2× assets for HiDPI).
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
   - Row: knobs Ø34 at tight 40px pitch — OCTV, FINE, START, [**RND** btn + LED:
     start_random], 1px divider, LEVEL, VELO, SHP DPT (yellow), **NOISE**, **NS COL**.
     (PAN lives in the TONES mixer strip.)
   - Row: vertical red "TVF" label; **TYPE LCD** (36×18: LP24/LP12/BP/HP); 4 knobs Ø34 red
     (CUT RES ENV KF); three 4-slider ADSR banks (62px tall) labeled **FILTER / AMPLITUDE
     / AUX**, separated by 1px vertical dividers (`#464e94` @40%).
   - Rows: **LFO1** and **LFO2** — each RATE + DEPTH knobs Ø26, **SHAPE mini-LCD** (34×16,
     TRI/SIN/SAW/SQR/S+H, click = menu, no arrows), SYNC btn + LED, DEST
     select (PITCH/CUTOFF/SHAPE/LEVEL). All per-tone. When SYNC is lit the RATE knob's
     pointer turns yellow and it quantizes to time divisions shown as its label:
     4/1, 2/1, 1/1, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32.
   - Row: **AUX ENV** — bipolar AMT± knob Ø26 (yellow, center detent, −63…+63 readout) +
     DEST select (PITCH/START/SHAPE/PAN/NOISE).
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
   VEC PHS, PAN, NOISE. Bottom: GLOBAL LFO — RATE Ø30 + shape button (TRI/SIN/SAW/SQR/S+H).
6. **FX** — three rows separated by 1px dividers, each: name (30px), < > arrows + type button (44px, click = menu), 4 knobs Ø30 at 36px
   pitch — 3rd is a yellow-pointer type-dependent **PARAM**,
   LED + on/off button:
   MOD (CHORUS/FLANGER/PHASER/ENSEMBLE): RATE, DEPTH, PARAM, MIX.
   DELAY (DIGITAL/TAPE/PONG): TIME, FB, PARAM, MIX.
   REVERB (ROOM/HALL/PLATE): SIZE, DAMP, PARAM, MIX.
   Vector SHAPE and GLOBAL LFO shape buttons also carry < > arrows.
   Tone-edit rows share a fixed 288px left column so SHP DPT/NOISE/NS COL and the
   FILTER/AMPLITUDE/AUX env banks align to the SHAPE-label vertical; the TVF group is
   vertically centered in its 88px row.

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

## Window resize
Cubase/VST3-style proportional resize: drag grip (15×15, bottom-right corner, diagonal
lines `#464e94`) scales the whole editor 50–200% (transform scale; LCD shows GUI SIZE
while dragging); double-click grip resets to 100%. Default size 1140×660. In JUCE:
setResizable(true,true) + setFixedAspectRatio + AudioProcessorEditor::setScaleFactor.

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
  dir, vint. Parameter IDs per DSP_BUILD.md §9 / GUI_SPEC.md.
- Global: **master**, flt1_type (0–13), flt1_cut/res/env, flt2_type, flt2_cut/res/morph,
  flt_route (ser/par), vec_phase, vec_orbit_rate, vec_orbit_shape (SAW/TRI/SIN/SQR/S+H),
  vec_orbit_on, vec_penv_on, vec_penv_start/end/time/loop, matrix ×3 (src, dst,
  amount **bipolar**), lfo_rate, lfo_shape, modfx_type/rate/depth/mix/on,
  dly_mode/time/fb/mix/on, rev_type/size/damp/mix/on, preset index (browser is UI-side).
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
