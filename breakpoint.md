# BREAKPOINT — The Dreamer (for the next session)

> Read AFTER PROJECT-NOTES.md STATE + CLAUDE.md. Overwrite-only, current open
> loop only (superseded story lives in CHANGELOG.md). Previous 1.x/2.0–2.1
> breakpoints are closed and erased.

## Where things stand — RC 2.5.0 deployed & green

- **RC 2.5.0** built, validated (dsp 10/10, staging/bundle/gui load+screenshot/
  deploy/host **pluginval 8**), deployed to both targets, committed + pushed
  (main `73f00d9`) + share `-src` refreshed. Header stamp must read **2.5.0**.
  2.5.0 = **v16 GUI** (GUI-Claude ADOPTED GUI_INTEGRATION_CONTRACT — load wired,
  data fetched, KINDs matched, local fonts; no integration seds this round) +
  **Global LFO 2** (2nd global LFO + matrix sources 5→7: G-LFO 2 live, G-Aux
  reserved). **Param-list change vs 2.4.x → FULL RE-SCAN.**
- ⚠ **v17 global-env tier is coming**: v16 app.js already RESERVES (declared, no
  widgets) `gamp_env_a/d/s/r`, `gflt_env_a/d/s/r`, `gaux_env_a/d/s/r` + per-tone
  `amp_ovr`/`flt_ovr`/`aux_ovr` follow-global flags, and the **G-Aux** matrix
  source. The next GUI handoff will likely BUILD these → they'll need DSP params +
  engine (global AMP/FILT/AUX envelopes + the G-Aux source made live). Decide
  add-to-DSP vs defer when it lands.
- The whole recent arc shipped:
  - **DSP UX round D1–D16** (2.2.0 + 2.3.0): live env-rate, log env-time+units,
    global offsets, semi, g-octave, engine detune, bipolar keytrack, switchable
    limiter, INIT program 0, panic+FX-flush, output scope tap, MIDI learn, user
    presets, per-wave loudness table, PLAY MODE all-types + granular LOOP RATE.
  - **v15 production GUI** (2.4.0): the new framework-free WebView face
    (design_handoff_dreamer_gui/production) integrated, + DSP grown to match its
    contract (flt2_env, lfo_sync, FX PARAMS focus model, detune→Choice,
    flt_route/fx_prepost→Bool).
  - **Preset-load fix** (2.4.1): the v15 app.js had load unwired + hardcoded
    lists → wired loadPreset/loadUserPreset + boot-fetch of the processor's
    authoritative getPresetList/getUserPresetList/getWaveList.

## OPEN LOOP #1 — user's Cubase eye+ear pass on 2.4.1 (the new face)
No param change since 2.4.0 → **reload the instance / restart Cubase** (WebView2
cache is the recurring "old GUI" cause — a rescan does NOT clear it; full restart
does). Coming from ≤2.3.0 needs a FULL RE-SCAN. Verify on the NEW v15 panel:
- **Presets now load** (the just-fixed bug): step ▲/▼ and pick from the browser →
  the sound + knobs change; SAVE/RENAME/DELETE user bank works.
- **MOD MATRIX moves the sound** and **P-ENV EDIT modal** works (both were
  editor-only in earlier app.js revs; bound in file(2)/2.4.0).
- F2 **ENVELOPE**, GLOBAL **OFFSET MODE**, **DETUNE**, **LOOP RATE** on ENS waves,
  FX PARAMS **focus**, **SPECTRUM**, **MIDI-learn** right-click, **SOUND OFF**.
- Known-by-design quirks to confirm (not bugs): LO-FI PARAMS uses the RAW named
  knobs; **DELAY/REVERB PARAMS knobs are inert** (ported FX); FX PARAM matrix
  dest is reserved/inert.

## OPEN LOOP #2 — GUI-Claude coordination (IMPORTANT)
The preset-load fix + two KIND/id fixes (`orbitVoice`→toggle, `width_width`→
`width`) currently live in the **plugin's** `plugin/gui/app.js`. **GUI-Claude's
next handoff will OVERWRITE them** unless folded in upstream. Written for them and
mirrored to the share `\\VBOXSVR\vagrant\The Dreamer\`:
- **GUI_INTEGRATION_CONTRACT.md** — the rules + the pre-handoff self-check.
- GUI_PROD_MISMATCH.md, GUI_DSP_DIFF.md — the binding cross-checks.
When the next GUI handoff lands: **diff its app.js against these fixes** before
integrating; re-apply/verify §1–§5 of the contract, then build+gui+pluginval,
deploy. Do NOT re-introduce hardcoded WAVES/PRESETS or unwired load.

## Pending / deferred (flagged decisions, not bugs)
- **v3 loop_roots.json** — if the v3 library re-tuned the loop samples, LoopRoots.h
  still holds the v2-measured roots (tuning may drift); a v3 loop_roots.json would
  re-measure. Not delivered.
- **Exact E-MU Z-plane frame coefficients** for DreamPlane (flt type 13) — using a
  credible generic frame set; precise X-Lead coeffs could come from
  prodatum/X-Lead data if wanted.
- **FX-PARAM matrix dest** — still reserved/inert (loop_rate IS now a live dest;
  FX PARAM needs a GUI focus target).
- **LADDER filter** stays mild (weak resonator in the byte-locked rhino port,
  rule 1 — not ours to retune).

## Toolchain (see memories for detail)
Skills `dsp-pass` / `gui-pass`; validator `C:\code-bank\validator\validate.ps1
-Project C:\the-dreamer -Stages …`; `release.ps1` (bump+build+verify+deploy) /
`ship.ps1`; MSVC at `C:\BuildTools` (vcvars64.bat there). Branch → merge --no-ff →
push per commit; explicit `git add` (concurrent sessions). GUI face work is
GUI-Claude's lane ([[gui-work-via-frontend-developer]]); this session integrated +
built + validated + deployed.
