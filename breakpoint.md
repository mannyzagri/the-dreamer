# BREAKPOINT — The Dreamer (for the next session)

> Read this AFTER PROJECT-NOTES.md STATE + CLAUDE.md. It captures the open
> loop at the end of the 2026-07-18 session. **The user said DO NOT REBUILD**
> in the session that wrote this — a corrected GUI handoff from the design
> track (Claude Design) is expected first. Wait for it, then integrate per
> the gui-pass skill.

## Where things stand (deployed, green)

- **RC 0.7.1** is built, validated (all stages incl. pluginval 8), deployed,
  committed to `main`, pushed, share `-src` export refreshed.
- Deployed bundle: `\\VBOXSVR\vagrant\The Dreamer.vst3` (share root) AND
  `C:\the-dreamer\The Dreamer.vst3`. **Verified 2026-07-18**: the share copy
  is byte-identical to the fresh build and contains the v9 GUI (markers
  `uiMeters`, `N+LP`, batch2 loop `OPERA_VIOLA`, zero `NL3`). It is the ONLY
  `The Dreamer.vst3` on the share.
- Design as of this session: `design-handoff/v9/` == the share's `GUI_3` drop
  (byte-identical README + PNG). No newer design had arrived yet.

## The "old GUI in Cubase" situation — DIAGNOSED

The share build is current and correct; the old-GUI the user sees is
**host-side**, NOT a deploy miss. The share root `.vst3` is a drop-off;
Cubase loads from the host's own VST3 folder (`C:\Program Files\Common
Files\VST3\`). Remedy the user must do on the HOST:
1. Copy the whole bundle folder `\\VBOXSVR\vagrant\The Dreamer.vst3` into
   `C:\Program Files\Common Files\VST3\`, overwriting the old one.
2. Cubase: full plugin re-scan (param list changed), remove/re-add instance.
3. If still old: fully restart Cubase (WebView2 user-data cache in temp).
Tells for the new build: header stamp **0.7.1**, DELAY row has a **SYNC**
btn, filter menu shows **N+LP** (not "NL3 N+LP"), 114-entry wave list.

## OPEN GUI ITEMS the user gave to Claude Design (expect a new handoff)

The user's verbatim asks to the design track (paraphrased into actionable
work for whoever integrates the next handoff):

1. **Labels behind the corner screws / "old gui with labels behind the
   screwdrives".** The corner screws (faceplate finish) render on a layer
   that sits over panel labels, OR content slides under the fixed screws on
   resize (see #3). Fix: z-order/layout so no label is occluded by a screw;
   the version stamp especially (see #4). Likely `plugin/gui/style.css`
   z-index of `.screw`/faceplate vs labels, and/or the resize structure.

2. **Panel duplicity in the handoff — dedupe.** The handoff prototype/markup
   may contain a duplicate panel; keep only the current previewed state.
   When the corrected handoff lands, diff it against `design-handoff/v9/` and
   confirm the duplication is gone before integrating.

3. **Resize behavior (Cubase): the outer frame/corners stay fixed and the
   layout resizes INTO it** (letterbox), instead of the WHOLE frame scaling
   as one unit. Root-cause hypothesis for the next session: the screws /
   faceplate-finish are drawn on a FIXED-size outer container while only the
   inner panel content is scaled by the page `fit()` transform — so the
   corners don't move with the content and content can slide under them
   (ties to #1). Fix direction: put the faceplate finish + screws INSIDE the
   single scaled `#frame` so `transform: scale(...)` scales everything
   together (screws included). Editor side is already correct:
   `setResizable(true,true)` + `setFixedAspectRatio(kBaseW/kBaseH)` + limits
   50–200% (PluginEditor.cpp); the fix is in the PAGE structure
   (editor.html/style.css/app.js `fit()`), a frontend-developer task.

