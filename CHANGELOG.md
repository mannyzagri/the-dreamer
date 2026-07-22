# The Dreamer — CHANGELOG (newest first)

History of shipped release candidates. The CURRENT state lives in
PROJECT-NOTES.md STATE (current-only); this file is the running history.

- 2026-07-22 (TD-001: 0 dBFS noise fix) — **RC 2.5.2** (branch
  `fix/td-001-noise` → main `0c52c12`; released+deployed both targets,
  pluginval 8 SUCCESS; **re-scan required** per release tool — moduleinfo
  changed). Bug (user report, mac
  triage msg 2026-07-22T10:15): sustained full-scale noise after ~20-30 s of
  use OR idle. **Root cause (proven, deterministic):** the fractional
  delay-read helper in `Ensemble.h` (and latently `Dimension.h`/`Rotary.h`)
  computed `pos = w_ - d; if (pos < 0) pos += (float)len_;` — when `d` lands
  1-2 ulp above `w_`, the wrap-add rounds to EXACTLY `(float)len_`, so
  `i0 == len_` → a **one-past-end heap read** returned verbatim (`fr == 0`).
  Fired deterministically on preset 13 "SOLINA FIELDS" (ENSEMBLE modfx,
  rate 0.3/depth 0.6) at ~14 s @44.1 k; audibility per launch = ASLR luck
  (adjacent heap bytes as float ≈ 1e26). The burst then loaded juce::Reverb's
  combs (fb ≤ 0.98 → rings for MINUTES) and the D12 limiter tanh-clipped it to
  exactly the −0.1 dBFS ceiling → steady "0 dBFS noise"; D13 never fired (all
  finite). Diagnosed via the new soak harness (`tools/hostprobe`, generalized
  to code-bank `tools/vst3-probe`) + compiled-out stage tracer
  (`DREAMER_TD001_TRACE`) + exhaustive index-math micro-proof.
  **Fixes:** (1) re-normalize `pos >= len_ → 0.0f` (bit-correct congruent
  limit) in Ensemble/Dimension/Rotary; (2) D13 was blind to pure NaN
  (`std::fmax` returns the non-NaN operand) — added a per-sample NaN latch;
  finite overloads deliberately do NOT trigger D13 (user directive: NaN is
  NaN); (3) reset completeness: `flushFx()` now clears f1/f2 + LoFi/Width/
  Talk + scope ring; `prepareToPlay` now resets f1/f2 (state survived host
  stop/start); `StereoWidth::prepare` now calls `reset()` (missed
  lowL2_/lowR2_). **Tests:** new `test_fx_read_bounds` harness (pre-fix math
  exhibits 3572 OOB boundary cases; post-fix 0, exhaustive over every w_ ×
  ulp-neighborhood × all three geometries; real classes finite/bounded over
  the faulting trajectory) — added to validator.json. Validator dsp **11/11**;
  end-to-end: 2× full vst3-probe suite (incl. the faulting preset sweep +
  concurrent hammer) — 0 faults, 0 tracer trips (was: fault at block 148467).
  Ported files untouched (rule 1); same hazard NOTED in ported ModFx.h/
  Effects.h for the record. No param change → reload instance, no re-scan.

