# GUI DEFECT SHEET — for the GUI-Claude face master (2026-07-23, plugin @ 2.7.2)

> **STATUS UPDATE (2.7.2)**: every item in sections B and C except C4 has been
> FIXED by wiring in the shipped app.js (frontend-developer pass, all changes
> carry `⚠ GUI-Claude fold upstream` comments) — they move to the section-A
> FOLD list. Plus new: fitToWindow base = live layout height (660/850) so the
> keybed is part of the resize in both fold states (user-stated requirement);
> `window.uiProgram` receiver (editor pushes program changes). Still open for
> upstream DESIGN decisions: C4 (lofi_alias) and the D-series.

Audience: **GUI-Claude** (the face designer/owner). Source: four-agent full-code
audit of the v16 production face against the 2.7.x engine, plus the wiring
fixes the VM session has already applied locally to `plugin/gui/app.js`
(marked ⚠ FOLD — a fresh handoff that overwrites app.js will silently revert
them unless folded into the face master).

The C++ side already provides **every** feed listed here — no new native
functions are requested except where marked (one small addition). File/line
refs are against the shipped `plugin/gui/app.js` at 2.7.1.

---

## A. Already fixed in the shipped app.js — MUST BE FOLDED into the master

| # | What | Where (2.7.1 app.js) |
|---|---|---|
| A1 | TD-005 envelope refresh: `renderEnv` subscribed to all env params | subs block after renderEnv() |
| A2 | Envelope source of truth: v17-reserved ids (`gamp/gflt/gaux_env_*`, `*_ovr`) routed to `Bridge.local` (NOT relays); `envVal` reads real per-tone `tva_/tvf_/aux_`; TONE edit sets only the touched stage; ALL writes through to all 4 tones | Bridge.local, V17_GLOB/V17_TONE, envVal/commitEnv |
| A3 | TD-003 fitToWindow (full min-ratio scale, origin-0,0 centering, boot rAF re-fit) | fitToWindow |
| A4 | TD-007 list swap: `TVFTYPES` = full 14-type list, `FTYPES` = 4 (parity with Params.h); mock `f1Type/f2Type` defaults 0 | consts ~line 125 |
| A5 | Sync-division label law = engine's `round(v*11)` (tone LFOs + delay), replacing `floor(v*12)` | lfoRow, delay knob |
| A6 | Preset-load wiring (2.4.1): loadPreset/loadUserPreset native calls; boot pulls getPresetList/getWaveList/getUserPresetList | Bridge.fn call sites |

## B. Broken/lying in the live plugin — face must consume existing feeds

| # | Defect | Feed that already exists (PluginEditor.cpp) |
|---|---|---|
| B1 | **`ui_global_offset` is a phantom id** — OFFSET MODE toggle binds a relay that has no param; LED dead, mode unsaved/unautomatable. Decide: UI-local state (Bridge.local, like A2) or ask for a real param upstream. | n/a (id must stop being a live relay) |
| B2 | **On-screen keyboard/wheels/KEYS fold are visual-only** — regression vs the v13 face. Keys flash + print NOTE ON but make no sound; fold clips the keybed instead of resizing the editor. | `noteOn/noteOff/pitchBend/modWheel/keyboardFold` all registered (lines ~207–244) |
| B3 | **Header meters are fake** (tone-level + `Math.random()` animation). | 30 Hz `window.uiMeters({l,r})` push (lines ~171–179) — face must define the receiver |
| B4 | **LIM LED is fake** (lit from the fake meters). | `getLimiterGR` native fn (lines ~284–289) |
| B5 | **Version silk hardcoded "VER 1.0"** (binary is 2.7.1). | `getInfo().version` |
| B6 | **Radar orbit**: GUI advances the REAL `vec_phase` param ~60 fps with a wrong rate law (`0.02*1000^v`; engine is `0.02*400^v` = 0.02–8 Hz) and ignores orbit shape; fights automation; RATE Hz readout lies. Make the radar READ-ONLY (local display phase), adopt the 400^v law for the readout. | engine law: DreamVoice.h orbitRateHz |
| B7 | **MODFX list has 4 of 7 modes** — DIMENSION/ROTARY/BARBERPOLE unreachable; LCD prints "undefined" for host-set modes ≥ 4. List: CHORUS/FLANGER/PHASER/ENSEMBLE/DIMENSION/ROTARY/BARBERPOLE. | modfx_type is choice-7 |
| B8 | **User-preset bank list fetched once at boot** — SAVE/RENAME/DELETE reopen the browser from the stale array (saved preset invisible live; possible TypeError on empty bank; auto-name numbers off the stale count). Re-pull `getUserPresetList` after each mutation. | fns exist |
| B9 | **Header preset name** tracks only GUI factory loads — stales on host program change, user-preset load, and boot (session restore shows P001). Needs a small push: editor → `window.uiProgram(index,name)` on program change (VM session will add the C++ push; face must define the receiver). "FACTORY · 47" also hardcoded. | C++ push to be added (flagged) |

## C. Stale-on-external-change composites (TD-005 class)

| # | Composite | Symptom |
|---|---|---|
| C1 | Tone-edit wave-class compartment (`isENS/isSHOT`, `hitPlay` branch) frozen at build | preset/automation wave change leaves LOOP RATE cluster or STRETCH controls for the OLD wave class on screen |
| C2 | Tempo-sync knob labels are snapshots (tone LFO rate, LOOP RATE, DELAY TIME) | external `*_sync`/`*_beats` change leaves the old label; GUI sync click is the only rebuild path |
| C3 | UTIL modal: `prePost` button has paint-on-click only (the file's ONE sub-less widget); LO-FI PARAMS knob binds the focus-selected raw at modal-open (external `lofi_pfocus` change → LCD says SRATE, knob edits BITS) | stale while modal open |
| C4 | `lofi_alias` is a Bool shown as a continuous knob (both sides slider-bound) | design decision needed: toggle widget, or upstream param change to a real continuous param |

## D. Cosmetic / hygiene

- D1 KEY FLW knob shows ±63 (`fmtBip`) while the param's host display law is −100..+100 — align.
- D2 `fmtTime` (app.js ~line 321) is dead code with a WRONG law (20 s vs the engine's 10 s ceiling) — delete or fix before anyone wires it.
- D3 Subscription accumulation: every `rebuildToneEdit()` re-registers ~130+ listeners with no unsubscribe (wrapSlider/wrapToggle/wrapChoice unsubscribe stubs are no-ops). No live-node corruption found, but CPU grows with tone-switch count. Consider tracked/unsubscribable relays in the next master.
- D4 Dead shadow state: `UI.matrix`, `UI.penv`, `openMenuObj` are unused leftovers — delete.
- D5 `g_env_a/d/s/r` are real automatable params with relays but no widget (the ENVELOPE editor's ALL mode deliberately writes per-tone params instead). Decide their GUI fate or document host-only.
- D6 `drift` (global humanize) has no GUI control and no host-only documentation — decide.

## E. Known/deliberate (do not "fix")

- dly/rev PARAMS knobs inert (ported FX can't take extra knobs — rule 1).
- FX-PARAM matrix dest reserved/inert; MIDI LEARN button non-functional (deferred).
- v17 global-env tier (`gamp_env_*` etc.) is UI-local by design until the DSP tier lands; FOLLOWS GLOBAL/OVERRIDE line + dots are session-local cosmetics.
- Envelope sliders show raw 0–127 (house style), not ms — deliberate.