4. **Add a version-number label at the lower-right, near the corner screw.**
   The `getInfo` native fn already returns `{version, build}` (PluginEditor.cpp);
   a `v<version>` stamp existed bottom-right in earlier revs — reposition/add
   it clearly near the bottom-right screw and make sure it's NOT occluded by
   the screw (#1). Should read `0.7.1` (or the built version) in the plugin.

## Fixes already shipped in 0.7.1 (do NOT redo)

- Global-filter resonance/BAL overflow → `GlobalFilter` output safety net
  (dsp/glue/GlobalFilter.h): peak 2.57→1.49, transparent <0.8.
- Reverb click + ROOM/HALL huge gain → `kDryScale` 2.0→1.0
  (PluginProcessor.cpp reverb block): dry continuous with bypass.
- `NL3 N+LP` → `N+LP` (Params.h filterTypes + app.js FTYPES).
- v9 `dly_sync`: tempo-synced delay time (12-div table off host BPM) +
  GUI SYNC btn/division label.
- "Filter 1 not applying" was **NOT reproduced** — DSP proven (F1 LP24@300Hz
  kills 6kHz, test_fx_v15 [global-filter]) AND GUI binding proven correct
  (F1 CUT→flt1_cut, distinct from F2). Likely was masked by the now-fixed
  overflow. If the user still reports it on 0.7.1, get the exact state
  (SER/PAR, F2 settings, BAL position) before touching anything.

## Known gaps / deferred (not bugs — flagged design decisions)

- **Per-tone LFO SHAPE**: the v9 design shows a shape mini-LCD (TRI/SIN/SAW…)
  on each tone LFO1/LFO2 row, but the engine's per-tone LFOs are FIXED SINE —
  there is no `lfo1_shape_[t]`/`lfo2_shape_[t]` param, so my build omits the
  control (nothing to bind). To honor the design, add per-tone LFO shape
  params (dreamer::ToneParams::ToneLfo already has an unused `shape` field →
  DreamVoice would need to apply it via a per-tone Lfo shape; the Lfo class
  supports shapes). Small engine + param + GUI addition. FLAG to user before
  doing — it's a param-count change (Cubase re-scan).
- **dly/rev PARAMS extras (p0..p3) RESERVED/inert** — ported StereoDelay /
  juce::Reverb can't take extra knobs under rule 1 (need non-ported
  replacements for TAPE wow/flutter, reverb predelay/etc.).
- **MIDI LEARN** button is a visual no-op (deferred; user chose meters+resize).
- **FXPARAM** matrix dest deferred (no slot-selector/pfocus stepper in GUI).
- **24 factory presets** = authored placeholders in app.js (real sound design
  pending from the design track).
- **Filter types 8-13** (Notch/Comb±/N+LP/Formant/Allpass) bypass [V1.1];
  DreamPln [V2].

## How to work here (toolchain)

- Skills: `dsp-pass`, `gui-pass` (the protocols). Personas via general-purpose
  agents that read+adopt `C:\Users\vagrant\.claude\agents\<name>.md`
  (frontend-developer for GUI, cpp-pro/architect-reviewer for DSP; note:
  memory says Fable keeps DSP, delegates GUI to frontend-developer).
- Tools: `C:\code-bank\validator\validate.ps1 -Project C:\the-dreamer
  [-Stages ...]`; `tools\release.ps1 -BumpTo X -Forbid Y [-DryRun][-SkipHost]`;
  `tools\ship.ps1 -Project C:\the-dreamer -Files <explicit> -Message ...`
  (ship commits on current branch + pushes + refreshes share `-src`; it
  archives `main`, so merge to main first).
- Git: branch-per-phase → merge --no-ff to main → push. Explicit `git add`
  lists only (concurrent sessions). LF→CRLF warnings are benign.
- Validator config: `validator.json` (dsp harness list = ground truth;
  staging BinaryData audit; gui load+screenshot; bundle build block;
  srcExport for ship).

## Next-session first moves

1. Check the share for a NEW GUI handoff folder (the corrected one addressing
   items 1–4 above) — likely under `\\VBOXSVR\vagrant\The Dreamer\GUI_*` or
   `files*`. Diff its README/prototype against `design-handoff/v9/`.
2. If it addresses the 4 items, integrate via frontend-developer (gui-pass):
   dedupe panels, fix screw/label z-order + resize-scales-whole-frame, add the
   bottom-right version label. Decide with the user whether to also add
   per-tone LFO shape (engine change).
3. Then release (bump 0.7.2 or 0.8.0), full validator, ship, and give the user
   the install steps (host VST3 folder copy + re-scan) — the recurring cause
   of "old GUI".