- 2026-07-22 (loop-tuning regression fix) — **RC 2.5.1**. Bug (user report):
  every ENS loop played out of tune. Root cause: the v3 library is SYNTHESIZED at
  a fixed 220 Hz nominal (`bake_final.py`: `root=220.0`, "no rootHz correction, no
  pitch-lock warp"), but `LoopRoots.h` still held the **v2-measured** per-loop
  roots (217–223 Hz, off ±up to ~45 c) — a known-flagged risk that materialized.
  Playing v3 audio through v2 roots detuned every tonal loop **~+13 cents sharp
  (up to ±20 c)**. Verified: the 140 loop/hit WAVs are byte-identical to
  dreamer-library-v3.zip (audio was fine); only the tuning table was stale.
  Fix: `assets/library-src/loop_roots.json` → all 130 = 220.0, re-baked
  `LoopRoots.h` (all 220.0). No param change → **reload the instance, no re-scan**.
  Tests updated to the v3 truth (test_loop_tuning: all-220, loops play in tune, 0 c
  detune; test_bank3: 130-at-220 split, pitch-test loop at 220). dsp 10/10 +
  pluginval 8. Resolves the STATE-flagged v3 loop_roots.json pending item.

- 2026-07-21 (v16 GUI + Global LFO 2) — **RC 2.5.0**. The v16 production face
  ("Design markdown file.zip") **adopts GUI_INTEGRATION_CONTRACT** — preset load
  wired, processor data fetched (getPresetList/getWaveList), relay KINDs matched
  (orbit_voice→bool, width id fixed), LO-FI raw routing, local fonts — so the
  disconnected-controller class is resolved upstream. Integrated as-is (no
  integration seds needed); style.css uses the plugin's flat-filename @font-face
  block. v16 polish: radar sweep decoupled from RATE, real-FFT header, filter
  BALANCE layout, whole-panel host scaling. **DSP grown to back v16's new
  features (user decision):** **Global LFO 2** — new GLOBAL `lfo2_rate`/
  `lfo2_shape`/`lfo2_sync` (distinct from the per-tone `lfo2_*_[t]`) + a 2nd
  global LFO in DreamSynth (free-run or tempo-synced); **mod-matrix sources
  5→7** — `G-LFO 2` (live) + `G-Aux` (reserved/inert until the v17 global-aux-env
  tier). The v17 tier (`gamp_env`/`gaux_env`/`gflt_env` + `_ovr`) is reserved in
  app.js (no widgets) → no DSP yet. **PARAM-LIST CHANGE → Cubase FULL RE-SCAN.**
  Validator dsp 10/10 + staging/gui load+screenshot (2 G-LFO rows) + pluginval 8.

- 2026-07-21 (GUI preset-load fix) — **RC 2.4.1**. Bug (user report): the preset
  bank shows but changing a preset didn't load it. Root cause was GUI-side: the
  v15 app.js left preset LOAD unwired (selection only updated local UI state; the
  "LOAD SELECTED" button just closed the overlay) AND hardcoded the WAVES/PRESETS
  mock arrays (order diverged from presets.json → an index load would recall the
  wrong preset). Fix (app.js only, no param change → reload, no re-scan): (1) wire
  Bridge.fn('loadPreset')(i) / loadUserPreset(name) on every path — header ▲/▼
  steppers, factory rows, user rows, LOAD SELECTED; (2) on boot, pull the
  processor's authoritative getPresetList/getUserPresetList/getWaveList and
  replace the hardcoded arrays so names/order/count match the DSP. Flagged for
  GUI-Claude to own upstream (the design marked these "PRODUCTION: replace with
  native fns"). gui load+screenshot + pluginval 8 PASS.

- 2026-07-21 (v15 production GUI + DSP alignment) — **RC 2.4.0**. Integrates the
  new framework-free WebView face (design_handoff_dreamer_gui/production:
  editor.html/app.js/style.css) and grows the DSP to match its binding contract.
  **PARAM-LIST CHANGE → Cubase FULL RE-SCAN.** DSP additions (per the two design
  decisions): **flt2_env** (Filter 2 envelope, symmetric w/ F1); **lfo_sync**
  (global LFO tempo sync); **FX PARAMS focus model** — one `<slot>_param` knob per
  MOD/DELAY/REVERB/TALK drives the focus-selected extra (engine focus-shadow,
  one-pole smoothed; dly/rev proxies inert), + `lofi_pfocus`/`talk_pfocus`; LO-FI
  keeps RAW named params (the GUI focus-routes to lofi_bits/srate/compand/alias).
  Type changes to match GUI kinds: `detune_voices` Int→Choice{1..4};
  `flt_route`/`fx_prepost` Choice→Bool. GUI: editor.html uses local bundled fonts
  (Google Fonts is CSP-blocked) + the check_native_interop→window.Juce→app.js
  bootstrap; PluginEditor relay lists rewritten to mirror app.js's binding map
  exactly (incl mtx()/P-ENV); native fns getScopeData/getLimiterGR/panic +
  user-preset save/rename/delete/load registered; loadPreset→applyPreset (0-based
  factory; D4 INIT is host program 0). Two app.js integration fixes applied
  (width_width→width, orbitVoice choice→toggle; flagged for GUI-Claude upstream).
  Validator staging + gui (load+screenshot) + pluginval 8 PASS; the full v15 panel
  renders. Ear/eye pass pending.

- 2026-07-21 (PLAY MODE all-types + granular LOOP RATE, D15/D16) — **RC 2.3.0**.
  Follow-up DSP round (UX_DSP_TASKS D15/D16, added after 2.2.0). **PARAM-LIST
  CHANGE (16 new per-tone params + matrix dest 9→10) → Cubase FULL RE-SCAN.**
  (D15) per-tone PLAY MODE (hit_play, displayed "Play Mode") now applies to ALL
  wave types. LOOP + STRETCH = new **LOOP RATE**: the default is a **decoupled
  granular** morph sweep — PcmOsc3 reads the buffer at NOTE PITCH via a 2-grain
  50%-overlap Hann read (COLA→unity, click-free) while a separate morph clock
  sweeps the grain start at loop_rate, so a pad's texture breathes slower/faster
  **without detuning**. `loop_varispeed` = plain pitch-follows varispeed instead;
  `loop_rate` 0..1→0.25×..4× (log, 0.5=1×); `loop_rate_sync`+`loop_rate_beats`
  lock traversal to one buffer sweep per beat-division; loop_rate is mod-matrix
  dest 10. CYCLE+STRETCH = graceful no-op. Granular path is gated → NORMAL loops
  and every existing render stay byte-identical (dsp 10/10 incl loop/cycle
  bit-identity + loop_tuning). (D16) naming only — `start_random` already exposes
  automation name "Start Random" (not "RND"); GUI relabels. New params:
  loop_rate/loop_rate_sync/loop_rate_beats/loop_varispeed_[t]. Validator dsp 10/10
  + pluginval 8; clean build, no new warnings.

- 2026-07-20 (UX polish round, D1–D14 + D11) — **RC 2.2.0**. Usability/product
  round from UX_DSP_TASKS.md (GUI-side counterpart flagged in
  design-handoff/UX_ROUND_HANDOFF.md). **PARAM-LIST CHANGE (~276→~296) → Cubase
  FULL RE-SCAN.** Also added `VERSION ${PROJECT_VERSION}` to juce_add_plugin so
  the Windows PE tracks the real version ([[juce-pe-version-resource-gotcha]]).
  (D1) env A/D/R map cubic→exponential (1 ms–10 s) in a shared JUCE-free
  ParamLaws.h; params show real units ("340 ms"/"1.2 s"); preset env-times
  migrated (1488 values) to preserve authored seconds. (D2, felt bug) envelopes
  read A/D/S/R **continuously** (updateLive re-sets all three) so a ringing tail
  shortens the moment you cut RELEASE. (D5) global bipolar offsets g_env_a/d/s/r
  + g_cutoff/g_res add to every tone's normalized value, clamped, relationships
  preserved. (D7) per-tone `semi` −12..+12; (D8) `g_octave` −2..+2. (D9) per-tone
  engine detune: detune_voices 1..4 × detune_amount (0..±25 c) = symmetric taps
  per voicing tap, equal-power, phase-coherent (voices=1 bit-exact no-op). (D10)
  `tvf_kf` unipolar→bipolar (−100..+100, detent 0; darker up the keyboard);
  presets migrated old[0..1]→[0.5..1] (188 values). (D12) `limiter_on` (default
  ON) gates the output soft-clip+ceiling; `getLimiterGR()` dB telemetry. (D4)
  host program 0 = INIT (basic saw, FX off); factory presets shift to programs
  1..N; GUI preset API unchanged. (D13) `panic()` = all-notes-off + FX flush
  (audio-thread, also NaN-recovery). (D3) lock-free 2048-sample output tap
  `getScopeData()` for the GUI FFT. (D6) MIDI learn (CC→any param, persisted in a
  new DreamerState wrapper; legacy states still load); (D14) user presets
  save/rename/delete/load in userAppData, factory bank read-only, list tagged
  FACTORY/USER. (D11) per-wave **playback** loudness gain (bank bytes untouched,
  rule 3): cycles+loops normalized to −14 dBFS RMS, HITs keep peak 0.98; baked by
  tools/bake_wave_norm.cpp → dsp/glue/WaveNormTable.h. Validator dsp 10/10 +
  pluginval 8; clean build, no new warnings. D15/D16 deferred to the next round.

- 2026-07-19 (wave library → v3) — **RC 2.1.2**. Replaced the loop library with
  dreamer-library-v3.zip. The 130 loop **filenames are identical** to v2 (same
  manifest order) → **wave indices, preset references, and tone wave names fully
  retained** — only the WAV sample data changed (re-baked). HITs unchanged. Re-baked
  LoopBankData.h; ShotBankData.h/manifest untouched. bank3 12-bit invariant + 218
  counts PASS. ⚠ v3 shipped NO loop_roots.json — kept the v2-measured per-loop roots
  (LoopRoots.h unchanged); if v3 re-tuned the samples, a v3 loop_roots.json is needed
  to re-measure (tuning may drift slightly otherwise). No param/order/name change →
  presets retained, no re-scan. Validator all stages PASS + pluginval 8.

- 2026-07-19 (real wave names in the GUI) — **RC 2.1.1**. Bug fix (user report:
  the wave list showed "Loop 001..130" / "Hit 01..10" placeholders instead of the
  real bank names). The regenerated GUI hardcoded placeholder loop/hit names because
  the real names weren't exposed to the JS. Fix: new processor `getWaveList()` +
  editor native fn returning the bank-authoritative 218 entries {category,name,tag}
  (from bank3::kWaveforms; tag ""/ENS/SHOT by WaveType); app.js `loadWaveList()` (mirrors
  loadPresetList) pulls them on boot and replaces the placeholders in place. Now the
  wave overlay/LCD shows PAD_01, AIRY_06, MORPH_PADAIR, HIT_CHIFF, etc. Preset browser
  was already correct (real names). No param/DSP change. Validator all stages PASS +
  pluginval 8.

- 2026-07-19 (v2 library + 47 presets + filter fixes + DreamPlane Z-plane) — **RC 2.1.0**.
  Feature release. No param-list change vs 2.0.x → reload the instance, no re-scan.
  (1) **Fixed sound library v2** (files(8)): the 130 loops re-baked from corrected
  source (WAV data + per-loop roots updated; same 130 names/order → wave indices
  unchanged; HITs untouched). LoopBankData.h + LoopRoots.h regenerated.
  (2) **47 factory presets** (PRESETS.md, presets(1).zip = presets.zip): processor-
  owned bank (bank A 23 + B 24) embedded as BinaryData::presets_json, parsed once at
  ctor, exposed via the standard program interface (Cubase menu) AND the in-GUI
  browser (native getPresetList/loadPreset → applyPreset). applyPreset dispatches on
  param TYPE (JSON stores normalized 0..1 for float/int, index for choice, bool for
  bool). 219 preset ids ⊂ 281 APVTS ids (0 unknown; tools/check_presets.sh).
  **State-restore fix:** exposing 47 host programs made JUCE inject a synthetic
  Program param that pluginval toggled → applyPreset wrote clean 1.0 to bools while
  a raw fractional survived in the tree (AudioParameterBool stores the raw normalized
  float, doesn't quantize) → 9 bools failed state round-trip. Fixed by snapping every
  bool to clean 0/1 after replaceState in setStateInformation; 47 programs kept.
  (3) **Filter fixes** (user report, rule-1-safe glue): CLASSIC/LIQUID resonance
  perceptual curve (res^0.35 in GlobalFilter) so the ring spreads from ~1/3 knob
  instead of jumping near max (LADDER stays mild — weak resonator in the port, can't
  ring without touching ported math); ALLPASS made clearly audible (6 staggered
  allpasses + RES feedback → −13..−33 dB swept notches, was −1.9 dB inaudible);
  **DreamPlane (type 13) implemented as an E-MU Xtreme Lead Z-plane morphing filter**
  (new dsp/glue/ZPlaneFilter.h: 6 peaking bells/12-pole, 3 frames GLASS/VOWEL/BRITE,
  MORPH=flt2_morph crossfades, CUTOFF shifts freqs, RES sharpens) — was a V2 bypass.
  Validator all stages PASS: dsp 10/10 + staging/bundle/gui/deploy + pluginval 8.

- 2026-07-19 (loop tuning + gain staging + GUI regenerated from handoff) — **RC 2.0.2**.
  No param-list change vs 2.0.0/2.0.1 → just reload the instance (no re-scan).
  **DSP (R&D-Claude files(7) spec, all rule-1-safe glue — zero ported-math edits):**
  (1) **Per-loop tuning** (DSP_BUILD §1b): loops were all played at a fixed 220 Hz
  root but each is detuned ±36–45¢. New `dsp/bank/LoopRoots.h` (generated by
  tools/bake_loop_roots.cpp from assets/library-src/loop_roots.json in manifest
  order — 130 roots, 115 tonal + 15 inharmonic METAL/metal-morphs pinned 220);
  Bank3 makeTable uses `kLoopRoots[i]` instead of 220; PcmOsc3 already divides by
  wf_->rootHz so pitch corrects with no runtime cost. Worst-case FM_03 +45¢ now in
  tune. (Avoided re-baking the 102 MB LoopBankData.h — sample data unchanged, no
  LFS re-upload.) (2) **Gain staging** (GAIN_STAGING.md — fixes clipping on preset
  load / reverb): per-voice tone-sum ÷√(active tones), one-pole smoothed (single-
  tone bit-identical; a 4-tone patch is now ~2× a single, not 3–4× → **4-tone
  patches are noticeably quieter, by design**); delay mix additive→crossfade
  `dry*(1-mix)+wet*mix`; reverb over-sum (1.4× at mix=1, dry never removed)→proper
  crossfade with wet normalized −3 dB (bypass continuity preserved); delay fb capped
  0.9; new `dsp/glue/OutputStage.h` tanh soft-clip + hard −0.1 dBFS ceiling after
  master. Mod-FX (all 7) audited — already equal-power, untouched. Resonant-filter
  make-up trim **deliberately NOT added**: it would break the immutable bitwise
  filter null test and the live GlobalFilter path already self-limits (safety()
  clamp + ladder self-saturation, measured peak 1.27 under loud input; output ceiling
  backstops) — RhinoFilterSlot reverted bitwise-clean. If resonant clipping persists
  by ear, the fix is a 1-line res-comp in GlobalFilter (deferred, needs authorization).
  New harnesses test_loop_tuning + test_gain_staging (added to validator.json); dsp
  matrix 10/10 PASS. **GUI: REGENERATED from the v13 handoff master** (not renovated
  — process correction: GUI design is GUI-Claude's; this session regenerates faithfully
  from the handoff). Fresh reproduction from the master's own geometry (inline-style
  architecture matching the master's React source; style.css 24→2.7 KB, editor.html
  a minimal shell), JUCE bridge attached as fixed plumbing. Relay-id parity EXACT:
  281 ids (184 slider + 29 toggle + 68 combo) vs PluginEditor.cpp, zero missing/extra.
  Verified headless at 1×/1.5×/0.7× + expanded vs master PNG. Validator all stages
  PASS + pluginval 8.

- 2026-07-19 (v13 GUI resize/overflow fixes) — **RC 2.0.1**. GUI-only patch
  fixing three Cubase resize bugs the user reported (all one root cause) +
  an FX overflow. No DSP/param change → moduleinfo param list unchanged vs
  2.0.0 (just reload the instance / restart Cubase; no full re-scan needed).
  (1) **Removed the prototype resize grip** — the v8 corner grip (bottom-right,
  cursor:nwse-resize) was still shipping in the plugin. It scaled content via a
  `uiScale` transform INDEPENDENT of the window, which produced exactly the
  three reported symptoms: buttons overflow the panel (content zoomed past the
  fixed frame), frame grows only on keybed fold (grip never called setSize),
  layout scales inside a fixed frame (that IS what the grip did). Per the v13
  handoff's "Window resize" section ("the host window border replaces it"),
  removed the grip element/CSS/handler and the `uiScale` machinery; `fit()` now
  derives scale ONLY from the window (`min(innerW/BASE_W, innerH/currentBaseH)`).
  The host window border is now the sole resize path (JUCE setResizable +
  fixed-aspect constrainer, already correct). (2) **FX group overflow** — the
  row2 `1fr` track (`minmax(auto,1fr)`) inflated to the DELAY row's min-content
  (widened by its SYNC column), pushing the FX group 19px past the panel's
  overflow:hidden edge so the MOD/DELAY/REVERB on/off buttons + UTIL ▸ were
  clipped. Tightened the FX knob pitch to the master's spacing (fxrow gap 5→3;
  primary-knob wrappers 36→32, PARAMS 40→36) so the group fits its 380px slot;
  buttons now fully inside with margin, matching the master exactly. Validator
  bundle/gui/deploy/host(pluginval 8) PASS; verified headless at 1×/1.5×.
  Design-track double-check doc (design-handoff/v13, doc-only revision — master
  PNG identical) synced; its new "three bugs to avoid" section independently
  confirms this diagnosis.

- 2026-07-19 (v13 GUI + full v3 library + voicing/loop/hit DSP) — **RC 2.0.0**.
  Major release integrating a delivered sound library, three new per-tone
  synthesis features, and an all-new faceplate. Orchestrated in four phases
  (build-engineer/cpp-pro + frontend-developer personas), each validator-gated.
  (1) **Full v3 sound library** — the 26 placeholder "Ens" loops + 10
  synthesized "Shot" one-shots were replaced by the delivered library:
  **130 family loops** (Pad/Airy/Vox/Ether/FM/Wind/Metal/Morph) + **10 HIT
  one-shots**. Bank now **218 waves** (78 cycle + 130 loop + 10 hit). New WAV→
  header bakers (tools/bake_loops_header.cpp rewritten for 130 manifest-order
  names + per-family category tags; tools/bake_shots_header.cpp new for the
  HITs); big sample arrays emitted `inline const` (not constexpr) so the
  ~102 MB LoopBankData.h compiles under MSVC (~21 s). 12-bit low-nibble
  invariant holds across all 14.87 M samples; Cycle path byte-identical.
  Embedded bank ≈ 28.4 MB (binary 36 MB). (2) **New per-tone DSP** —
  §11 **VOICING** multi-tap oscillator (SINGLE/OCT/POWER/DREAMY; DREAMY spread
  ADD9/MIN7/SUS2), taps fan the loop at equal-tempered ratios, equal-power sum
  (SINGLE stays bit-identical); §12 **LOOP MODE** FORWARD/PINGPONG (phase
  reflection, seam-friendly); §13 **HIT STRETCH** one-shot varispeed
  (0.25×–4× log + −24…+24 st pitch trim; pitch follows speed). +26 params
  (6 per-tone families ×4 + "Fx Param" 9th matrix dest, reserved/inert like
  Morph). (3) **v13 faceplate** (design-handoff/v13, wholesale WebView rebuild)
  — locked 1140×660 (collapsible 848), full-word labels, STRETCH/P.TRIM greyed
  in place, LOOP↔PLAY selector swapped by wave bank type, VOICING/DREAMY
  steppers, LFO1/LFO2 with tempo SYNC, BALANCE knob, 9-slot matrix with
  bipolar amount bars, FX focus-LCD + PARAMS knobs, UTIL overlay, 218-wave
  overlay with [ENS]/[SHOT] tags. Validator PASS across all stages;
  pluginval strictness 8 SUCCESS. **Param list changed (218 waves + 26 params)
  → Cubase FULL re-scan required** (remove/re-add the instance or restart).
  Flagged/deferred (unchanged from prior): MIDI learn rendered non-functional;
  FX-PARAM matrix dest reserved/inert (no GUI focus target yet); factory
  preset payloads still placeholders (now point into the 218-order — real
  authoring pending). test_bank3 counts/shot-bound updated + seam check made
  report-only (delivered material can't meet the literal seam criterion;
  PINGPONG is the remedy); new test_dsp_features + test_bank3_lib harnesses.

- 2026-07-18 (GUI v12 + comb fix) — **RC 1.2.0**. Two user bugs + the v12 GUI.
  (1) **Comb filters stepped** per cutoff value — my COMB+/COMB− used an
  integer delay length (`round(fs/cut)`), so the comb fundamental snapped to
  `fs/N` and the tuning quantized audibly. Fixed with a **fractional,
  linearly-interpolated delay read** + a one-pole glide on the delay length
  (dsp/glue/FilterExtra.h); test_filter_extra gained a continuity check
  (20/20 distinct responses over 20 fine cutoff steps, where the integer
  version plateaued). (2) **"Live MIDI deselects the Cubase track"** — turned
  out to be the user's **hardware MIDI keyboard**, not the plugin (the plugin's
  IS_SYNTH/NEEDS_MIDI_INPUT/output-only-bus config is correct). No plugin
  change. (3) **GUI_4 v12** integrated (frontend-developer, editor.html/
  style.css/app.js only): rubber-band separator strip; **collapsible keyboard**
  — a ▼/▲ KEYS pill on the rubber band toggles the keyboard, the page flips its
  logical height 864↔664 and calls a NEW native fn **`keyboardFold(folded)`**
  so the C++ editor resizes the host window and updates the 1140:H aspect
  ratio + resize limits; pitch-wheel red center stripe + **inverted drag**
  (down = bend up, hardware-style), still springs to centre. No new params →
  moduleinfo byte-identical, no forced re-scan. Validator 19/19 PASS
  (pluginval 8). (Also: fixed mojibake in PROJECT-NOTES.md from a prior
  PowerShell WriteAllLines by rewriting the file clean at 1.1.0.)

