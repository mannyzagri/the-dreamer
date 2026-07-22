# TD-001 — "0 dBFS noise after ~20-30 s": full incident report

**Status:** root-caused, fixed, verified (branch `fix/td-001-noise`, merged
`0c52c12`, 2026-07-22). Ships in **2.5.2**.
**Affected:** 2.2.0 → 2.5.1 (since the FX v1.5 modfx expansion introduced
`Ensemble.h`; `Dimension.h`/`Rotary.h` carried the same latent defect).

---

## 1. Symptom (user report, via mac-opus triage)

Sustained full-scale ("0 dBFS") noise appearing after ~20-30 s of use OR
idle on 2.5.0/2.5.1 builds. Initial guess was reverb/delay overflow. The
noise persisted until restart; appearance seemed random across sessions.

## 2. What it was NOT (all measured, not assumed)

Exhaustive offline probes — every one clean over 60-120 s renders:

| Ruled out | How |
|---|---|
| Voice side (glfo2, detune, live-env, drift, granular D15, HIT stretch, multi-tap stacks) | 72-scenario wave-type matrix through DreamSynth, held + idle |
| All 14 global filter types incl. DreamPlane Z-plane | static + continuously modulated cutoff/res, 44.1/48/96 k, both oversample paths (incl. the vintage os=1 "ringy" path) |
| WaveNorm makeup gains (mac hypothesis) | table max = 3.37× (idx 184) — cannot raise a floor to full scale |
| juce::Reverb self-oscillation | structurally Freeverb: fb ≤ 0.98, undenormalised, freeze forced off |
| Delay/modfx/LoFi/Width/Talk at production params | 60 s scenario matrix, kitchen-sink chain |
| Host-realistic abuse of the real binary | 10+10 idle/held soaks 12→120 s, 48-program preset sweep, seeded param fuzz, ragged block sizes — all clean |

Conventional gates (validator dsp 10/10, pluginval strictness 8) were green
throughout — this bug class is invisible to them.

## 3. The hunt

1. A minimal JUCE VST3 host (`tools/hostprobe`, now generalized as
   `code-bank/tools/vst3-probe`) drove the REAL deployed 2.5.1 binary through
   long offline renders (~30-60× realtime).
2. A concurrent-hammer phase produced finite astronomic peaks (6×10²⁶) at
   44.1 k — first repro. Initially read as a threading race; later shown
   deterministic.
3. A compiled-out per-stage tracer (`DREAMER_TD001_TRACE`, 8 taps through
   `processBlock`) named the stage: **MOD FX out = 6.5e26 while its input was
   0.003** — and `final = 0.9886`, exactly the limiter ceiling.
4. The fault fired at the **same cumulative block (148467) in every faulting
   run** — inside the single-threaded preset sweep, program 13 **SOLINA
   FIELDS** (ENSEMBLE modfx), ~13.7 s into a 30 s idle render. Runs that
   didn't fault differed only by process launch → **ASLR-dependent content of
   a bad memory read**, not a race, not an instability.
5. All seven modfx algorithms audit as BIBO-stable with in-range params ⇒ the
   value had to come from memory, not math. A cpp-persona audit then produced
   the exact mechanism with a numeric proof, confirmed by micro-harness.

## 4. Root cause

`dsp/glue/Ensemble.h` fractional delay reader (same helper copied in
`Dimension.h`, `Rotary.h`):

```cpp
float pos = (float)w_ - d;            // d clamped to [1, len_-2]
if (pos < 0.0f) pos += (float)len_;   // <-- the bug
const int i0 = (int)pos;              // i0 can equal len_
return buf[i0] + (buf[i1]-buf[i0])*fr;   // buf[len_] = one-past-end heap read
```

When the modulated tap distance `d` lands **1-2 ulp above** the write head
`w_`, `w_ - d` is a tiny negative float (exact, by Sterbenz), and the wrap-add
rounds up: ulp(1327) = 2⁻¹³ ≈ 1.22e-4, so any result within a half-ulp of
1327.0 becomes **exactly `(float)len_`**. Then `i0 == len_` (one past a
1327-element `std::vector`, unchecked in release) and — decisively —
`fr == 0.0` exactly, so the 4 bytes of adjacent heap are returned **verbatim,
unattenuated**. A random 32-bit pattern read as float is astronomically large
about half the time → the measured ~1e26 bursts, and the ~50% per-launch
audibility (heap layout decides the bytes).

Trigger cadence: exactly two representable `d` values per integer `w_` fire.
At SOLINA FIELDS' settings (rate 0.3 → 1.94 Hz, depth 0.6 → taps sweep
8-12.6 ms; six `read()` calls/sample) the alignment occurs ~2× per 30 s at
44.1 k, at fully deterministic sample positions. Micro-harness replay of the
index math reproduced hits at t = 13.97 s and 17.37 s — matching the traced
fault at ~13.7 s.

