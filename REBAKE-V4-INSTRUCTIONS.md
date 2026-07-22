# Loop-library re-bake (v3 → v4): kill the mid-loop pitch drift

Instructions for the Claude session performing the bake. Written 2026-07-22
after root-causing the user's "loops offtune in the middle section" report.

## The defect (measured, not speculative)

`bake_final.py` (shipped inside dreamer-library-v3.zip) applies a "shared
tiny drift" to every loop:

```python
sd_amt=recipe.get('shared_drift',2.0e-5)
sd=np.cumsum(rng.normal(0,sd_amt,N)); sd-=np.linspace(0,sd[-1],N)  # shared, tiny
...
f=f0*2**(sd)
```

The endpoint correction turns the walk into a Brownian bridge: pinned to 0 at
both ends of the loop (clean seam) → the pitch deviation **peaks mid-loop**.
With `sd_amt=2e-5` over ~247k oversampled samples the drift is NOT tiny.
Measured on the delivered v3 WAVs (autocorrelation, 0.3 s windows):

| loop     | 0%    | 25%    | 45%   | 65%   | 85%   |
|----------|-------|--------|-------|-------|-------|
| PAD_02   | +1.3c | +4.2   | +6.6  | +8.3  | +5.3  |
| AIRY_01  | +3.5c | +10.7  | +9.3  | +5.2  | +3.2  |
| VOX_01   | +2.2c | −4.9   | −4.4  | +2.7  | +4.6  |

±8–10 cents on 220 Hz pads = audible detune, and it fights the engine's own
DRIFT/detune params (the script's own header says "width is the engine's job
now"). User decision: remove it.

## The change — ONE line

In `bake_final.py`:

```python
sd_amt=recipe.get('shared_drift',2.0e-5)
```
→
```python
sd_amt=recipe.get('shared_drift',0.0)      # v4: drift removed (engine owns "life")
```

Nothing else. Do NOT touch: recipes, families, counts, morph specials,
`random.seed(7)`, durations, air amounts, the loop crossfade, the 12-bit
quantize (`np.round(loop*2047)` → int16 scaled), names, or output order.

## Determinism note

`rng=np.random.default_rng(hash(name)&0xffff)` — Python string `hash()` is
salted per process. Run with `PYTHONHASHSEED=0` so the bake is reproducible.
(With drift=0 the rng only shapes the 'air' noise layer, so this only matters
for byte-reproducibility, not pitch.)

## Verify BEFORE delivering (all four, no skipping)

1. **Count/names**: exactly 130 loop WAVs, names identical to v3
   (PAD_01..28, AIRY_01..18, VOX_01..16, ETHER_01..16, FM_01..14,
   WIND_01..12, METAL_01..12, 14 MORPH_* specials). The 10 HIT_* one-shots
   are NOT part of this bake — do not regenerate or include altered HITs.
2. **Pitch-flatness** (the acceptance test for the defect): for at least
   PAD_01, PAD_02, AIRY_01, VOX_01, FM_01, ETHER_01 — autocorrelation pitch
   at 0/25/45/65/85% positions, 0.3 s windows. Criterion: **max deviation
   from the file's own mean < ±2 cents** for tonal families (METAL_* and
   metal-MORPHs are inharmonic — exempt).
3. **Format**: 44100 Hz, mono, 16-bit PCM, every sample value on the 12-bit
   grid (int16 = (q/2048)*32767 with q ∈ [−2048, 2047]).
4. **Seam**: loop start/end continuity preserved (the 0.3 s crossfade region
   is unchanged code — just confirm no clicks on a wrap-around listen/plot).

## Deliverables

`dreamer-library-v4.zip` containing: the 130 WAVs, the modified
`bake_final.py`, `bake_loops.py`, `fam_sources.json`, `akwf_feats.json`, and
a `loop_roots.json` with **all 130 roots = 220.0** (the v3 nominal is
unchanged; the 2.5.1 all-220 decision stands — the drift was the real
problem, not the roots).

## Integration steps (the-dreamer VM session, afterwards)

1. Unzip the 130 WAVs over `assets/loops/` (names identical → overwrite).
2. Re-bake `dsp/bank/LoopBankData.h` via the house C++ tool
   (`tools/bake_loops_header.cpp`) — NO python on the VM, by design.
   `LoopRoots.h` needs no change (already all-220 since 2.5.1).
   `ShotBankData.h` untouched.
3. Run the bank tests (12-bit invariant, 218 counts) + full validator +
   pluginval 8. No param change → reload instance, no re-scan.
4. Update CHANGELOG + STATE; branch → merge --no-ff; `git add` explicit
   file lists only. assets/loops WAVs and LoopBankData.h are **Git LFS** —
   verify `git lfs ls-files` shows them tracked after commit.

## What NOT to do (history has receipts)

- Do NOT re-introduce per-loop measured roots to compensate drift — that was
  the v2 band-aid; with drift=0 the loops are truly 220-flat.
- Do NOT renormalize/re-dither or "clean up" the 12-bit grain.
- Do NOT rename, reorder, or change the count — indices are preset refs and
  the wave-choice param; they are LOCKED.
- Do NOT keep "a whisper" of drift without the user asking: the engine's
  global DRIFT param (±3 cents, per-voice) is the sanctioned life-source.
