# CLAUDE.md — "The Dreamer" project: port & integration brief

Project: 90s ROMpler VST3 (JUCE, CMake, C++17, Windows 11, Cubase 15
as the validation DAW). Built from two sources:

1. **Rubber-Rhino codebase** (existing, working plugin) — donor for filters,
   effects, envelopes/mod routing, parameter plumbing, editor scaffolding.
2. **rompler-bank v2** (this repo, `dsp/bank/`) — RomplerBankData.h,
   RomplerBank.h, PcmOscillator.h, Mode2Voice.h. These are verified; treat as
   read-only unless a task explicitly says otherwise.

Read this file fully before any task. When in doubt: ask, don't improvise.

---

## Scope decisions v3 (user, 2026-07-18 — DSP_BUILD.md WINS over FEATURES.md)

design-handoff/v6/DSP_BUILD.md + GUI_SPEC.md are the current contracts
(section 9 = the canonical parameter table). Executed phases 11-14:

1. **Bank v3**: 104 waves — 78 cycles (v2, untouched) + 16 "Ens" loops
   (assets/loops WAVs + bake_loops.py recipes committed; header baked by
   tools/bake_loops_header.cpp) + 10 "Shot" one-shots (tools/bake_shots.cpp,
   synthesized deterministic). All 12-bit left-justified. dsp/bank/Bank3.h;
   PcmOsc3 (dsp/glue): v2 cycle path bit-parity, Loop/OneShot via 32.32
   phase, rootHz 220 stretch (chipmunk intentional), START 0..1 + RANDOM.
2. **Per-tone noise** (level/color, 12-bit, pre-shaper; AUX + matrix dest
   NOISE), **humanize drift** (per-voice S&H walk, global depth → ±3 cents),
   explicit 12-bit requant chain stage (section 5 order).
3. **Vector**: ORBIT SHAPE SAW/TRI/SIN/SQR/S+H, rate 0.02..8 Hz, per-voice
   free-run flag; P-ENV start/end/time/loop.
4. **ENSEMBLE** mod-fx mode (new glue, 3-tap modulated delay, spread LFO
   phases); **MASTER** = the post-FX mapped output param (0..1, def 0.78);
   fixed pre-FX headroom 0.5 replaces the old volume param.
5. **Param relock to section 9** (suffix _a.._d style, normalized 0..1;
   maps documented in PluginProcessor.cpp). FLAGGED deviations, all
   documented in Params.h: aux_amt_[t] kept (user-approved; section-9
   omission), vec_penv_loop + vec_orbit_voice added (section-6 features),
   interp/engine kept as host-only carryovers, tvf_env/flt1_env read as
   UNIPOLAR per the table (negative env unreachable — flag to spec track),
   loop-seam test criterion re-based to body-relative (the literal
   "<2 quant steps" is unsatisfiable for the delivered loop material).
6. Old validation demos (test_bank/test_mode2 renders) still pass on the
   untouched v2 bank files.

## Scope decisions v2 (user, 2026-07-18 — the FEATURES handoff supersedes v1)

The design track delivered design-handoff/FEATURES.md (+ ENVIRONMENT.md, GUI
v4 mockup, design_handoff_dreamer_gui GUI package). User-approved rebuild:

1. **4 Tones per voice** (JD-990 model) replaces the v1 2-partial engine.
   Per tone: PcmOscillator → Waveshaper (01/W LUTs) → ToneSvf TVF
   (LP24/LP12/BP/HP) → TVA; TVF/TVA/AUX ADSRs; G-LFO tap; pan; vector DIR/INT.
2. **Character filters are GLOBAL**: 2 slots on the voice-sum bus, SER/PAR,
   frozen 14-entry type list (4 SVF + LIQUID/CLASSIC/LADDER Rhino verbatim;
   NOTCH…DREAMPLN reserved → bypass until V1.1/V2 phases).
3. **One global LFO** with per-tone depth/dest taps (not per-tone LFOs);
   **Dream Vector v4** (per-tone DIR/INT, phase = manual + orbit + P-env);
   **3-slot mod matrix** (VEC PHS self-route clamped; MORPH dest reserved).
