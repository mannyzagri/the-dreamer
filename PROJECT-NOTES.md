# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-18, RC 0.3.0 — v2 engine + WebView GUI, validator 13/13 PASS)

| Item | Value |
|---|---|
| Deployed version | **0.3.0 RC (v2 engine + v5 WebView GUI)** — `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root), built 2026-07-18; validator PASS all stages (dsp×4, bundle, deploy×2, gui×2, pluginval strictness 8 incl. editor tests) |
| Engine | v2 per design-handoff/FEATURES.md CORE: 4 tones/voice (osc→shaper→SVF TVF→TVA, AUX env, G-LFO taps, pan), Dream Vector v4, 3-slot mod matrix, 2 global filters (frozen 14-type list, 7 live) SER/PAR, FX = modfx→delay→reverb. 24 voices |
| GUI | **WebView2 editor SHIPPED** (design handoff v5, pixel-faithful): plugin/gui/ editor.html+style.css+app.js + JUCE helpers + 4 committed fonts; ~185 relay bindings by APVTS ID; 1140×660 fixed-aspect; standalone-mock mode for headless-Chrome checks; MASTER knob = volume. No-panel params (penv start/end/time/loop, aux_amt, output, interp, engine) are host-automatable only — flag to design track for a future GUI rev (aux_amt especially: AUX env has no amount knob on the v5 panel) |
| Plugin identity | "The Dreamer", VST3, `Mnsh`/`Drmr` — param LIST changed vs 0.1.0 → Cubase FULL re-scan; WebView2 runtime needed on the host (evergreen, Cubase machine has it) |
| Current phase | gui/webview-v5 merged; awaiting user Cubase ear+eye pass |
| Pending | user Cubase pass; V1.1 filters (global list entries 8-13 currently bypass); V2 DreamPlane (f2_morph inert until then) |
| Git | main pushed to mannyzagri/the-dreamer; share the-dreamer-src\ export refreshed |
| Build plan | design-handoff/FEATURES.md §9 phases + gui-v5 README |

### Phase-gate checklist (every phase, before merge)
- [ ] cl.exe test harness(es) for the phase compile and print ALL CHECKS PASSED
- [ ] No new warnings introduced (plugin phases: Release x64 build clean)
- [ ] Ported files diff vs donor = namespace rename only (audit with fc/diff)
- [ ] STATE block updated (phase, git state)
- [ ] Explicit `git add` file lists only — NEVER `git add -A` (concurrent sessions)

## Facts a fresh session needs

- Validator (2026-07-18): `powershell -ExecutionPolicy Bypass -File
  C:\code-bank\validator\validate.ps1 -Project C:\the-dreamer` runs the 4 test
  harnesses + bundle/deploy checks + pluginval strictness 8 in one command
  (config: `validator.json`; see CLAUDE-WORKFLOW.md §7.7). pluginval is now
  KEPT at C:\Utilities\pluginval\ (the "not installed" note below is obsolete).

- Donor: C:\rubber-rhino (READ-ONLY). Bank source of truth: files\rompler-bank-v2\
  (copied verbatim to dsp\bank\ — READ-ONLY, 12-bit grain is data).
- deps\JUCE = JUCE 8.0.4 source cache (gitignored; robocopy from
  C:\rubber-rhino\deps\JUCE on a fresh clone — note robocopy needs BACKSLASH paths).
- Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/the-dreamer/deps/JUCE`
- Tests: `cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_X.cpp /Fo:tests\bin\ /Fe:tests\bin\test_X.exe`
  (add /DRHINO_TEST_FEATURES=1 for anything including ported FX; run vcvars64 first).
  Harnesses set FTZ/DAZ themselves (`_mm_setcsr(_mm_getcsr() | 0x8040)`).
- Known bank defect (fix lives in glue only): Mode2Synth steal-stamp bug — see
  CLAUDE.md scope decision 2.
