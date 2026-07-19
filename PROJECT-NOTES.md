# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-19, RC 2.0.2 — loop tuning + gain staging + GUI regenerated from handoff, validator all stages PASS + pluginval 8)

> History (0.1.0 … 2.0.1) lives in CHANGELOG.md. This block is CURRENT-ONLY.

| Item | Value |
|---|---|
| Deployed version | **2.0.2 RC (per-loop tuning + gain-staging clip fixes + GUI regenerated from v13 handoff)** — binary literal `2.0.2`, `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root), build↔local↔share SHA-identical. Validator PASS all stages (dsp 10/10 incl. loop_tuning+gain_staging, staging/bundle/gui/deploy/host pluginval 8). **NO param-list change vs 2.0.0/2.0.1** → just reload the instance / restart Cubase (no re-scan). Coming from 1.2.0 or earlier still needs the 2.0.0 FULL RE-SCAN. |
| 2.0.2 changes | **DSP (R&D files(7), rule-1-safe glue only):** (1) per-loop measured rootHz via dsp/bank/LoopRoots.h (130, from loop_roots.json; METAL/metal-morphs stay 220) → fixes ±36–45¢ loop detune; Bank3 uses kLoopRoots[i]. (2) Gain staging (fixes preset/reverb clipping): per-voice tone-sum ÷√(active tones) smoothed (⚠ **4-tone patches now noticeably QUIETER by design** — lever if too quiet = `kVoiceHeadroom` 0.5 in PluginProcessor); delay + reverb mixes → proper crossfades; delay fb cap 0.9; OutputStage.h tanh soft-clip + −0.1 dBFS ceiling after master. Resonant-filter make-up trim NOT added (would break bitwise null test; live path self-limits — RhinoFilterSlot reverted clean; **if resonant clipping persists by ear, authorize a 1-line res-comp in GlobalFilter**). **GUI: REGENERATED from the v13 handoff master (not renovated)** — fresh reproduction from the master geometry (inline-style arch; style.css 24→2.7 KB; editor.html minimal shell + check_native_interop.js classic script), JUCE bridge as fixed plumbing, relay parity EXACT 281 ids vs PluginEditor.cpp. |
| ⚠ Scope (2026-07-19) | GUI DESIGN is GUI-Claude's, DSP direction is R&D-Claude's; THIS session = architecture + DSP + VST3 build. GUI is **REGENERATED from the handoff, never renovated** ([[gui-work-via-frontend-developer]]). |
| 2.0.0 additions | (1) **Full v3 library**: 26 "Ens" placeholder loops + 10 synth "Shot" → **130 family loops** (Pad/Airy/Vox/Ether/FM/Wind/Metal/Morph) + **10 HIT** one-shots. Bank **218** (78 cycle + 130 loop + 10 hit). WAV→header bakers: tools/bake_loops_header.cpp (130 manifest-order names, per-family category, LoopEntry now carries category) + tools/bake_shots_header.cpp (new). Sample arrays `inline const` (not constexpr) → ~102 MB LoopBankData.h compiles ~21 s. 12-bit invariant holds (14.87 M samples). (2) **New per-tone DSP**: §11 VOICING multi-tap osc (SINGLE/OCT/POWER/DREAMY + dreamy_spread ADD9/MIN7/SUS2, equal-power tap sum, SINGLE bit-identical); §12 LOOP MODE FWD/PINGPONG (PcmOsc3 phase reflection); §13 HIT STRETCH varispeed (hit_play NORMAL/STRETCH, hit_stretch 0.25×–4× log, hit_pitchtrim ±24 st). (3) **v13 faceplate** (design-handoff/v13, WebView rebuilt wholesale). |
| Bank | **218 waves** = 78 cycles (0-77) + 130 loops (78-207, per-family category tags) + 10 HIT shots (208-217). LoopBankData.h ≈ 102 MB text / 28 MB int16; ShotBankData.h ≈ 0.8 MB. WAV source in assets/loops (130) + assets/shots (10); bake via tools/bake_*_header.cpp (no python). |
| Contracts | library1.zip DSP_BUILD §11/12/13 is newest DSP (INTEGRATED); GUI = design-handoff/**v13** (INTEGRATED). ~276 params |
| Engine | v3+v7 + FX v1.5 (modfx 7 modes; LO-FI/WIDTH/TALK; per-slot PARAMS p0..p3; fx_prepost; meters) + V1.1 global filters (14 slots, DreamPln bypass) + **§11 voicing / §12 loop-mode / §13 hit-varispeed (per tone)**. Tone now owns 4 osc taps (voicing fans osc only; chain downstream shared). Matrix dest 9 (FX PARAM reserved/inert). |
| GUI | **v13 face deployed**: locked 1140×660 (collapsible 848, collapsed default; PluginEditor kBaseH=848/kFoldedH=660, aspect 1140/660, limits 570,330,2280,1320), full-word labels, STRETCH/P.TRIM greyed IN PLACE, LOOP↔PLAY selector swapped by wave bank type, VOICING+DREAMY steppers, LFO1/LFO2 tempo SYNC, BALANCE knob, 9-slot matrix bipolar bars, FX focus-LCD + PARAMS knobs, UTIL overlay, 218-wave overlay ([ENS]/[SHOT] tags). Bridge preserved (getSliderState/getToggleState relays, getNativeFunction getInfo/keyboardFold/noteOn, check_native_interop). MIDI LEARN rendered non-functional (deferred). **2.0.1: prototype resize grip REMOVED — resize ONLY via the host window border (fit() scale derived solely from window size); FX group overflow fixed (buttons/UTIL no longer clipped at panel edge).** |
| ⚠ Preset payloads | factory patch payloads still AUTHORED placeholders (app.js) AND now point into the new 218-order → sound wrong; real authoring pending. Sound assets on share: `\\VBOXSVR\vagrant\The Dreamer\files (5)` / `files (6)` (not yet integrated). |
| Flagged deviations | FX-PARAM matrix dest reserved/inert (no GUI focus target yet); MIDI learn deferred; hit_stretch NOT a matrix dest (§9 wins, lists FX PARAM); STRETCH detaches one-shot from key pitch (authentic varispeed); dly/rev PARAMS reserved/inert; WIDTH fixed POST; DreamPln V2 bypass; loop-seam report-only (delivered material can't meet literal seam criterion — PINGPONG is the remedy); carried tvf_env/flt1_env unipolar. |
| Pending | **user Cubase ear+eye pass on 2.0.0** (FULL re-scan first): audition the 130 loops + 10 HITs; VOICING modes + DREAMY spreads; LOOP MODE pingpong on loops; PLAY MODE STRETCH + PITCH TRIM on HIT waves; v13 face + fold; confirm LFO SYNC. Then: author real presets from files(5)/files(6); FX-PARAM matrix wiring (needs GUI focus target); MIDI learn wiring; DreamPln V2. |
| Git | **Pushed to main via Git LFS** (branch port/v2.0.0-library-gui-dsp → merge --no-ff → main → push; origin `3ffb800`). The sound library is tracked by **Git LFS** (`.gitattributes`: `assets/loops/*.wav`, `assets/shots/*.wav`, `dsp/bank/LoopBankData.h`, `dsp/bank/ShotBankData.h`) — 142 LFS objects, ~137 MB, because `LoopBankData.h` (~102 MB) exceeds GitHub's 100 MB direct-push limit. A fresh clone needs `git lfs pull` to get the real bank data before building. Share the-dreamer-src\ export refreshed with REAL files via working-tree robocopy (NOT `git archive`, which would export LFS pointer stubs). |

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