4. **CORE scope only** this pass; V1.1 filters + DreamPlane are future phases.
5. **24 voices** kept (user choice over the spec's 8-voice budget figure).
6. Bake scripts are **C++ tools** (tools/bake_shapers.cpp), not python — the
   VM keeps no python by design; ENVIRONMENT.md's python/Ninja/preset items
   are satisfied by the equivalent house toolchain (documented deviation).
7. FX = panel subset (MOD FX → DELAY → REVERB); Rubber-Rhino stages beyond
   the panel (dist, comp, clip, instability, spring, delay-sync) run at
   bit-transparent fixed defaults and are omitted from the processor.
8. Param IDs re-locked in plugin/Params.h (~180, a_/b_/c_/d_ tone blocks) —
   param-LIST change vs v0.1.0 → Cubase full re-scan.
9. Validation runs through the shared validator
   (C:\code-bank\validator\validate.ps1 -Project C:\the-dreamer; config
   validator.json at repo root).

## Scope decisions v1 (user, 2026-07-17 — superseded where v2 conflicts)

1. **DREAM is the only mode.** No Mode 1 / RHINO, no mode switch. The voice is
   the dual-partial JD-990 topology from Mode2Voice.h. The old phase 3
   (Mode 1 topology) is dropped.
2. **24-voice polyphony**, oldest-note stealing. (Known defect in the verified
   bank code: `Mode2Synth::noteOn` never assigns `stamp_` to the voice, so its
   "steal oldest" steals the least-reused voice instead of the oldest note.
   Fixed in glue (`DreamSynth` passes a global stamp); dsp/bank stays untouched.)
3. **2 LFOs per partial** (ported rhino::Lfo: sine/tri/ramp/square/S&H,
   exp 0.05–30 Hz), each with shape / rate / depth / dest (Pitch, Cutoff,
   Level) / key-trigger. Values applied at the voice's existing 16-sample
   control rate — the stepping is era-honest, do not smooth it.
4. **Full Rubber-Rhino FX chain ported verbatim** (distortion, DC block,
   16-bit truncation, stereo delay + HP/LP returns, chorus, flanger, phaser,
   spring reverb + juce::Reverb, instability, comp/limiter, brickwall), on a
   post-voice-sum stereo bus in the donor's processBlock order.
5. **RubberFilter ported without the "mod"**: the Character block
   (drive/squelch/tweet/tweak) stays compiled but permanently disabled
   (default `Character{}`) and is not parameter-exposed. `Faithful1017Filter`
   is NOT ported. All 7 filter types are exposed per partial.
6. Envelopes stay the bank's `rompler::EnvelopeAdsr` (era-honest ADSR).
7. Ported-file rule: the ONLY textual change in a ported file is namespace
   `rhino` → `dreamer` (plus deleting the unreachable Faithful1017Filter from
   RhinoFilter.h). This lets donor and port coexist in one test TU for exact
   null tests.
8. Plugin identity: product "The Dreamer", company "Menashe Audio",
   manufacturer `Mnsh`, plugin code `Drmr`, VST3 only, generic APVTS editor
   until the WebView GUI handoff lands. Parameter IDs are locked for that
   handoff (see plugin/Params.h).

---

## Non-negotiable rules

1. **NO changes to DSP math in ported files.** Filter coefficient calculations,
   saturation curves, envelope shapes, effect algorithms, denormal handling,
   oversampling factors — byte-for-byte logic identical to Rubber-Rhino.
   Allowed changes in ported DSP files: namespace, includes, class renames per
   the mapping table below, removal of dead code paths *only* if provably
   unreachable. If a port "requires" changing math, STOP and report instead.
2. **Authenticity over cleanliness.** No band-limiting the PCM oscillator, no
   interpolation upgrades, no "fixing" aliasing, no smoothing that the 90s
   hardware didn't have. PcmOscillator.h already encodes this — don't touch.
3. **12-bit bank grain is data, not a bug.** The low nibble of every bank
   sample is zero by design. Never renormalize or re-dither the bank.
4. **C++17 only.** No C++20 features (Rubber-Rhino toolchain parity).
5. **Real-time safety:** no allocation, locks, or logging on the audio thread.
   Same standard as Rubber-Rhino.
6. Never commit directly to `main`. Branch per phase: `port/<phase-name>`.
7. Every phase ends with: builds clean (`-Wall`, zero new warnings), unit
   tests pass, pluginval passes at strictness 8, and a written summary of
   every file touched and why.

---

## Repo layout (target)

    rompler/
      CMakeLists.txt
      dsp/
        bank/            # rompler-bank v2 headers -- READ-ONLY
        ported/          # Rubber-Rhino DSP, ported verbatim (rule 1 applies)
        glue/            # adapters written new for this project
      plugin/            # JUCE processor/editor
      tests/             # unit + null tests, standalone console targets
      validation/        # reference renders, null-test scripts

## Class-name mapping (filled 2026-07-17; all ports keep class names, only the namespace changes rhino → dreamer)

| Rubber-Rhino source                                        | Ported name / location |
|------------------------------------------------------------|------------------------|
| Source/dsp/RubberFilter.h — `rhino::RubberFilter`, `Type`, `Character` | dsp/ported/RhinoFilter.h (`dreamer::RubberFilter`; Faithful1017Filter class DELETED — unreachable; Character kept but permanently disabled) |
| `RubberFilter::Type` enum {classic,liquid,scream,plane,ladder,ms20,wasp} | keep values & order EXACTLY (7-entry choice param, frozen) |
| Source/dsp/Envelope.h (FilterADEnv/VolumeADEnv)            | NOT ported — envelopes stay rompler::EnvelopeAdsr (scope decision 6) |
| Source/dsp/Lfo.h — `rhino::Lfo`                            | dsp/ported/RhinoLfo.h (`dreamer::Lfo`) |
| Source/dsp/Effects.h (Distortion, DCBlocker, Truncate16/12, StereoDelay) | dsp/ported/fx/Effects.h |
| Source/dsp/ReturnFilter.h                                  | dsp/ported/fx/ReturnFilter.h |
| Source/dsp/ModFx.h (ModDelayFx, Phaser)                    | dsp/ported/fx/ModFx.h |
| Source/dsp/SpringReverb.h                                  | dsp/ported/fx/SpringReverb.h |
| Source/dsp/Instability.h                                   | dsp/ported/fx/Instability.h |
| Source/dsp/Dynamics.h (CompLimiter, BrickwallClip)         | dsp/ported/fx/Dynamics.h |
| Source/dsp/Voice.h, MorphOscillator.h, Sequencer.h, WaveMap.h | reference only — NOT ported (single-mode decision) |
| APVTS layout helpers                                       | plugin/Params.h |

New glue (this project, not ports): dsp/glue/RhinoFilterSlot.h (FilterSlot
adapter over dreamer::RubberFilter), dsp/glue/DreamVoice.h (adapted copy of
Mode2Voice.h with 2 LFOs/partial, pitch bend, live-param updates, steal fix,
fixed 24 voices).

---

## Phase plan (rewritten 2026-07-17 per scope decisions; executed by Fable 5.
## Historical dual-mode phases kept below for reference — superseded where they
## mention Mode 1 / mode switch.)

1. port/scaffold — layout, bank copy, deps/JUCE, CMake configure, test_bank. DONE 2026-07-17.
2. port/filter — RhinoFilter.h + RhinoFilterSlot.h + test_filter_port. DONE 2026-07-17 (bitwise parity).
3. port/fx — RhinoLfo.h + fx/* + test_fx_port. DONE 2026-07-17 (bitwise parity per class).
4. port/voice — DreamVoice.h + test_voice + demo renders. DONE 2026-07-17.
5. port/plugin — Params.h + PluginProcessor + generic editor. DONE 2026-07-17 (Release green).
6. port/validation — pluginval strictness 8 SUCCESS (incl. 44.1/48/96k sweep), RC 0.1.0 deployed local + share. DONE 2026-07-17 — see validation/VALIDATION.md. Cubase manual pass pending (user).

### Phase 1 (historical) — Filter port + FilterSlot adapter   [Sonnet 4.6]
- Copy Rubber-Rhino filter classes into `dsp/ported/` per mapping table.
- Write `dsp/glue/RhinoFilterSlot.h`: adapter implementing the FilterSlot
  interface from Mode2Voice.h:
      setSampleRate(double) / setCutoffRes(double hz, double res01) /
      process(float) / reset()
  The adapter maps hz+res01 onto the Rubber-Rhino filter's native parameter
  space. Conversion happens IN THE ADAPTER; the ported filter is untouched.
  Expose filter-type selection on the adapter (all Rubber-Rhino types must be
  reachable in both modes — project requirement).
- Test: `tests/test_filter_port.cpp` — impulse responses of ported filter
  (driven directly, native params) vs. via adapter (equivalent params) match
  within -120 dB RMS. Plus a sweep-stability test at res=max.

### Phase 2 — Mode 2 integration   [Sonnet 4.6]
- Wire RhinoFilterSlot into Partial via setFilter(); one adapter instance per
  partial, owned by the voice.
- Replace the ADSR stubs ONLY if Rubber-Rhino envelopes are ported by then;
  otherwise keep EnvelopeAdsr (it is era-honest).
- Test: render the bell + string-pad demo patches from test_mode2.cpp through
  the Rhino filter; verify finite output, tail silence, and no allocation on
  the audio thread (wrap with an allocation-detecting new/delete in the test).

### Phase 3 — Mode 1 topology   [Sonnet 4.6, escalate on sound mismatch]
- Recreate the Rubber-Rhino voice topology (its osc→filter→amp routing and
  mod connections) as `dsp/glue/Mode1Voice.h`, but with PcmOscillator as the
  oscillator. Keep the Rubber-Rhino mod-routing semantics exactly.
- Acceptance: with waveform = bank Saw and matched settings, Mode 1 must
  null against a Plastic-Rhino render to the extent the oscillators allow
  (PCM saw vs. VA saw differ; see validation section for the filter-isolation
  null test that removes the oscillator from the comparison).

### Phase 4 — Effects port   [Sonnet 4.6]
- Port the Rubber-Rhino FX section verbatim into `dsp/ported/fx/`; shared
  post-voice bus, available in both modes. Rule 1 applies in full.

### Phase 5 — Plugin shell   [Sonnet 4.6]
- JUCE AudioProcessor: APVTS with mode switch, per-partial params (Mode 2),
  Mode-1 param set mirroring Rubber-Rhino, waveform-select (category + name
  from bank::kWaveforms), filter type, FX params.
- Mode switch: rebuild voice graph on the message thread, swap atomically;
  kill active voices on switch (hardware-honest behavior).
- Editor: generic APVTS editor is fine for now (custom UI is a later project;
  Figma assets exist).

### Phase 6 — Validation & release candidate   [Opus 4.8 / Fable 5 review]
- Full checklist below; produce VALIDATION.md with results.

---

## Validation checklist (run before calling any phase done)

- [ ] Windows x64 Release build, zero warnings introduced by the port
- [ ] pluginval strictness 8 passes (VST3)
- [ ] Filter null test: white-noise file through Rubber-Rhino filter
      (reference render, committed to `validation/ref/`) vs. same file through
      ported filter offline — residual below -100 dB RMS per filter type
- [ ] FX null test: same approach per effect
- [ ] Pitch: bank Sine at A440 zero-crossing count = 440 ±1 over 1 s
- [ ] 12-bit grain intact: assert (sample & 0xF) == 0 across the bank
- [ ] No audio-thread allocation (allocation-guard test in tests/)
- [ ] Denormal behavior identical to Rubber-Rhino (FTZ/DAZ flags set the same)
- [ ] Sample-rate sweep: 44.1k / 48k / 96k render without artifacts
- [ ] Cubase 15 manual pass (human): load, automate mode switch + cutoff,
      bounce offline == realtime

## Escalation policy (model choice)

- Default model: **Sonnet 4.6**. The phases above are specified precisely so
  it can execute them mechanically.
- Switch to **Opus 4.8 / Fable 5** when: a null test won't converge, Mode 1
  doesn't sound like Rubber-Rhino and the diff isn't obvious, a real-time
  safety issue appears, or an architectural decision arises that this file
  doesn't cover. In the escalation prompt, include: the failing test output,
  the exact diff of ported vs. original DSP file, and this file.
- Never let any model "fix" a null-test failure by editing the reference or
  loosening the threshold.

## Things Claude must NOT do in this repo

- Introduce JUCE dsp:: module replacements for ported Rubber-Rhino DSP
- Add oversampling, band-limiting, or higher-order interpolation anywhere
- Reformat ported files wholesale (keeps diffs against Rubber-Rhino auditable)
- Touch dsp/bank/ headers
- Add dependencies beyond JUCE and the C++17 standard library