- EnvelopeAdsr release is exponential with a 1e-4 idle gate: a 1.2 s release
  needs ~11 s to reach silence — size tail-silence test windows accordingly.
- Deploy: whole "The Dreamer.vst3" bundle → \\VBOXSVR\vagrant\ root
  + C:\the-dreamer\; refresh share the-dreamer-src\ export after each push.
- v2 engine facts: waveshaper LUTs are BAKED — edit tools/bake_shapers.cpp
  then rerun it to regenerate dsp/glue/ShaperTables.h (no python on the VM,
  by design; ENVIRONMENT.md's python/Ninja items are satisfied by house
  equivalents, documented in CLAUDE.md v2 decision 6). Matrix param choices
  have NO "-" entry (GUI parity): engine enum = param index + 1, slot inert
  at amt 0. FX stages beyond the panel (dist/comp/clip/instability/spring/
  delay-sync) are OMITTED from the processor at bit-transparent defaults.

## STATUS log (newest first)

- 2026-07-18 (later) — WebView GUI shipped (RC 0.3.0): design_handoff_dreamer_gui
  v5 recreated as plugin/gui/ (frontend-developer subagent pass; the handoff
  HTML is a claude.ai artifact bundle — real source extracted from the
  __bundler/template script via ConvertFrom-Json). PluginEditor = WebView2 +
  ~185 loop-built relays (donor scaffolding); CMake adds WEBVIEW2 flags +
  TheDreamerAssets. Fonts fetched from google/fonts GitHub via IWR and
  committed. validator gained a gui stage (load+screenshot over HTTP bridge).
  GOTCHA: plain file:// blocks ES-module imports — headless-Chrome checks of
  the raw page need --allow-file-access-from-files (validator's HTTP path
  unaffected).
- 2026-07-18 — v2 CORE engine + plugin (design handoff FEATURES v4 executed,
  user-approved rebuild): 4-tone DreamVoice with waveshaper (C++-baked LUTs),
  ToneSvf TVF, AUX env, G-LFO taps, Dream Vector v4, mod matrix, per-tone pan;
  global filter block (14-type frozen list, 7 live); FX panel subset in spec
  order (filters→modfx→delay(mono-sum in)→reverb, donor knob laws + donor
  reverb voicing/kDryScale). ~180 params re-locked incl. GUI additions
  (f1_env = aux-max→cutoff ±2 oct, f2_morph reserved, delay_on/rev_on).
  test_engine.cpp replaces test_voice.cpp; validator.json harness updated.
  RC 0.2.0: validator 11/11 PASS (pluginval 8), deployed local + share.
- 2026-07-17 — Phases 2-6 complete, RC 0.1.0 deployed. Phase 2: RhinoFilter.h
  port (Faithful1017Filter dropped, namespace-only diff audited) + FilterSlot
  adapter, bitwise parity all 7 types. Phase 3: full FX chain + Lfo ported,
  bitwise parity per class. Phase 4: DreamVoice glue (2 LFOs/partial,
  control-rate pitch/cutoff/level laws, oldest-note steal fix, kill/killAll,
  sustain) — alloc-free, all behavior tests green. Phase 5: plugin shell,
  ~99 params, donor FX bus order, generic editor, Release build green.
  Phase 6: pluginval strictness 8 SUCCESS (incl. 44.1/48/96k × 64/256/1024),
  deployed local + share. Note: test-harness lesson — kill() silences but
  freezes envelope levels (by design, matches steal behavior); hermetic
  renders need fresh synth instances.
- 2026-07-17 — Phase 1 scaffold: repo layout (dsp/bank|ported|glue, plugin,
  tests, validation), bank headers copied, deps\JUCE cached, CMake configure
  green with placeholder plugin stubs, tests\test_bank.cpp ALL CHECKS PASSED
  (grain, pitch 440±0, bell/pad demo renders, stealing). CLAUDE.md updated
  with scope decisions (single-mode DREAM, 24 voices, 2 LFOs/partial, full FX,
  Character-off filters).
