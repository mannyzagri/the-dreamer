# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-17, phase: port/scaffold)

| Item | Value |
|---|---|
| Deployed version | none yet (no plugin build shipped) |
| Plugin identity | "The Dreamer", VST3, `Mnsh`/`Drmr` — NOT yet scanned by any host |
| Current phase | 1/6 port/scaffold (branch `port/scaffold`) |
| Phases done | — |
| Pending phases | port/filter → port/fx → port/voice → port/plugin → port/validation |
| GUI | generic APVTS editor; WebView design handoff IN FLIGHT elsewhere (Claude Design) — param IDs locked, see plugin/Params.h once it exists |
| Git | repo C:\the-dreamer, remote mannyzagri/the-dreamer, main pushed; phase branches merge to main after each gate |
| Build plan | C:\Users\vagrant\.claude\plans\read-claude-workflow-md-from-shared-drifting-leaf.md (approved 2026-07-17) |

### Phase-gate checklist (every phase, before merge)
- [ ] cl.exe test harness(es) for the phase compile and print ALL CHECKS PASSED
- [ ] No new warnings introduced (plugin phases: Release x64 build clean)
- [ ] Ported files diff vs donor = namespace rename only (audit with fc/diff)
- [ ] STATE block updated (phase, git state)
- [ ] Explicit `git add` file lists only — NEVER `git add -A` (concurrent sessions)

## Facts a fresh session needs

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

- 2026-07-17 — Phase 1 scaffold: repo layout (dsp/bank|ported|glue, plugin,
  tests, validation), bank headers copied, deps\JUCE cached, CMake configure
  green with placeholder plugin stubs, tests\test_bank.cpp ALL CHECKS PASSED
  (grain, pitch 440±0, bell/pad demo renders, stealing). CLAUDE.md updated
  with scope decisions (single-mode DREAM, 24 voices, 2 LFOs/partial, full FX,
  Character-off filters).