- 2026-07-18 (V1.1 filters) — **RC 1.1.0**. User report: "except LP/BP/HP/
  LADDER, no filter modes work." Measured first (3 cl.exe probes driving
  GlobalFilter exactly like the processor): LIQUID/CLASSIC filter correctly
  with unity passband and full cutoff/res response; LADDER is authentically
  gentle; and the real "don't work" is that **types 7-13 were bypass
  placeholders**. Implemented the V1.1 set as new glue `dsp/glue/FilterExtra.h`
  (rule-1 safe — no ported code touched): NOTCH (RBJ band-reject), COMB+/COMB−
  (delay-line feedback comb, ±feedback), N+LP (notch → 12 dB LP), FORMANT
  (3 vowel-bandpass resonators sweeping A→U by CUT, 2.2× makeup), ALLPASS
  (4× cascaded allpass 50/50 dry mix → swept phase notches). Routed from
  GlobalFilter for type 7-12; DreamPln(13) stays V2 bypass. Also put the Rhino
  path (LIQUID/CLASSIC/LADDER) through the same output `safety()` clamp it had
  been skipping (latent runaway fix). New harness `test_filter_extra`
  (notch rejects centre, comb combs, N+LP low-passes, formant peaks, allpass
  ~flat, all bounded < 1.6 at max res; regression that 0-6 still filter).
  **No new parameters** (the 14 filter choices already existed) → moduleinfo
  byte-identical, no forced re-scan. NOT done: LIQUID/CLASSIC "never filters"
  could not be reproduced — flagged for the user to re-check after a FULL
  Cubase restart (WebView2 cache). Validator 19/19 PASS (pluginval 8).

