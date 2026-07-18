# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-18, RC 0.7.0 — loops v2 (26 loops), validator all-stages PASS)

| Item | Value |
|---|---|
| Deployed version | **0.7.0 RC (FX v1.5 + v8 + loops v2)** — `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root); validator PASS all stages (pluginval 8) |
| Bank | **114 waves** = 78 cycles + **26 Ens loops** (v1's 16 at indices 78-93 held stable + v2 batch2's 10 string/choir/vox at 94-103) + 10 Shot (104-113). Loops baked from assets/loops/*.wav by tools/bake_loops_header.cpp; recipes tools/bake_loops.py + bake_loops2.py. LoopBankData.h = 6.13 MB int16 (exceeds §1's <5 MB soft target — consequence of 26 loops; not blocking). NOTE: "bank2"/rompler-bank-v2 (the 78 cycles) was already fully in since v1 — the share copy only differed by CRLF. |
| Contracts | design-handoff/v8/ (DSP_BUILD/FEATURES §11 + GUI_2 README) is newest; supersedes v7 where it conflicts. ~250 params → Cubase FULL re-scan |
| Engine | v3+v7 base + **FX v1.5**: modfx 7 modes (+Dimension/Rotary/Barberpole; extras p0..p3 drive them), standalone LO-FI/WIDTH/TALK stages (host-automatable only, not on panel), per-slot PARAMS (modfx/dly/rev p0..p3+pfocus; **dly/rev extras RESERVED/inert** — ports can't take knobs under rule 1), fx_prepost (LO-FI PRE/POST; WIDTH fixed POST), output peak meters. Bus order + RT-safety per architect review (see CLAUDE.md v1.5 decisions) |
| GUI | v8 face on v7: header MIDI LEARN (**NON-FUNCTIONAL — deferred**) + stereo L/R meters (window.uiMeters, 30 Hz C++ feed); FX rows RATE/DEPTH/**PARAM**/MIX (PARAM binds modfx/dly/rev _p0, type-dependent label); modfx stepper 7 modes; Global-LFO + Vector SHAPE gain <> steppers; native proportional resize 50–200% + grip |
| ⚠ Preset payloads | 24 factory patch payloads still AUTHORED placeholders (app.js) — real sound design pending from the design track |
| Flagged deviations | dly/rev PARAMS reserved/inert (ports); FXPARAM matrix dest DEFERRED (no slot-selector in v8 matrix); MIDI learn deferred (button is a visual no-op); WIDTH fixed POST (F4); + carried v3/v7 (tvf_env/flt1_env unipolar, loop-seam body-relative, flt_bal PAR-only) |
| Pending | user Cubase ear+eye pass; real factory presets; MIDI learn wiring; dly/rev PARAMS wiring (needs non-ported delay/reverb); FXPARAM matrix dest + pfocus GUI; V1.1 filters; V2 DreamPlane |
| Git | committed via ship.ps1; share the-dreamer-src\ export refreshed |

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

- 2026-07-18 (loops v2) — loop bank v1→v2 (RC 0.7.0). Added the 10 batch2
  string/choir/vox loops (dreamer-loops-v2.zip / bake_loops2.py) appended
  after v1's 16 (indices 94-103; v1 78-93 byte-identical, shots shift to
  104-113). Re-baked LoopBankData.h (26 loops, 6.13 MB), Bank3.h kNumLoops
  26, kNumWaveforms 114, test_bank3 + wave-list + app.js WAVES all to 114.
  Found: "bank2" (rompler-bank-v2, the 78 cycles) was ALREADY incorporated —
  the share copy differed only by CRLF. Full validator PASS; released 0.7.0.
- 2026-07-18 (v1.5/v8) — FX v1.5 + v8 GUI (RC 0.6.0). Architect-reviewed FX
  bus restructure: 3 new modfx modes (Dimension/Rotary/Barberpole) + LO-FI/
  WIDTH/TALK stages + per-slot PARAMS extras + LO-FI pre/post; RT-safety
  fixes (Barberpole feedback tanh+DC clamp, quadrature LFOs, quantize-on-hold,
  fixed Haas, discrete de-click). test_fx_v15 harness. v8 GUI: 7-mode stepper,
  FX PARAM knobs (p0), header meters + resize; MIDI learn button non-functional
  (deferred). Full validator PASS; built+deployed via release.ps1, committed
  via ship.ps1. Gotchas: dly/rev PARAMS extras are inert (ports can't take
  knobs, rule 1); staging parser needs the CMake closing `)` on its own line;
  bus is already stereo from per-tone pan (architect premise correction).
- 2026-07-18 (v3) — DSP_BUILD phases 11-14 + GUI_SPEC renovation (RC 0.4.0).
  Bank v3 (loops baked from delivered WAVs via C++ tool, shots synthesized),
  PcmOsc3, noise, drift, vector upgrades, Ensemble, MASTER, §9 param relock
  (~192 params, tid() suffix style), GUI rebound + renovated
  (frontend-developer agent, 100% id coverage). Validator 14/14 incl. new
  bank3 harness. Gotchas: delivered loop WAVs are full 16-bit (12-bit
  quantization happens at header-bake); §9 has no aux amount / penv loop /
  orbit-voice params (flagged additions); compile of the 3.7MB LoopBankData
  header is fine (~1 min).
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
