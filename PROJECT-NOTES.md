# The Dreamer — PROJECT NOTES (session handoff)

> Read CLAUDE.md first (project contract + scope decisions), then this STATE
> block. STATE is OVERWRITE-ONLY: update it in place as the last act of a work
> pass — never append a second STATE block.

## STATE (updated 2026-07-23, RC 2.5.3 — TD-003 GUI-fit fix RELEASED+DEPLOYED on top of 2.5.2 TD-001 noise fix; ear+eye pass pending)

> History (0.1.0 … 2.5.2) lives in CHANGELOG.md. This block is CURRENT-ONLY.

| Item | Value |
|---|---|
| Deployed version | **2.5.3 RC** — binary literal `2.5.3`, `The Dreamer.vst3` at C:\the-dreamer\ AND \\VBOXSVR\vagrant\ (share root), build↔local↔share SHA-identical, pluginval 8 SUCCESS. **RE-SCAN REQUIRED** + remove/re-add instance; header must read 2.5.3. **EAR+EYE GATE PENDING (one session verifies both):** (1) panel fills+centers the window immediately at open and at every resize (TD-003); (2) SOLINA FIELDS @44.1 k, play + idle 60 s → stays clean (TD-001). `juce_add_plugin VERSION ${PROJECT_VERSION}`. |
| TD-003 (GUI fit) | ✅ **FIXED in 2.5.3** (main `162b133`): app.js fitToWindow scaled by 0.8 (never filled >80%) + CSS flex centered the UNSCALED box → dead frame, "resize lags". Now full min-ratio scale + absolute/origin-0,0 scaled-box centering + boot rAF re-fit (rubber-rhino fit() reference; frontend-developer pass). ⚠ **upstream: GUI-Claude must fold into the face master** (handoff-overwrite class, like the 2.4.1 preset-load fix). |
| TD-001 (0 dBFS noise) | ✅ **FIXED in 2.5.2** (root cause + full story: validation/TD-001-REPORT.md; user notes: RELEASE-NOTES.md; CHANGELOG 2.5.2): one-past-end heap read in Ensemble/Dimension/Rotary `read()` (float wrap rounds pos to exactly len_), deterministic on SOLINA FIELDS ~14 s @44.1 k; reverb trapped the burst, limiter pinned it at ceiling. + D13 pure-NaN blindness fixed (fmax swallows NaN; finite ≠ NaN per user), flushFx/prepareToPlay/StereoWidth reset completeness. New harness `fx_read_bounds` (validator 11/11); end-to-end 2× vst3-probe suite clean. Soak tool: C:\code-bank\tools\vst3-probe. Tracer `DREAMER_TD001_TRACE` compiled-out — strip after 2.5.2 soaks green. Ported ModFx.h/Effects.h carry the same hazard class (on record, rule 1). Remaining from mac triage: TASK 2 (GUI resize pipeline check), TASK 3 (library v4, blocked on zip). |
| Loop tuning (fixed 2.5.1) | ✅ **RESOLVED** the v3 loop_roots.json flag. The v3 library is synthesized at 220 Hz nominal (bake_final.py: no pitch warp), but LoopRoots.h held stale **v2-measured** roots (217–223 Hz) → every tonal ENS loop played **~+13c sharp** (up to ±20c). Fix: loop_roots.json all 130 → 220.0, re-baked LoopRoots.h (all 220). Loop/hit WAVs were byte-identical to dreamer-library-v3.zip all along — only the tuning table was stale. |
| GUI (v16 face) | **v16 production face — adopts GUI_INTEGRATION_CONTRACT** (preset load wired, data fetched via getPresetList/getWaveList, relay KINDs matched, LO-FI raw routing, local fonts) → the disconnected-controller class is fixed **upstream** in GUI-Claude's app.js (no integration seds this round). Polish: radar sweep decoupled from RATE, real-FFT header, filter BALANCE layout, whole-panel host scaling. style.css uses the plugin's flat-filename @font-face (no fonts/ path / Regular-weight dep). editor.html bootstrap: check_native_interop → window.Juce(index.js) → app.js, mount #root; paths resolve by filename via getResource. |
| Global LFO 2 (v2.5.0) | new GLOBAL params `lfo2_rate`/`lfo2_shape`/`lfo2_sync` (distinct from per-tone `lfo2_*_[t]`) + a 2nd `glfo2_` in DreamSynth (free-run or tempo-synced, mirrors LFO 1). Mod-matrix sources **5→7**: `G-LFO 2` (engine srcGlfo2=6, live) + `G-Aux` (srcGAux=7, **reserved/inert** until the v17 global-aux-env tier). v17 tier (gamp/gaux/gflt_env + *_ovr) is reserved in app.js (no widgets) → no DSP yet. |
| Preset load (fixed 2.4.1) | v15 app.js had preset LOAD unwired + WAVES/PRESETS hardcoded (order ≠ presets.json). Fixed app.js: Bridge.fn('loadPreset')(i)/loadUserPreset(name) on steppers+rows+LOAD SELECTED; boot pulls getPresetList/getUserPresetList/getWaveList (authoritative). ⚠ GUI-Claude must own this upstream (next handoff would overwrite app.js) — design marked these "PRODUCTION: replace with native fns". |
| GUI (v15 face) | **NEW production WebView face integrated** (design_handoff_dreamer_gui/production: editor.html/app.js/style.css, framework-free). Replaces the v13 face. Local bundled fonts (Google Fonts CSP-blocked); bootstrap = check_native_interop.js → window.Juce (index.js) → app.js, mounts #root. PluginEditor relay lists mirror app.js GLOBAL_ID/TONE_ID+KIND exactly (incl mtx()/P-ENV/v15 adds). Native fns: getInfo/noteOn/off/pitchBend/modWheel/keyboardFold/getPresetList/getWaveList/loadPreset(→applyPreset)/getScopeData/getLimiterGR/panic/get+save+rename+delete+loadUserPreset. **2 app.js integration seds** (width_width→width, orbitVoice choice→toggle) — flagged for GUI-Claude to fix upstream ([[gui-work-via-frontend-developer]]). Contract/diff docs: design-handoff/GUI_DSP_DIFF.md + GUI_PROD_MISMATCH.md. |
| DSP align to GUI (v2.4.0) | flt2_env (F2 env like F1); lfo_sync (global LFO tempo sync); FX PARAMS focus-shadow — `<slot>_param` drives focus-selected extra for MOD/DELAY/REVERB/**TALK** (dly/rev inert), + lofi_pfocus/talk_pfocus; **LO-FI reads RAW** lofi_bits/srate/compand/alias (GUI focus-routes to them, NOT lofi_param). detune_voices Int→Choice{1..4} (engine idx+1); flt_route + fx_prepost Choice→Bool. |
| D15/D16 (v2.3.0) | per-tone PLAY MODE (hit_play, "Play Mode") now ALL wave types. LOOP+STRETCH = **LOOP RATE**: default **decoupled granular** morph sweep (PcmOsc3 2-grain Hann COLA read at note pitch + morph clock at loop_rate → pad breathes without detuning); loop_varispeed = pitch-follows; loop_rate_sync/loop_rate_beats tempo-lock; CYCLE+STRETCH no-op. New params loop_rate/loop_rate_sync/loop_rate_beats/loop_varispeed_[t]; loop_rate = matrix dest 10. Granular gated → NORMAL loops byte-identical. D16 = naming only (start_random already "Start Random"). |
| UX round (v2.2.0) | UX_DSP_TASKS D1–D14 + D11 done; GUI hand-off = design-handoff/UX_ROUND_HANDOFF.md (D15/D16 params appended). New params: semi_[t], detune_voices/detune_amount_[t], g_env_a/d/s/r, g_cutoff, g_res, g_octave, limiter_on; tvf_kf **bipolar** (−100..+100, presets migrated). Env A/D/R log 1 ms–10 s with real-unit display (preset times migrated). D2 live env-rate. GUI-facing processor API: getScopeData (D3), getLimiterGR (D12), panic (D13), resetToInit/program-0 INIT (D4), midiLearn* (D6), user-preset save/rename/delete/load + getUserPresetList (D14). D11 = per-wave **playback** loudness gain (bank untouched, −14 dBFS RMS, tools/bake_wave_norm.cpp → dsp/glue/WaveNormTable.h). |
| Bank / library | **218 waves** = 78 cycles (0-77) + 130 loops (78-207, **dreamer-library-v3**; per-family tags Pad/Airy/Vox/Ether/FM/Wind/Metal/Morph) + 10 HITs (208-217, "Hit"). Names/order LOCKED across v1/v2/v3 (indices = preset refs = wave-choice param). LoopBankData.h ≈102 MB (LFS) / 28 MB int16; ShotBankData.h ≈0.8 MB. WAV source assets/loops (130) + assets/shots (10); bake via tools/bake_*_header.cpp (no python). Per-loop tuning: dsp/bank/LoopRoots.h — **all 130 = 220.0** (v2.5.1 fix). The v3 library is synthesized at 220 Hz nominal (bake_final.py: no pitch warp), so no per-loop root correction is needed; loops play in tune at their note. (Pre-2.5.1 held stale v2-measured roots → detuned; now loop_roots.json = all 220.) 12-bit low-nibble invariant asserted. |
| Engine | v3+v7 + FX v1.5 (modfx 7 modes; LO-FI/WIDTH/TALK; per-slot PARAMS p0..p3; fx_prepost; meters) + per-tone §11 VOICING multi-tap osc (Single/Oct/Power/Dreamy+spread; Tone owns 4 osc taps, equal-power, fans osc only) / §12 LOOP MODE Fwd-Pingpong / §13 HIT STRETCH varispeed. Gain-staged: per-voice tone-sum ÷√(active tones) smoothed (⚠ **4-tone patches quieter by design** — lever = `kVoiceHeadroom` 0.5), equal-power delay/reverb crossfades, OutputStage.h tanh soft-clip + −0.1 dBFS ceiling. Matrix dest 9 (FX PARAM reserved/inert). |
| Global filters (14) | 0-3 SVF (LP24/LP12/BP/HP), 4-6 Rhino ports (Liquid/Classic/Ladder), 7-12 FilterExtra (Notch/Comb±/N+LP/Formant/Allpass), **13 DreamPlane = E-MU Z-plane morph filter** (dsp/glue/ZPlaneFilter.h, 12-pole/3 frames GLASS/VOWEL/BRITE, MORPH=flt2_morph, CUTOFF shift, RES sharpen). Res^0.35 perceptual curve on the Rhino path so CLASSIC/LIQUID sweep smoothly. ⚠ **LADDER stays mild** — weak resonator in the port, can't ring without a ported-math change (rule 1). DreamPlane uses a credible GENERIC Z-plane frame set — exact E-MU Xtreme Lead frame coefficients could later come from the X-Lead/prodatum data ([[prodatum-vst3-project]]) if precise fidelity wanted. |
| Presets | **47 factory presets (WORKING)** — processor-owned (BinaryData::presets_json, parsed at ctor), standard program interface (Cubase preset menu, 47) + in-GUI browser (native getPresetList/loadPreset → applyPreset, type-dispatched; setStateInformation snaps bools to 0/1 so state round-trips at pluginval 8). 219 preset ids ⊂ 281 APVTS (0 unknown; tools/check_presets.sh). |
| GUI | **v13 face, REGENERATED from the handoff master (not renovated)** — locked 1140×660 (collapsible 848, collapsed default; PluginEditor kBaseH=848/kFoldedH=660, aspect 1140/660, limits 570,330,2280,1320). Resize ONLY via the host window border (fit() scale = min(innerW/1140, innerH/currentBaseH); no grip). Real bank wave names via getWaveList() native fn. Preset browser wired. Bridge = fixed plumbing (getSliderState/getToggleState relays, getNativeFunction getInfo/keyboardFold/noteOn/getPresetList/getWaveList, check_native_interop.js classic script + index.js). Relay parity 281 ids ↔ PluginEditor.cpp. MIDI LEARN rendered but non-functional (deferred). |
| ⚠ Scope / lanes | GUI DESIGN is GUI-Claude's; DSP direction is R&D-Claude's; THIS session = architecture + DSP + VST3 build+validate+deploy. GUI is **REGENERATED from the handoff, never renovated**; GUI data that is a DSP-contract datum (wave/preset names) comes from the processor via a native fn, never hardcoded. See [[gui-work-via-frontend-developer]]. |
| Flagged deviations | FX-PARAM matrix dest reserved/inert (no GUI focus target yet); MIDI learn deferred; hit_stretch NOT a matrix dest (DSP_BUILD §9 wins → FX PARAM); STRETCH detaches one-shot from key pitch (authentic varispeed); dly/rev per-slot PARAMS reserved/inert (ports); WIDTH fixed POST; loop-seam report-only (delivered material can't meet the literal seam criterion — PINGPONG is the remedy); tvf_env/flt1_env unipolar. |
| Pending | **user Cubase ear+EYE pass on 2.4.0** (FULL RE-SCAN first). v2.4.0 EYE: the NEW v15 face loads + all controls drive params (verify F2 ENVELOPE, GLOBAL offset-mode, DETUNE, LOOP RATE on ENS waves, MOD MATRIX moves the sound, P-ENV EDIT modal, FX PARAMS focus, MIDI-learn right-click, SPECTRUM, preset SAVE/USER bank, SOUND OFF). Known GUI quirks to confirm: LO-FI uses raw named knobs (not the proxy); dly/rev PARAMS knobs are inert (ported FX). v2.3.0 listen: (D15) on a LOOP/ENS wave set PLAY MODE=STRETCH → LOOP RATE breathes the texture **without detuning** (pitch stays on the note); try loop_rate_sync to tempo; loop_varispeed ON = pitch-follows; listen for grain artifacts (2-grain Hann — flag if audible on any loop). v2.2.0 listen (carried): (D2) cut RELEASE while ringing → tail shortens; (D1) ms/s LCD; (D9) detune widens without wander; (D10) −KF darkens up keyboard; (D5) global offsets; (D7/D8) semi/octave; (D11) even browse loudness (~10 dB quieter overall — say if too quiet); (D4) program 0 = INIT; (D12) limiter; (D6/D14/D3/D13) need GUI wiring. Then GUI-Claude wires UX_ROUND_HANDOFF.md (incl D15/D16). Still open: v3 loop_roots.json if re-tuned; FX-PARAM matrix wiring; exact E-MU Z-plane coefficients. |
| Git | main @ v2.5.1 = loop-tuning fix (branch fix/loop-tuning-v3-roots; v2.5.0 = `73f00d9` v16 GUI + Global LFO 2; v2.4.x = v15 GUI + preset-load fix). Branch → merge --no-ff → push per commit. Sound library tracked via **Git LFS** (`.gitattributes`: `assets/{loops,shots}/*.wav`, `dsp/bank/{LoopBankData,ShotBankData}.h`) because LoopBankData.h ~102 MB > GitHub's 100 MB limit. Fresh clone needs `git lfs pull` before building. Share the-dreamer-src\ export = working-tree robocopy (NOT `git archive` — that exports LFS pointer stubs). |

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
  NOTE two shared-tool gotchas hit this session — see memory
  [[release-ship-tool-gotchas]] (release.ps1 -DryRun leaves CMakeLists bumped;
  ship.ps1 push fallback hardcodes origin main → drive git by hand on branches).

- Donor: C:\rubber-rhino (READ-ONLY). Bank source of truth: files\rompler-bank-v2\
  (copied verbatim to dsp\bank\ — READ-ONLY, 12-bit grain is data).
- deps\JUCE = JUCE 8.0.4 source cache (gitignored; robocopy from
  C:\rubber-rhino\deps\JUCE on a fresh clone — robocopy needs BACKSLASH paths).
- Configure: `cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/the-dreamer/deps/JUCE`
- Tests: `cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_X.cpp /Fo:tests\bin\ /Fe:tests\bin\test_X.exe`
  (add /DRHINO_TEST_FEATURES=1 for anything including ported FX; run vcvars64 first).
  Harnesses set FTZ/DAZ themselves (`_mm_setcsr(_mm_getcsr() | 0x8040)`).
- Global filters: 0-3 built-in SVF (ToneSvf), 4-6 Rhino ports (RhinoFilter, via
  GlobalFilter; res^0.35 perceptual curve applied in GlobalFilter, NOT the port),
  **7-12 glue FilterExtra** (Allpass now audible), **13 DreamPlane = glue
  ZPlaneFilter** (E-MU Z-plane morph; GlobalFilter.setMorph(flt2_morph)). All
  driven cutoff(Hz)+res(0..1) at gfCtrl control rate; GlobalFilter.process()
  clamps every path through safety().
- Known bank defect (fix lives in glue only): Mode2Synth steal-stamp bug — see
  CLAUDE.md scope decision 2.
- EnvelopeAdsr release is exponential with a 1e-4 idle gate: a 1.2 s release
  needs ~11 s to reach silence — size tail-silence test windows accordingly.
- Deploy: whole "The Dreamer.vst3" bundle → \\VBOXSVR\vagrant\ root
  + C:\the-dreamer\; refresh share the-dreamer-src\ export after each push.
- v2 engine facts: waveshaper LUTs are BAKED — edit tools/bake_shapers.cpp
  then rerun it to regenerate dsp/glue/ShaperTables.h (no python on the VM,
  by design). Matrix param choices have NO "-" entry (GUI parity): engine enum
  = param index + 1, slot inert at amt 0.

## STATUS log

Moved to **CHANGELOG.md** (newest first). The STATE block above is
current-only per the docs-hygiene rule.