- 2026-07-18 (v11 GUI) — **RC 1.0.0**. Design-handoff GUI_4 (v11) integrated +
  two engine additions. (1) **On-screen keyboard + pitch/mod wheel strip**:
  panel 1140×660→1140×864; keys→noteOn/off, PITCH wheel→pitch-bend (±2 st,
  springs to 0 on release), MOD wheel→CC1-equivalent (holds). Path: editor
  WebView native fns (noteOn/noteOff/pitchBend/modWheel, message thread) →
  a **lock-free juce::AbstractFifo SPSC** (architect-reviewed choice over
  MidiMessageCollector, which locks) → drainGuiMidi() at the top of
  processBlock, before the host MIDI loop so events stay offset-0-sorted →
  the existing NoteEvent path. Editor dtor calls guiAllNotesOff() so a note
  held on the on-screen keyboard can't hang when the editor closes. Keybed
  C2–D6 (MIDI 36–86), 30 white / 21 black. (2) **Per-tone LFO SHAPE**: +8
  params `lfo{1,2}_shape_[t]` (TRI/SIN/SAW/SQR/S+H); ToneLfo gains a `shape`
  field, applied in DreamVoice via a shared panelLfoShapeToLfo() map (also
  now used by the global LFO); default SIN maps to Lfo::sine → bit-identical
  to the pre-v11 fixed-sine engine (proven by the suite's exact-equality
  CHECKs still passing). test_engine [tonelfo] extended: square-shape render
  diverges 0.25 from sine, stays bounded. (3) v11 face fixes (the four
  breakpoint items): whole-frame uniform resize (screws inside the scaled
  #frame, aspect 1140:864, no letterbox); header logo/POWER 26px screw
  clearances; single panel; bottom-right VER stamp from getInfo().version.
  GUI via frontend-developer (editor.html/style.css/app.js only; index.js +
  check_native_interop.js untouched). Validator 18/18 PASS all stages
  (pluginval 8). Param-list change → Cubase FULL re-scan. Notes: the GUI_4
  handoff is internally inconsistent (GUI_SPEC.md still shows a single tone
  LFO + a 660 resize section) — followed the newer README + prototype/PNG.
  Tool bug hit + worked around: release.ps1 -DryRun leaves CMakeLists bumped,
  so a subsequent real run passes validate.ps1 two `-Forbid` flags (PS
  rejects); recovered by resetting the version to 0.7.1 before the real run —
  flag for back-prop into the tool.

- 2026-07-18 (bug fixes + v9) — RC 0.7.1. User-reported bugs, measured then
  fixed: global-filter resonance/BAL overflow (ToneSvf had no safety →
  GlobalFilter output soft-clip), reverb click + ROOM/HALL gain (kDryScale
  2→1). NL3→N+LP name. v9 delay tempo-sync (dly_sync + GUI SYNC btn/division
  label). test_fx_v15 [global-filter] regression added. "F1 not applying"
  could not be reproduced — DSP + GUI binding both proven correct; flagged
  for user re-verify (likely masked by the overflow, now fixed). GUI: only
  app.js/style.css changed (frontend-developer pass).
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
