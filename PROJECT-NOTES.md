# The Dreamer â€” PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass â€” never append a second STATE block.

## STATE (updated 2026-07-18, RC 1.0.0 â€” v11 GUI: keyboard/wheels + per-tone LFO shape, validator all-stages PASS)

> History (0.1.0 â€¦ 0.7.1) lives in CHANGELOG.md. This block is CURRENT-ONLY.

| Item | Value |
|---|---|
| Deployed version | **1.0.0 RC (v11 GUI + keyboard/wheels + per-tone LFO shape)** â€” `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root); validator 18/18 PASS all stages (pluginval 8). Param-list change vs 0.7.1 â†’ Cubase FULL re-scan |
| 1.0.0 additions | (1) **On-screen keyboard + pitch/mod wheel strip** (panel 1140Ã—660â†’**1140Ã—864**), FUNCTIONAL: keysâ†’noteOn/off, PITCHâ†’bend (Â±2 st, springs to 0), MODâ†’CC1-equiv, all via editor native fns â†’ a **lock-free juce::AbstractFifo SPSC** drained at the top of processBlock (architect-reviewed; honours rule 5, no audio-thread lock). Editor dtor calls guiAllNotesOff() (stuck-note guard). (2) **Per-tone LFO SHAPE** â€” +8 params `lfo{1,2}_shape_[t]` (TRI/SIN/SAW/SQR/S+H), applied in DreamVoice via the shared panelâ†’Lfo::Shape map; default SIN is bit-identical to the pre-v11 fixed-sine engine. (3) v11 face fixes: whole-frame uniform resize (1140:864, no letterbox), header z-order/screw clearances, single panel, bottom-right VER stamp from getInfo(). |
| Bank | **114 waves** = 78 cycles + **26 Ens loops** (v1's 16 at indices 78-93 held stable + v2 batch2's 10 string/choir/vox at 94-103) + 10 Shot (104-113). Loops baked from assets/loops/*.wav by tools/bake_loops_header.cpp; recipes tools/bake_loops.py + bake_loops2.py. LoopBankData.h = 6.13 MB int16 (exceeds Â§1's <5 MB soft target â€” consequence of 26 loops; not blocking). NOTE: "bank2"/rompler-bank-v2 (the 78 cycles) was already fully in since v1 â€” the share copy only differed by CRLF. |
| Contracts | design-handoff/v8/ (DSP_BUILD/FEATURES Â§11 + GUI_2 README) is newest; supersedes v7 where it conflicts. ~250 params â†’ Cubase FULL re-scan |
| Engine | v3+v7 base + **FX v1.5**: modfx 7 modes (+Dimension/Rotary/Barberpole; extras p0..p3 drive them), standalone LO-FI/WIDTH/TALK stages (host-automatable only, not on panel), per-slot PARAMS (modfx/dly/rev p0..p3+pfocus; **dly/rev extras RESERVED/inert** â€” ports can't take knobs under rule 1), fx_prepost (LO-FI PRE/POST; WIDTH fixed POST), output peak meters. Bus order + RT-safety per architect review (see CLAUDE.md v1.5 decisions) |
| GUI | **v11 face** (design-handoff GUI_4): 1140Ã—864, keyboard+pitch/mod wheel strip (functional), per-tone LFO1/LFO2 **SHAPE** LCDs (bound `lfo{1,2}_shape_[t]`), whole-frame uniform resize 50â€“200% (screws inside the scaled #frame â€” no letterbox), header logo/POWER 26px screw clearances, bottom-right VER stamp from getInfo(). Carries v8: MIDI LEARN (**NON-FUNCTIONAL â€” deferred**), L/R meters (window.uiMeters 30 Hz), FX rows RATE/DEPTH/**PARAM**/MIX, modfx 7-mode stepper. (Handoff GUI_SPEC.md is internally stale â€” its single-LFO row + 660 resize section are superseded by its own README/prototype; followed the README/PNG.) |
| âš  Preset payloads | 24 factory patch payloads still AUTHORED placeholders (app.js) â€” real sound design pending from the design track |
| Flagged deviations | dly/rev PARAMS reserved/inert (ports); FXPARAM matrix dest DEFERRED (no slot-selector in v8 matrix); MIDI learn deferred (button is a visual no-op); WIDTH fixed POST (F4); + carried v3/v7 (tvf_env/flt1_env unipolar, loop-seam body-relative, flt_bal PAR-only) |
| Pending | **user Cubase ear+eye pass on 1.0.0** (play the on-screen keyboard + wheels; A/B each per-tone LFO SHAPE; drag-resize the window corner-to-corner â€” should scale as one unit, no letterbox, no label under a screw); real factory presets; MIDI learn wiring; dly/rev PARAMS wiring (needs non-ported delay/reverb); FXPARAM matrix dest + pfocus GUI; V1.1 filters; V2 DreamPlane |
| Git | committed via ship.ps1; share the-dreamer-src\ export refreshed |

### Phase-gate checklist (every phase, before merge)
- [ ] cl.exe test harness(es) for the phase compile and print ALL CHECKS PASSED
- [ ] No new warnings introduced (plugin phases: Release x64 build clean)
- [ ] Ported files diff vs donor = namespace rename only (audit with fc/diff)
- [ ] STATE block updated (phase, git state)
- [ ] Explicit `git add` file lists only â€” NEVER `git add -A` (concurrent sessions)

## Facts a fresh session needs

- Validator (2026-07-18): `powershell -ExecutionPolicy Bypass -File
  C:\code-bank\validator\validate.ps1 -Project C:\the-dreamer` runs the test
  harnesses + bundle/deploy checks + pluginval strictness 8 in one command
  (config: `validator.json`; pass tools + tools-first policy:
  CLAUDE-WORKFLOW.md Â§7.7). pluginval is kept at C:\Utilities\pluginval\.

- Donor: C:\rubber-rhino (READ-ONLY). Bank source of truth: files\rompler-bank-v2\
  (copied verbatim to dsp\bank\ â€” READ-ONLY, 12-bit grain is data).
- deps\JUCE = JUCE 8.0.4 source cache (gitignored; robocopy from
  C:\rubber-rhino\deps\JUCE on a fresh clone â€” note robocopy needs BACKSLASH paths).
- Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/the-dreamer/deps/JUCE`
- Tests: `cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_X.cpp /Fo:tests\bin\ /Fe:tests\bin\test_X.exe`
  (add /DRHINO_TEST_FEATURES=1 for anything including ported FX; run vcvars64 first).
  Harnesses set FTZ/DAZ themselves (`_mm_setcsr(_mm_getcsr() | 0x8040)`).
- Known bank defect (fix lives in glue only): Mode2Synth steal-stamp bug â€” see
  CLAUDE.md scope decision 2.
- EnvelopeAdsr release is exponential with a 1e-4 idle gate: a 1.2 s release
  needs ~11 s to reach silence â€” size tail-silence test windows accordingly.
- Deploy: whole "The Dreamer.vst3" bundle â†’ \\VBOXSVR\vagrant\ root
  + C:\the-dreamer\; refresh share the-dreamer-src\ export after each push.
- v2 engine facts: waveshaper LUTs are BAKED â€” edit tools/bake_shapers.cpp
  then rerun it to regenerate dsp/glue/ShaperTables.h (no python on the VM,
  by design; ENVIRONMENT.md's python/Ninja items are satisfied by house
  equivalents, documented in CLAUDE.md v2 decision 6). Matrix param choices
  have NO "-" entry (GUI parity): engine enum = param index + 1, slot inert
  at amt 0. FX stages beyond the panel (dist/comp/clip/instability/spring/
  delay-sync) are OMITTED from the processor at bit-transparent defaults.

## STATUS log

Moved to **CHANGELOG.md** (newest first). The STATE block above is
current-only per the docs-hygiene rule.
