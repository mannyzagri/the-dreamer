# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. The 2026-07-18 open loop
> (waiting for a corrected GUI handoff) is CLOSED — handoff GUI_4 (v11) is
> integrated and shipped as **RC 1.0.0**. This file now tracks the current
> open loop only.

## Where things stand (deployed, green)

- **RC 1.0.0** built, validated (18/18 all stages incl. pluginval 8),
  deployed to `C:\the-dreamer\The Dreamer.vst3` AND `\\VBOXSVR\vagrant\The
  Dreamer.vst3` (share root). Committed + pushed + share `-src` refreshed via
  ship.ps1. Param-list change vs 0.7.1 → Cubase FULL re-scan.
- Face is design-handoff **GUI_4 (v11)**: 1140×864 with the on-screen
  keyboard + pitch/mod wheel strip, per-tone LFO SHAPE LCDs, whole-frame
  resize, screw clearances, VER stamp. See STATE + CHANGELOG 1.0.0 for detail.

## The four GUI_4 items — RESOLVED in 1.0.0
1. **Labels behind the corner screws** → header logo/POWER given 26px screw
   clearances; screws on their own layer; nothing occluded.
2. **Panel duplicity** → single clean panel rebuilt from the prototype.
3. **Resize letterbox** → faceplate finish + 4 screws now live INSIDE the
   single scaled `#frame`; fit() scales the whole 1140×864 frame as one unit
   (C++ aspect 1140:864, limits 570×432..2280×1728). No letterbox.
4. **Version label** → `VER <version>` bottom-right (right:34/bottom:16),
   left of the corner screw, populated from getInfo().version.

## Open loop now — the user's Cubase pass on 1.0.0
Install on the HOST first (the recurring "old GUI" cause is host-side, NOT a
deploy miss): copy the whole `\\VBOXSVR\vagrant\The Dreamer.vst3` bundle into
`C:\Program Files\Common Files\VST3\`, overwrite, then Cubase → full plugin
re-scan → remove/re-add the instance (restart Cubase if the WebView cache is
stale). Tells for 1.0.0: header/VER stamp **1.0.0**, an on-screen KEYBOARD at
the bottom, per-tone LFO1/LFO2 **SHAPE** LCDs.

Then ear+eye check: play the on-screen keys + wheels (pitch springs back, mod
holds); A/B each per-tone LFO SHAPE (default SIN = the old sound); drag-resize
the window corner-to-corner (scales as one unit; no label under a screw).

## Still deferred (flagged design decisions, not bugs)
- **MIDI LEARN** button — visual no-op (deferred).
- **dly/rev PARAMS extras (p0..p3)** — inert (ported StereoDelay/juce::Reverb
  can't take extra knobs under rule 1; need non-ported replacements).
- **FXPARAM matrix dest** — deferred (no slot-selector/pfocus stepper in GUI).
- **24 factory presets** — authored placeholders in app.js (real sound design
  pending from the design track).
- **Filter types 8-13** (Notch/Comb±/N+LP/Formant/Allpass) bypass [V1.1];
  DreamPln [V2].

## Toolchain note (unchanged)
Skills `dsp-pass` / `gui-pass`; validator `C:\code-bank\validator\validate.ps1
-Project C:\the-dreamer`; `release.ps1` / `ship.ps1`. Personas via
general-purpose agents that read+adopt `C:\Users\vagrant\.claude\agents\<name>.md`
(frontend-developer for GUI, architect-reviewer/cpp-pro for DSP).

**Tool bug seen this pass (flag for back-prop):** `release.ps1 -DryRun` leaves
CMakeLists bumped to the target version; a following real `release.ps1` run
then hands `validate.ps1` two separate `-Forbid` flags (previous == target),
which PS 5.1 rejects. Workaround used: reset the version to the old literal
before the real run. Fix in the tool: dedupe/array the forbid list, or don't
persist the -DryRun bump.
