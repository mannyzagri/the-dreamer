# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. Overwrite-only, current open
> loop only (superseded story lives in CHANGELOG.md).

## Where things stand — TD-001 fixed on branch, 2.5.2 release pending

- **TD-001 (0 dBFS noise after ~20-30 s)** root-caused and fixed on
  `fix/td-001-noise` — full story in CHANGELOG 2.5.2 entry. Validator dsp
  **11/11** (new `fx_read_bounds` gate), end-to-end 2× vst3-probe full suite
  clean (was: deterministic fault at block 148467, preset SOLINA FIELDS).
- **NEXT: user go → release.ps1 (bump 2.5.2, -DryRun first, remember the
  [[release-ship-tool-gotchas]]) → deploy both targets → user ear pass**:
  load SOLINA FIELDS @44.1 k, play + idle 60 s → must stay clean; panic
  button now genuinely clears every stage if anything ever appears.
- After 2.5.2 soaks green: STRIP the `DREAMER_TD001_TRACE` blocks from
  PluginProcessor.cpp (architect verdict: tracer retires once closed).

## OPEN — mac triage inbox (comms/to-vm.md, msg 2026-07-22T10:15, [ACK])
- **TASK 2 — GUI resize doesn't rescale in built VST3**: verify pipeline
  first (deployed app.js contains fitToWindow? → WebView2 cache → only then
  debug). Not started.
- **TASK 3 — library v4 integration**: BLOCKED until dreamer-library-v4.zip
  lands (REBAKE-V4-INSTRUCTIONS.md §Integration steps).
- **Bootstrap rollout** (msg 2026-07-22T11:00): code-bank agents/, junctions,
  CLAUDE-header, changelog ledger (claim TD-001/002/003 ids), archive sweep
  (post move list to to-human BEFORE deleting). Ordered after TD-001.

## Notes for the next session
- vst3-probe soak harness now shared: C:\code-bank\tools\vst3-probe
  (run-probe.ps1; ASan variant; README has the detection caveats). Project
  copy tools/hostprobe stays as the TD-001 repro archive.
- Ported ModFx.h readDelayed + Effects.h tape path carry the SAME
  one-past-end hazard class (documented in CHANGELOG; rule 1 = no ported
  edits; a glue-side guard needs a design decision if ever exercised).
- 2.5.1 on the share STILL CARRIES TD-001 until 2.5.2 deploys.
