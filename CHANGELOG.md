# The Dreamer — CHANGELOG (newest first)

History of shipped release candidates. The CURRENT state lives in
PROJECT-NOTES.md STATE (current-only); this file is the running history.

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
