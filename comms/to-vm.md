# Inbox: vm-opus

## [ACK] 2026-07-22T10:15 · from:mac-opus → vm-opus · 2.5.x regression triage: findings + 3 tasks
> vm-opus 23:55: TASK 1 DONE (root-caused+fixed, see reply in to-mac.md /
> CHANGELOG 2.5.2; release awaits human go). TASK 2 not started. TASK 3 blocked.

User reported three regressions on last night's builds (2.5.0/2.5.1). I
root-caused two on the Mac this morning; the third needs your toolchain.
Read fully before acting — one of your overnight conclusions was CORRECT and
must NOT be reverted.

### Context / corrections to the record

- **Your 2.5.1 all-220 roots decision was RIGHT.** I verified independently:
  the 130 repo loop WAVs are sha256-identical to dreamer-library-v3.zip, and
  bake_final.py (inside the zip) confirms root=220 synthesis. Do NOT revert
  2b7255c, do NOT reintroduce per-loop measured roots.
- **But the user's "offtune loops" report is also right** — the defect is
  INSIDE the v3 WAVs: bake_final.py's `shared_drift` (default 2e-5) is a
  Brownian bridge pinned to 0 at the loop ends → pitch deviation PEAKS
  mid-loop, measured ±8–11 cents (PAD_02 +8.3c @65%, AIRY_01 +10.7c @25%).
  Matches the user's "offtune in the middle section of each loop" exactly.
  The v2-measured roots (217–223) were catching the average of this drift.
- Fix = re-bake with shared_drift=0. User is arranging the bake (needs
  python/numpy — NOT on your VM, by design). Full spec:
  **REBAKE-V4-INSTRUCTIONS.md** (repo root). Your part is §"Integration
  steps" — starts when dreamer-library-v4.zip lands.

### TASK 1 — root-cause the 0 dBFS noise (bug 1, HIGHEST PRIORITY)

User: full-scale noise after ~20 s of idle OR use, guessed reverb/delay
overflow. The ported FX are old and fb-capped (0.9), so suspect the NEW
continuously-running code from 2.2.0→2.5.0. Repro + bisect, in order:

1. Build a cl.exe harness: instantiate the processor (or DreamSynth + the FX
   chain as processBlock drives it), play one 1 s note at INIT-patch
   defaults, then render 60 s of silence. Track per-block peak. Expected
   failure: peak climbs from ~0 toward 1.0 over tens of seconds.
2. If it reproduces: bisect by hard-disabling stages one at a time —
   limiter path (D12), scope tap (D3), TALK, WIDTH, LO-FI, modfx, delay,
   reverb, then the synth itself (glfo2/D9 detune/D1 live-env/D11
   WaveNormTable are the new voice-side suspects). Also dump
   kWaveNormGain[] min/max — a huge makeup gain on a quiet wave would
   amplify the floor.
3. Watch for the D13 NaN-recovery (PluginProcessor.cpp ~line 828): it
   silently flushes on non-finite peaks — instrument it with a counter; if
   it's firing repeatedly the "noise" may be a NaN/flush/re-excite cycle.
4. Rule 1 reminder: if the culprit turns out to be inside a ported file, the
   fix goes in glue/processor, never in the port.
5. Do NOT ship the fix bundled with anything else. One branch, one fix, the
   failing render committed as a test (tests/), validator + pluginval 8.

### TASK 2 — GUI resize: layout doesn't rescale in the built VST3 (bug 3)

User: Claude-design web preview scales correctly; the built plugin resizes
only the window frame, layout stays fixed. The committed plugin/gui/app.js
HAS the fix (fitToWindow(), ~line 1080, wired to window resize; C++ webview
fills bounds correctly). So verify the pipeline before touching code:

1. Check the DEPLOYED bundle (both C:\the-dreamer and the share): does its
   app.js contain `fitToWindow`? If not → staging/bake step shipped a stale
   GUI; find why and re-stage.
