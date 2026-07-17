# THE DREAMER — UI design vision

*Dual-mode 90s ROMpler VST3. This document briefs the design work (Claude
Design / Figma). Reference images will be added to the References section —
treat them as authoritative over the prose when they conflict.*

---

## 1. What this instrument is

A love letter to the 1994–1999 rack ROMpler: Roland JD-990, E-MU Orbit /
Planet Phatt, and the sound of dream house and early trance built on them.
78 single-cycle ROM waves played through an authentic 12-bit signal path —
aliasing, ROM grain and all — with two personalities:

- **Mode 1 "RHINO"** — single-oscillator acid topology (Rubber-Rhino lineage)
- **Mode 2 "DREAM"** — dual-partial JD-990-style topology for pads, bells,
  strings, vox

The name is the aesthetic: *The Dreamer* is the machine that made Children-era
dream house — hazy pads, glassy bells, detuned strings at 3 AM. The UI should
feel like a piece of studio hardware you'd find lit by a single desk lamp,
not like software.

## 2. Design pillars

1. **Hardware, not skeuomorphic kitsch.** A 2U rack unit rendered flat-ish:
   believable panel, controls, and silkscreen — but no fake screws-and-wood
   photorealism. Think "official editor for hardware that never existed."
2. **1990s rack idiom.** Dense panel, small labeled controls in bordered
   function groups, a central character display, silkscreen typography.
   The JD-990 front panel and E-MU's colored-panel rack units are the
   spiritual parents.
3. **Cosmic panel, hot red light.** Reference-locked (user direction):
   Novation Supernova 2 colors/lights/LEDs, with E-MU Audity 2000 as a
   secondary panel-graphics reference. Blue-purple "cosmic" faceplate,
   knobs AND sliders, bright red LEDs. LEDs are the ONLY red on the panel —
   lit = bright red, unlit = dark maroon, strictly binary (Supernova
   behavior). Silkscreen in pale lavender/ice. The Plastic-Rhino amber/dark-
   grey language is NOT carried over; only the physical knob/button forms
   may be reused, recolored to this palette.
4. **The display is the soul.** Black LCD panels with BRIGHT YELLOW text
   (user-locked). One main character display (waveform, mode, patch name,
   touched-parameter value) plus small inline yellow-on-black readouts for
   waveform selects and mod destinations. Visible pixel-grid character font.
5. **Authentic limitation as visual language.** No smooth gradients-of-2020,
   no glassmorphism. Value readouts snap in coarse steps. LED states are
   binary or 3-level, never smoothly dimmed (except the halo glow bloom,
   which is part of the established button design).

## 3. Layout concept (to be validated against references)

    +----------------------------------------------------------------------+
    |  THE DREAMER            [========= LCD DISPLAY =========]   [LOGO]   |
    |                          Pad > Hollow 3     DREAM  P042              |
    +----------------------------------------------------------------------+
    | MODE      | PARTIAL A            | PARTIAL B            | FILTER     |
    | [RHINO]   | WAVE  <  >  octv fine| WAVE  <  >  octv fine| type sel   |
    | [DREAM]   | start level velsens  | start level velsens  | cutoff res |
    |           | TVF: cut res env kf  | TVF: cut res env kf  | env  kf    |
    |           | ENV: A D S R  (x2)   | ENV: A D S R  (x2)   | (Mode 1)   |
    +----------------------------------------------------------------------+
    | LFO / MOD (Mode 1 routing)       | FX: chorus  delay  [drive]        |
    | rate depth dest                  | mix  time  fb   ...               |
    +----------------------------------------------------------------------+

- Mode switch morphs the panel: RHINO hides Partial B and shows the Mode 1
  mod-routing group; DREAM shows both partials. The morph should feel like
  a hardware page-flip (instant, maybe a display flicker), NOT an animated
  crossfade.
- Waveform selection: category + name via inc/dec buttons AND a click-to-open
  list overlay styled as an LCD menu page (inverse-video cursor line).
