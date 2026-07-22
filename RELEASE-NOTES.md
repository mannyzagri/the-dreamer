# The Dreamer — Release Notes

User-facing notes per release (what changed for you, what to do). The full
engineering history lives in CHANGELOG.md; deep-dive incident reports live in
validation/.

---

## 2.5.2 — "the 0 dBFS noise" fix (built 2026-07-22; deploy pending)

### Fixed
- **Sustained full-scale noise after ~20-30 s of playing or idling**
  (TD-001). A memory-read defect in the ENSEMBLE mod-fx delay reader could
  return a garbage value once per ~15 s of rendering; the reverb then held
  that burst for minutes and the output limiter clipped it to a constant
  0 dBFS roar. Whether you heard it on a given plugin load was literally
  memory-layout luck — which is why it felt random and survived restarts
  inconsistently. Most audible on **SOLINA FIELDS** and other ENSEMBLE-modfx
  presets at 44.1 kHz. The same latent defect in the DIMENSION and ROTARY
  mod-fx modes is fixed too.
  → Full incident report: **validation/TD-001-REPORT.md**.
- **SOUND OFF (panic) and the NaN-recovery now clear everything.** Previously
  the two global filters and the LO-FI/WIDTH/TALK stages kept their internal
  state through a panic and even through host stop/start — a corrupted filter
  could keep ringing after "recovery". Every stateful stage is now reset.
- **NaN detection actually works now.** The engine's NaN-recovery only ever
  triggered on infinities (a C++ `fmax` subtlety silently swallowed NaN);
  a true NaN in the chain is now caught and flushed. Finite loud signals are
  deliberately NOT treated as faults — no new limiting, no sound change.

### Sound
- **No sonic change to any legitimate signal path.** All 11 DSP validator
  harnesses pass bit-stable; the fixes only affect values that were never
  valid audio.

### Upgrading
- No parameter change vs 2.5.0/2.5.1 → **reload the instance** (or restart
  Cubase); no re-scan needed. Coming from ≤2.4.x still needs a FULL re-scan.
- Suggested verification after install: load **SOLINA FIELDS** at 44.1 kHz,
  play a chord, then let the instrument sit for 60 s — output stays clean.

---

## 2.5.1 — loop tuning fix (2026-07-22, deployed)
- All 130 ENS loops were playing ~13 cents sharp (stale v2 tuning table over
  the v3 library). Fixed; loops now play in tune. Reload, no re-scan.
- Known issue at this version: TD-001 (above) — fixed in 2.5.2.

## 2.5.0 — v16 GUI + Global LFO 2 (2026-07-21, deployed)
- New production face (preset load wired, live data from the engine), second
  global LFO, mod-matrix sources 5→7. **Param-list change → FULL re-scan.**
