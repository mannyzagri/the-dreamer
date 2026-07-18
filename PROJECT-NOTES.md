# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-18, RC 1.1.0 — V1.1 global filters implemented, validator 19/19 PASS)

> History (0.1.0 … 1.0.0) lives in CHANGELOG.md. This block is CURRENT-ONLY.

| Item | Value |
|---|---|
| Deployed version | **1.1.0 RC (V1.1 global filters)** — `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root); validator 19/19 PASS all stages (pluginval 8). **No param-list change vs 1.0.0** (filter choices already existed) → moduleinfo byte-identical; user still reloads the instance / restarts Cubase to pick up the new binary + clear WebView cache |
| 1.1.0 additions | **Global filter types 7-12 now DO something** (were bypass placeholders): NOTCH, COMB+, COMB−, N+LP, FORMANT, ALLPASS — new glue `dsp/glue/FilterExtra.h` (RBJ biquads for notch/allpass/N+LP-LP, delay-line comb for COMB±, 3-resonator vowel FORMANT w/ 2.2× makeup), routed from GlobalFilter for type 7-12. DreamPln(13) stays V2 bypass. Also: the Rhino path (LIQUID/CLASSIC/LADDER) now goes through the same output `safety()` clamp (was unclamped — latent runaway risk). test_filter_extra harness added. **LIQUID/CLASSIC measured UNITY passband (0.71 in/out) and filter correctly — NO makeup added** (would clip); the user's "they never filter" could not be reproduced in 3 probes (see breakpoint.md) |
| 1.0.0 additions | On-screen keyboard + pitch/mod wheel strip (panel 1140×864), functional via editor native fns → lock-free juce::AbstractFifo SPSC → drainGuiMidi at top of processBlock; per-tone LFO SHAPE (+8 params `lfo{1,2}_shape_[t]`, default SIN bit-neutral); v11 face fixes (whole-frame resize, screw clearances, single panel, VER stamp). See CHANGELOG. |
| Bank | **114 waves** = 78 cycles + 26 Ens loops (78-103) + 10 Shot (104-113). LoopBankData.h = 6.13 MB int16. rompler-bank-v2 (the 78 cycles) was already in since v1. |
| Contracts | design-handoff/v8 DSP_BUILD/FEATURES §11 is newest DSP; GUI now GUI_4 **v12** (README on the share — rubber-band strip, collapsible keyboard, pitch-wheel red stripe + inverted drag) — NOT yet integrated (deployed face is v11). ~250 params |
| Engine | v3+v7 base + FX v1.5 (modfx 7 modes +Dimension/Rotary/Barberpole; LO-FI/WIDTH/TALK stages; per-slot PARAMS p0..p3; dly/rev extras RESERVED/inert; fx_prepost; meters) + **V1.1 global filters (all 14 slots now audible except DreamPln)**. Bus order + RT-safety per architect review |
| GUI | **v11 face deployed** (design-handoff GUI_4 v11): 1140×864, functional keyboard/wheels, per-tone LFO1/LFO2 SHAPE LCDs, whole-frame resize, screw clearances, VER stamp. GUI_4 on the share is now **v12** (pending integration). MIDI LEARN still non-functional (deferred) |
| ⚠ Preset payloads | 24 factory patch payloads still AUTHORED placeholders (app.js) — real sounds pending; user says sound assets are at `\\VBOXSVR\vagrant\The Dreamer\files (5)` (not yet read/integrated) |
| Flagged deviations | dly/rev PARAMS reserved/inert (ports); FXPARAM matrix dest DEFERRED; MIDI learn deferred; WIDTH fixed POST; DreamPln(13) V2 bypass; FilterExtra ALLPASS is a 50/50 dry-mix phase-notch (a bare allpass is inaudible on sustained tone — deliberate voicing); + carried v3/v7 (tvf_env/flt1_env unipolar, loop-seam body-relative, flt_bal PAR-only) |
| Pending | **user Cubase ear pass on 1.1.0** (audition NOTCH/COMB±/N+LP/FORMANT/ALLPASS; re-check LIQUID/CLASSIC after a FULL Cubase restart — clears WebView cache); integrate GUI_4 **v12**; author real presets from files (5); DreamPln (V2); MIDI learn wiring; dly/rev PARAMS wiring |
| Git | committed via branch → merge --no-ff → main → push; share the-dreamer-src\ export refreshed |

### Phase-gate checklist (every phase, before merge)
- [ ] cl.exe test harness(es) for the phase compile and print ALL CHECKS PASSED
- [ ] No new warnings introduced (plugin phases: Release x64 build clean)
- [ ] Ported files diff vs donor = namespace rename only (audit with fc/diff)
- [ ] STATE block updated (phase, git state)
- [ ] Explicit `git add` file lists only — NEVER `git add -A` (concurrent sessions)

## Facts a fresh session needs

- Validator (2026-07-18): `powershell -ExecutionPolicy Bypass -File
  C:\code-bank\validator\validate.ps1 -Project C:\the-dreamer` runs the test
  harnesses + bundle/deploy checks + pluginval strictness 8 in one command
  (config: `validator.json`; pass tools + tools-first policy:
  CLAUDE-WORKFLOW.md §7.7). pluginval is kept at C:\Utilities\pluginval\.
  NOTE two shared-tool gotchas hit this session — see memory
  [[release-ship-tool-gotchas]] (release.ps1 -DryRun leaves CMakeLists bumped;
  ship.ps1 push fallback hardcodes origin main → drive git by hand on branches).

- Donor: C:\rubber-rhino (READ-ONLY). Bank source of truth: files\rompler-bank-v2\
  (copied verbatim to dsp\bank\ — READ-ONLY, 12-bit grain is data).
- deps\JUCE = JUCE 8.0.4 source cache (gitignored; robocopy from
  C:\rubber-rhino\deps\JUCE on a fresh clone — robocopy needs BACKSLASH paths).
- Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/the-dreamer/deps/JUCE`
- Tests: `cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_X.cpp /Fo:tests\bin\ /Fe:tests\bin\test_X.exe`
  (add /DRHINO_TEST_FEATURES=1 for anything including ported FX; run vcvars64 first).
  Harnesses set FTZ/DAZ themselves (`_mm_setcsr(_mm_getcsr() | 0x8040)`).
- Global filters: 0-3 built-in SVF (ToneSvf), 4-6 Rhino ports (RhinoFilter, via
  GlobalFilter), **7-12 new glue FilterExtra**, 13 DreamPln bypass (V2). All
  driven cutoff(Hz)+res(0..1) at gfCtrl control rate; GlobalFilter.process()
  clamps every path through safety().
- Known bank defect (fix lives in glue only): Mode2Synth steal-stamp bug — see
  CLAUDE.md scope decision 2.
- EnvelopeAdsr release is exponential with a 1e-4 idle gate: a 1.2 s release
  needs ~11 s to reach silence — size tail-silence test windows accordingly.
- Deploy: whole "The Dreamer.vst3" bundle → \\VBOXSVR\vagrant\ root
  + C:\the-dreamer\; refresh share the-dreamer-src\ export after each push.
- v2 engine facts: waveshaper LUTs are BAKED — edit tools/bake_shapers.cpp
  then rerun it to regenerate dsp/glue/ShaperTables.h (no python on the VM,
  by design). Matrix param choices have NO "-" entry (GUI parity): engine enum
  = param index + 1, slot inert at amt 0.

## STATUS log

Moved to **CHANGELOG.md** (newest first). The STATE block above is
current-only per the docs-hygiene rule.
