# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. Overwrite-only, current open
> loop only (superseded story lives in CHANGELOG.md).

## Where things stand — 2.6.0 RELEASED + DEPLOYED (NEW v5 LIBRARY + TD-001..006 fix stack), ear+eye gate pending

- **2.6.0 = new v5 sound library** (130 loops replace v3/v4; same slots, new
  content; HITs unchanged). See STATE "Sound library (v5)" row. Bake sources
  now in assets/library-src. Presets play v5 timbres → may need re-voice.
- measure_drift octave-errors on ~9 v5 loops (subharmonic) — tool limit, not a
  tuning defect (STATE row). Ear-confirm PAD/VOX/MORPH pitch.
- The 2.5.2–2.5.6 fixes (noise/loops/resize/loudness/preset-envelope/DreamPlane)
  are all still in and deployed under 2.6.0.


- **TD-005** preset-load envelope refresh (app.js wiring), **TD-006** DreamPlane
  level-match (ZPlane kMakeup 0.55) — both in 2.5.6, validator 11/11.
- **TD-007 (filter-bank swap) SCOPED + DEFERRED** — see STATE row; user
  confirmed "tone gets 14 types, global = LP/BP/HP"; big preset-breaking
  change, do after the ear-pass.
- ⚠ GUI-Claude upstream fold list now: TD-003 (fitToWindow) + TD-005
  (renderEnv preset-refresh subscription) — both in app.js, both overwritten
  by the next handoff unless folded into the face master.

- **TD-001 (0 dBFS noise)** fixed 2.5.2, **TD-003 (GUI dead frame/resize)**
  fixed 2.5.3, **TD-004 (+6 dB louder)** 2.5.4 — all deployed, pluginval 8.
  TD-001 story: validation/TD-001-REPORT.md. TD-003/004: CHANGELOG.
- **NEXT: user ear+eye pass (one session, header 2.5.4)** — no re-scan needed
  (no param change): (1) +6 dB loudness feels right (iterate the WaveNorm
  target if not); (2) panel fills+centers at open and every resize; (3)
  SOLINA FIELDS @44.1 k play+idle 60 s stays clean; panic clears every stage.
- **TD-002 (loop detune)**: ✅ FIXED in 2.5.5 VM-side (Route B) — corrected the
  baked drift + layered-center spread in the delivered WAVs via
  tools/correct_drift.cpp (flat 220, verified <0.86¢ all 130). assets/loops is
  now the corrected audio; do NOT re-run correct_drift on it. If the true
  dreamer-library-v4.zip lands, it cleanly replaces (drift=0 by construction) —
  overwrite assets/loops + re-bake, no correction needed.
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