- Knob-count target: dense but not JD-800. Everything on one panel, no tabs
  except the waveform list overlay.

## 4. Component inventory (Figma library)

Reuse from the existing Plastic-Rhino asset set (file oUsZUkLxAyZU0umsFHn2Mc):
- Fluted dark-grey rubber knob (all sizes)
- Square push-button with amber halo LED (on/off/blink states)

New components needed:
- Character-LCD display block (pixel-grid font, 2 sizes: main display +
  inline value readouts)
- Inc/dec button pair (small, silkscreen < > arrows)
- Function-group frame (thin border + silkscreen title, JD-990 style)
- Mode toggle (two latching buttons or one rocker, mutually exclusive lit)
- LCD list-overlay page (waveform browser)
- Small round LED (mode/partial-active indicators)

## 5. Typography & silkscreen

- Panel silkscreen: condensed grotesque, all-caps, slightly tight tracking
  (the JD/E-MU label look). One weight for group titles, one smaller for
  parameter labels.
- LCD: authentic 5x7-dot character font. Never use the LCD font for panel
  silkscreen or vice versa.
- Logo "THE DREAMER": deserves its own exploration — candidates: 90s
  italic-tech wordmark (Roland "JD" energy), or softer script over a
  moon/star motif for the dream house angle. Explore both against refs.

## 6. Color tokens (v2 — Supernova/Audity direction)

    panel-bg        #232850   blue-purple cosmic faceplate
    panel-header    #1b1f40   darker top strip
    group-frame     #464e94   function-group borders
    silkscreen      #9aa1d8   labels
    silkscreen-hi   #c9cdf2   titles / button legends
    led-on          #ff2b3e   bright red, lit
    led-off         #4a1020   dark maroon, unlit
    knob-outer      #14162c   knob skirt
    knob-body       #2a2d48   knob cap
    pointer-white   #e8eaff   general knob pointers / slider caps
    pointer-red     #ff5b6e   filter-section knob pointers
    lcd-bg          #07070a   all displays
    lcd-ink         #ffd23f   bright yellow text
    slider-track    #0c0e20

Rules: red appears ONLY as LEDs and filter pointers; yellow ONLY inside
LCD glass; no gradients on the faceplate in the flat concept — the Figma
pass may add subtle brushed-metal texture per Supernova photos.

## 7. Sizing & tech constraints

- Fixed-ratio plugin window, target ~980x560 logical px, 2x assets for HiDPI.
- JUCE rendering: prefer filmstrip/atlas or vector-drawn components mirroring
  the Figma masters; LCD as a drawn pixel-grid (cheap, crisp at any scale).
- All controls must map 1:1 to APVTS parameters in the CLAUDE.md phase-5
  plan; no UI-only state except the waveform-list overlay.

## 8. References

- [x] ref-01 — Novation Supernova 2 front panel: PRIMARY for colors,
      lights, LED behavior (blue metal, red LEDs, cream silkscreen)
- [x] ref-02 — E-MU Audity 2000 front panel: SECONDARY inspiration
      (purple cosmic panel graphics, encoder layout)
- [x] ref-03 — user directive: black LCD, bright yellow text
- [ ] ref-04+ — user reference images (pending)

*When references arrive: re-validate pillars 3 (second accent), the display
color, the logo direction, and layout density. References win over this doc.*

## 9. Resolved & open questions

RESOLVED by user direction: accent = red LEDs on blue-purple panel;
display = yellow-on-black; envelopes = slider banks (knobs + sliders mix).

Still open:
1. Logo: italic serif wordmark (current concept) vs Supernova-style
   tech type vs Audity-style cosmic graphic?
2. Faceplate texture in Figma: flat color vs subtle brushed metal?
3. Mode 1 (RHINO) page: same layout with Partial B swapped for mod
   routing, or a visually distinct sub-panel?
4. Star-field / cosmic graphic motif on the panel (Audity-style art)
   — yes/no?
