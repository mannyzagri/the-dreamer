# Handoff: The Dreamer — 4-Tone Vector ROMpler GUI (v5)

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
- Optional faceplate variants (off by default): star-field (80 seeded 1–2px dots,
  `#c9cdf2` at 12–42% opacity) and brushed-metal (repeating 3px horizontal line gradient,
  white/black at ≤3% opacity).

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
(520×44): 12-bar waveform glyph, line 1 `P042 ETHEREAL DAWN` + `TONE <A-D>`,
line 2 last-touched param `NAME  <0-127>` + 14-cell block meter; **MASTER volume knob
(Ø40)** and POWER LED right.

Row 1 — grid `118px | 1fr | 200px`, gap 10, height 352, padding 14/12:
1. **TONES** — 4 columns (A–D): name, select LED + button, 150px level fader,
   ON LED + button. Column dims to 55% opacity when tone is off.
2. **TONE EDIT <X>** (X = selected tone letter, `#ff5b6e`):
   - Row: WAVE LCD (170×20, "CAT > NAME") + `<` `>` steppers; SHAPE LCD (96×20) + steppers.
   - Row: 7 knobs Ø36 — OCTV, FINE, START, LEVEL, **PAN**, VELO, SHP DPT (yellow pointer).
   - Row: vertical red "TVF" label; **TYPE LCD** (36×18: LP/HP/BP); 4 knobs Ø34 red
     pointers (CUT RES ENV KF); three 4-slider ADSR banks (62px tall): TVF ENV, TVA ENV,
     AUX ENV (+ 44px AUX dest button: PITCH/START/SHAPE/PAN).
   - Row: TONE LFO — DEPTH knob Ø30, DEST button (PITCH/CUTOFF/SHAPE/LEVEL);
     VECTOR — DIR + INT knobs Ø30 yellow pointers.
     Footnote: "SHAPER + TVF + AUX + LFO + VECTOR + PAN ARE PER-TONE".
3. **FILTERS** — two strips: type LCD (click = menu; `<` `>` steppers) + 3 knobs Ø36:
   F1 CUT/RES/ENV (red), F2 CUT/RES/**MORPH** (MORPH yellow). 1px divider. Bottom:
   SER/PAR routing toggle with two binary LEDs.
   Types (14): LP 24, LP 12, BP, HP, LIQUID, CLASSIC, LADDER, NOTCH, COMB +, COMB −,
   NL3 N+LP, FORMANT, ALLPASS, DREAMPLN.

Row 2 — grid `250px | 296px | 1fr`, gap 10, height 212:
4. **DREAM VECTOR** — 150×132 LCD "SIGNAL FLOW · PHASE", neural-net style: four source
   nodes A–D on the left (x=22, y=22/52/82/112; radius 6+gain×3, `#ffd23f`, dim `#3a3520`
   when off; letter inside, selected = `#ff5b6e`), cubic-bezier connections
   (`M x0+9 y C x0+45 y, x1−40 66, x1 66` with x0=22, x1=117) converging into a **Σ MIX**
   node (circle r9 `#464e94` at 126,66; yellow Σ glyph). Connection stroke-width
   .5+gain×2.5 and opacity .08+gain×.8. A red dot (r 1.5+gain×2, `#ff2b3e`) flows along
   each active connection (speed .01+gain×.05 per 40ms tick; hidden when gain≤.05 or tone
   off). Phase readout: 138×3 track at bottom with a 10px red segment at x=6+phase×128.
   Knobs: PHASE (yellow) + RATE, Ø32. Toggles: ORBIT, P-ENV (LED + button).
   Gain law: `g = max(0, cos(2π·phase − 2π·DIR))²`, `gain = (1−INT) + INT·g`.
5. **MOD MATRIX** — 3 slots: number, SRC button, ">", DST button, Ø26 yellow AMT knob
   (no label), amount bar (8px, `#0c0e20` track, `#ffd23f` fill at 80%).
   SRC: G-LFO, VEC PHS, AUX, VELO, WHEEL. DST: PITCH, CUT 1, CUT 2, MORPH, SHAPE,
   VEC PHS, PAN. Bottom: GLOBAL LFO — RATE Ø30 + wave button (TRI/SIN/SAW/SQR/S+H).
6. **FX** — three rows separated by 1px dividers, each: name, type button, 3 knobs Ø32,
   LED + on/off button:
   MOD (CHORUS/FLANGER/PHASER): RATE, DEPTH, MIX. DELAY (DIGITAL/TAPE/PONG): TIME, FB,
   MIX. REVERB (ROOM/HALL/PLATE): SIZE, DAMP, MIX.

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
- **Menu overlay**: full-panel scrim `rgba(7,7,10,.82)`; 430px LCD page (max-height 520):
  title row + "ESC=EXIT", rows `NN  CAT  NAME` in Doto 12px; current row inverse-video
  (yellow bg, black text, ▸ prefix). Click row = select + close; ESC or scrim click = close.
  Slim dark scrollbar (6px, thumb `#3a3410`).

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
- Per tone ×4: wave (0–27), on, oct, fine, start, level, pan, velo, shape (0–5),
  shapeDepth, tvfType (LP/HP/BP), cut, res, env, kf, TVF ADSR ×4, TVA ADSR ×4,
  AUX ADSR ×4, auxDest is global-per-tone? → auxDest (0–3), lfoDepth, lfoDest (0–3),
  dir, vint.
- Global: **master** (volume), f1Type (0–13), f1Cut/Res/Env, f2Type, f2Cut/Res/Morph,
  route (ser/par), vphase, orbRate, orbit, penv, matrix ×3 (src, dst, amount), glfoRate,
  glfoWave, modFxType/Rate/Depth/Mix/On, dlyMode/Time/Fb/Mix/On, revType/Size/Damp/Mix/On.
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