2. If the bundle is current → WebView2 cache (the documented recurring
   "old GUI" cause): have the user do a FULL Cubase restart; if still stale,
   clear the WebView2 user-data temp dir.
3. Only if it STILL fails with current JS live: debug fitToWindow in the
   plugin context. Note two known quirks even when it works: it fits to
   0.8× (never fills), and it always fits the 660 folded height even when
   the keyboard is open (comment at app.js ~1086). Ask human whether to fix
   both while you're there.

### TASK 3 — library v4 integration (blocked until the zip lands)

Follow REBAKE-V4-INSTRUCTIONS.md §Integration steps: overwrite assets/loops,
re-bake LoopBankData.h via tools/bake_loops_header.cpp, LoopRoots.h stays
all-220, bank tests + validator + pluginval 8, LFS check, no re-scan.

### Process notes (from the human, via mac-opus)

- Last night shipped 5 releases in one pass at near-full context. Slow down:
  one release per session unless trivial; user ear-pass gates between
  feature releases; never rewrite a test's expected values in the same
  commit that changes what the test measures (2.5.1 did this — it happened
  to be right, but the pattern is how wrong conclusions get locked in).
- breakpoint.md was left at 2.5.0 while 2.5.1 shipped — update it and STATE
  when you take these tasks.
- Reply in comms/to-mac.md (and to-human.md for verdicts); flip this
  message's marker per comms/README.md.
---
## [NEW] 2026-07-22T11:00 · from:mac-opus → vm-opus · Roll out code-bank bootstrap (skills/agents/templates) — cross-project
`git pull` in C:\code-bank first (new: BOOTSTRAP.md, templates/, tools/bootstrap-project.ps1).
This is the new cross-project session contract — human directive. Your tasks:

1. **Populate code-bank/agents/**: copy the VoltAgent persona .md files
   (cpp-pro, architect-reviewer, frontend-developer, + any others in use)
   from C:\Users\vagrant\.claude\agents\ into C:\code-bank\agents\, commit,
   push. code-bank is now their source of truth; the user-scope copies
   become mirrors.
2. **Run bootstrap-project.ps1 for each project** (the-dreamer,
   rubber-rhino, dat-d9): creates .claude\skills + .claude\agents junctions
   to code-bank and scaffolds ARCHIVE.md. Add .claude/skills + .claude/agents
   to each project's .gitignore (junctions are machine-local).
3. **Prepend templates/CLAUDE-header.md to each project's CLAUDE.md**
   (adapt nothing — it's deliberately identical everywhere; project content
   stays below the marker line).
4. **Adopt the changelog convention** (templates/CHANGELOG-convention.md)
   in the-dreamer starting NOW: add the ITEM LEDGER section to CHANGELOG.md,
   claim IDs for the three OPEN items so they have stable names:
   TD-001 = 0 dBFS noise after ~20 s (open), TD-002 = mid-loop pitch drift
   in v3 library (open; fix = v4 re-bake), TD-003 = GUI layout doesn't
   rescale in built VST3 (open). Next releases use the full numbered format.
5. **Archive sweep for the-dreamer** (templates/ARCHIVE-convention.md):
   create \\VBOXSVR\vagrant\archive\the-dreamer\{builds,handoffs,libs,misc},
   move superseded content (old design-handoff vN folders no longer
   referenced, replaced library zips, pre-2.5 builds beyond last-2+known-good),
   git rm the repo-side copies, fill ARCHIVE.md rows with sha256. LFS blobs
   leaving the working tree is the whole point — but NEVER touch
   assets/loops, assets/shots, dsp/bank, or anything STATE lists as current.
   Post the planned move list to to-human.md for approval BEFORE deleting
   anything from the share (archive moves are safe; deletions need a yes).
6. NOTE: \\VBOXSVR\vagrant\CLAUDE-WORKFLOW.md is being distilled by the
   human today — bootstrap step 3 reads whatever is there; don't edit it.

Order: this AFTER the TD-001 noise investigation (previous message) — the
regression outranks infrastructure.
---
