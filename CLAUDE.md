# CLAUDE.md — "Rompler" project: port & integration brief

Project: dual-mode 90s ROMpler VST3 (JUCE, CMake, C++17, Windows 11, Cubase 15
as the validation DAW). Built from two sources:

1. **Rubber-Rhino codebase** (existing, working plugin) — donor for filters,
   effects, envelopes/mod routing, parameter plumbing, editor scaffolding.
2. **rompler-bank v2** (this repo, `dsp/bank/`) — RomplerBankData.h,
   RomplerBank.h, PcmOscillator.h, Mode2Voice.h. These are verified; treat as
   read-only unless a task explicitly says otherwise.

Read this file fully before any task. When in doubt: ask, don't improvise.

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

## Class-name mapping (fill in real Rubber-Rhino names before phase 1)

| Rubber-Rhino source              | Ported name / location            |
|----------------------------------|-----------------------------------|
| <FilterClass>       (TODO name)  | dsp/ported/RhinoFilter.h/.cpp     |
| <FilterTypeEnum>                 | keep values & order EXACTLY       |
| <EnvelopeClass>                  | dsp/ported/RhinoEnvelope.*        |
| <LfoClass>                       | dsp/ported/RhinoLfo.*             |
| <FxChain: chorus/delay/etc>      | dsp/ported/fx/*                   |
| <VoiceClass>                     | reference only -- NOT ported      |
| APVTS layout helpers             | plugin/Params.*                   |

---

## Phase plan

### Phase 1 — Filter port + FilterSlot adapter   [Sonnet 4.6]
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
