# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. Overwrite-only, current open
> loop only (superseded story lives in CHANGELOG.md).

## Where things stand — 2.5.4 RELEASED + DEPLOYED (TD-001 + TD-003 + TD-004), ear+eye gate pending

- **TD-001 (0 dBFS noise)** fixed 2.5.2, **TD-003 (GUI dead frame/resize)**
  fixed 2.5.3, **TD-004 (+6 dB louder)** 2.5.4 — all deployed, pluginval 8.
  TD-001 story: validation/TD-001-REPORT.md. TD-003/004: CHANGELOG.
- **NEXT: user ear+eye pass (one session, header 2.5.4)** — no re-scan needed
  (no param change): (1) +6 dB loudness feels right (iterate the WaveNorm
  target if not); (2) panel fills+centers at open and every resize; (3)
  SOLINA FIELDS @44.1 k play+idle 60 s stays clean; panic clears every stage.
- **TD-002 (mid-loop detune)**: VERIFIED nothing to disable DSP-side (roots
  all 220, pitch = constant ratio) — the drift is baked in the v3 WAVs;
  BLOCKED on dreamer-library-v4.zip (human-arranged python re-bake).
- After 2.5.x soaks green: STRIP the `DREAMER_TD001_TRACE` blocks from
  PluginProcessor.cpp (architect verdict: tracer retires once closed).
- ⚠ GUI-Claude upstream: the TD-003 app.js fix is handoff-overwrite class —
  fold into the face master (like the 2.4.1 preset-load fix).

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