### Why it presented as "sustained 0 dBFS noise after 20-30 s"

- The one-block burst enters the delay (written even when the delay is off)
  and **juce::Reverb's combs (fb ≤ 0.98), which shed only ~1%/block** — an
  e26 burst stays astronomically loud for **minutes**.
- The D12 output limiter (tanh, −0.1 dBFS ceiling) clips that to **exactly
  0.98855, forever** → a steady full-scale roar, magnitude hidden from meters.
- The D13 recovery never fired: everything stayed **finite**.
- Panic/flush couldn't fully clear it (see §5.2), so it survived "recovery".
- The 20-30 s = time-to-trigger (LFO/write-head alignment), not a growth time.

## 5. Secondary defects found and fixed in the same pass

1. **D13 was blind to pure NaN.** `peakL = std::fmax(peakL, fabs(l))` — C++
   `fmax` returns the *non-NaN operand*, so NaN was silently absorbed and
   `isfinite(peak)` stayed true; D13 only ever fired on ±Inf. Fixed with a
   per-sample NaN latch (`l != l`). Finite overloads deliberately do NOT
   trigger this path (project directive: **NaN is NaN** — separate failure
   classes).
2. **Incomplete resets.** `flushFx()` (D13/panic) reset synth, delay, reverb,
   modfx, dcblock — but **not** the global filters `f1/f2`, nor
   LoFi/StereoWidth/Talkbox, nor the scope ring. `prepareToPlay` set sample
   rates on `f1/f2` but never reset their state (survived host stop/start and
   SR changes). `StereoWidth::prepare` zeroed 2 of its 4 one-pole states.
   All completed.
3. **Same hazard class in ported code (NOT fixed — rule 1):**
   `dsp/ported/fx/ModFx.h readDelayed` (`while (rp<0) rp += size` then
   unchecked `buf[i0]`) and the `Effects.h` tape wow/flutter path (worse:
   larger buffer → wider ulp window). On record; a glue-side guard needs a
   design decision if those paths' modulation ranges are ever widened.

## 6. The fix

One line in each of `Ensemble.h`, `Dimension.h`, `Rotary.h`:

```cpp
if (pos >= (float)len_) pos = 0.0f;   // re-normalize: len_ ≡ 0 (mod len_)
```

`pos == len_` is congruent to index 0 and the interpolation's continuous
limit there is `buf[0]` with weight 1 — so `0.0f` is **bit-correct**, not
just safe. Zero sonic change to any in-range read.

## 7. Verification

| Gate | Result |
|---|---|
| `test_fx_read_bounds` (new, in validator.json) | pre-fix math: **3572** one-past-end indices across every `w_` × ±1-4 ulp × all three geometries; post-fix: **0**, renormalized cases land on `buf[0]` with `fr=0`; real classes finite/bounded over the faulting trajectory |
| Validator dsp matrix | **11/11 PASS**, all pre-existing harnesses bit-stable |
| End-to-end (vst3-probe full suite ×2 vs fixed traced build) | **0 faults, 0 tracer trips** (was: deterministic fault @ block 148467) |
| Pending | 2.5.2 release build + pluginval 8 + deploy; user ear-gate: SOLINA FIELDS @44.1 k, play + idle 60 s |

## 8. Tooling produced (kept)

- **`code-bank/tools/vst3-probe`** — generic six-phase long-render soak
  harness for any VST3 (idle/held soaks, factory-preset sweep, fuzz, ragged
  blocks, concurrent hammer; optional ASan build). This class of bug needs
  *minutes of rendering through real presets* — exactly the coverage gap.
- **`tools/hostprobe`** — project-local copy, the TD-001 repro archive.
- **`DREAMER_TD001_TRACE`** — compiled-out stage tracer in `processBlock`;
  strip after 2.5.2 soaks green.

## 9. Lessons (transferable)

1. **A limiter converts "astronomic garbage" into "legal-looking full-scale
   output"** — never soak-test only post-limiter; tap pre-limiter stages.
2. **`std::fmax` swallows NaN** — never build NaN detection on fmax/fmin.
3. **Fractional delay wrap-adds must re-normalize after the add**: with
   float indices, `x + len` can equal `len` exactly. Audit every
   `if (pos < 0) pos += len` in interpolated readers.
4. **Deterministic fault time + per-launch appearance = memory-content bug**
   (ASLR decides the bytes). Treat "sometimes it happens" as a heap smell.
5. **MSVC ASan silence is not innocence** (allocator-slack reads are
   invisible); exhaustive index-math proofs and content-based detection
   (peak/NaN watch) remain necessary.
6. **Recovery paths are part of the bug surface**: incomplete flush/prepare
   resets turned a one-block glitch into a permanent outage.
