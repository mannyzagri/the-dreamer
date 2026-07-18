# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. The 2026-07-18 open loop
> (waiting for a corrected GUI handoff) is CLOSED — handoff GUI_4 (v11) is
> integrated and shipped as **RC 1.0.0**. This file now tracks the current
> open loop only.

## Where things stand (deployed, green)

- **RC 1.1.0** built, validated (19/19 all stages incl. pluginval 8),
  deployed to `C:\the-dreamer\The Dreamer.vst3` AND `\\VBOXSVR\vagrant\The
  Dreamer.vst3` (share root). Committed + pushed + share `-src` refreshed.
  1.1.0 implements the V1.1 global filters (types 7-12); **no param change vs
  1.0.0** (moduleinfo byte-identical) — reload the instance / restart Cubase.
- Face is design-handoff **GUI_4 (v11)**: 1140×864 keyboard/wheels, per-tone
  LFO SHAPE LCDs, whole-frame resize, screw clearances, VER stamp. (GUI_4 on
  the share is now **v12** — pending integration; see below.)

## The four GUI_4 items — RESOLVED in 1.0.0
1. **Labels behind the corner screws** → header logo/POWER given 26px screw
   clearances; screws on their own layer; nothing occluded.
2. **Panel duplicity** → single clean panel rebuilt from the prototype.
3. **Resize letterbox** → faceplate finish + 4 screws now live INSIDE the
   single scaled `#frame`; fit() scales the whole 1140×864 frame as one unit
   (C++ aspect 1140:864, limits 570×432..2280×1728). No letterbox.
4. **Version label** → `VER <version>` bottom-right (right:34/bottom:16),
   left of the corner screw, populated from getInfo().version.

## Filter investigation (RC 1.1.0, 2026-07-18) — READ before touching filters

User: "except LP/BP/HP/LADDER, no filter modes work, parallel or serial."
Measured with 3 cl.exe probes driving `dsp/glue/GlobalFilter.h` EXACTLY like
the processor (setType each block, setCutoffRes at gfCtrl rate, res=0 default,
real cutHz mapping). Findings, rock-solid and REPRODUCIBLE:
- LP24/LP12/BP/HP work. **LIQUID/CLASSIC filter hard AND track cutoff/res**
  (4 kHz throughput 0.72→0.02→0.00 as CUT closes; passband unity 0.71).
- **LADDER is the genuinely weak one** (barely closes; res runs backwards) —
  this is the AUTHENTIC rubber-rhino "filter-mod-off" ladder (byte-locked
  port, rule 1 — its math is not ours to retune).
- Types **7-13 were bypass placeholders** = the real "most modes do nothing".

Resolved in 1.1.0: implemented 7-12 (`dsp/glue/FilterExtra.h`), DreamPln(13)
V2 bypass. Did NOT add LIQUID/CLASSIC makeup — measurement says unity passband;
adding gain would clip. The user's "LIQUID/CLASSIC never filter, stay bright"
is INVERTED from every measurement (their symptom matches LADDER, not
liquid/classic) and could not be reproduced. All GUI combo wiring / param
binding / 14-type mapping / cutoff drive are correct in source. Most likely a
**WebView2 host cache** serving a stale editor (a Cubase *rescan* does NOT
clear it — only a full Cubase restart / clearing the WebView user-data temp).
NEXT: if the user still reports it after a FULL restart on 1.1.0, it is a real
binary issue — get exact SER/PAR + F1/F2 types + CUT/RES positions and trace
the actual param values reaching the processor (consider a temp on-screen
readout of the engine-resolved filter type).

## Pending integrations (design track dropped these)
- **GUI_4 is now v12** (share README, 18:33): adds a "rubber band" separator
  strip, a collapsible keyboard (▼/▲ KEYS fold button; collapsed 664 / expanded
  864, host resizes with it), and a pitch-wheel red center stripe + inverted
  drag (down = bend up). Deployed face is still v11 — integrate v12 via
  frontend-developer (gui-pass). Filter section unchanged.
- **Sounds at `\\VBOXSVR\vagrant\The Dreamer\files (5)`** (user pointer) — real
  factory presets to replace the app.js placeholders. Not yet read.

## Open loop now — the user's Cubase pass on 1.1.0
Install on the HOST first (the recurring "old GUI" cause is host-side, NOT a
deploy miss): copy the whole `\\VBOXSVR\vagrant\The Dreamer.vst3` bundle into
`C:\Program Files\Common Files\VST3\`, overwrite, then Cubase → full plugin
remove/re-add the instance (**restart Cubase fully** — the WebView2 cache is
the recurring "behaves like an old build" cause, and it's the prime suspect
for the LIQUID/CLASSIC filter report). Tell for 1.1.0: **VER stamp 1.1.0**.

Then ear pass on the FILTERS (the point of 1.1.0): audition NOTCH / COMB+ /
COMB− / N+LP / FORMANT / ALLPASS on the main filters — they now do something.
Re-check LIQUID/CLASSIC after the full restart (measurement says they filter;
see the Filter investigation section above). Also still: play keys + wheels,
A/B per-tone LFO SHAPE, drag-resize the window.

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
