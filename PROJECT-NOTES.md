# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-17, phase: ALL 6 PHASES DONE — RC 0.1.0 deployed)

| Item | Value |
|---|---|
| Deployed version | **0.1.0 RC** — `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root), built 2026-07-17 |
| Plugin identity | "The Dreamer", VST3, `Mnsh`/`Drmr` — new identity: Cubase needs a FULL plugin re-scan on first load |
| Current phase | all 6 phases merged to main; awaiting user Cubase 15 manual pass |
| Phases done | scaffold, filter, fx, voice, plugin, validation (each gated: tests green, see validation/VALIDATION.md) |
| Pending | user Cubase pass; WebView GUI handoff integration (frontend-developer when it lands); optional: per-partial pan (deliberately omitted — new params + stereo voice sum if wanted) |
| GUI | generic APVTS editor (~99 params, scrollable); WebView design handoff IN FLIGHT elsewhere (Claude Design) — param IDs LOCKED in plugin/Params.h, bind by ID |
| Git | main has all 6 phase merges, pushed to mannyzagri/the-dreamer; share the-dreamer-src\ export refreshed |
| Build plan | C:\Users\vagrant\.claude\plans\read-claude-workflow-md-from-shared-drifting-leaf.md (approved 2026-07-17) |

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
- pluginval NOT installed on the VM; download via Invoke-WebRequest (TLS 1.2),
  fallback Steinberg SDK validator (C:\Utilities\vst-sdk_3.8.0_build-66_2025-10-20).
- Deploy (phase 6): whole "The Dreamer.vst3" bundle → \\VBOXSVR\vagrant\ root
  + C:\the-dreamer\; refresh share the-dreamer-src\ export after each push.

## STATUS log (newest first)

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
