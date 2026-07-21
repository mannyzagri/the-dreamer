# GUI_DSP_DIFF.md — new GUI handoff vs shipped DSP (v2.3.0)

For GUI-Claude. Compares the `design_handoff_dreamer_gui` contract against the
ACTUAL deployed backend (plugin/Params.h + PluginProcessor API, v2.3.0). The DSP
is the source of truth — bind only to IDs that exist here. See
UX_ROUND_HANDOFF.md for the full new-param contract.

> ## ✅ RESOLVED in handoff v15 ("Design markdown file (1).zip")
> v15 explicitly closes these gaps — verified present: DETUNE (D9), GLOBAL OCTAVE
> (D8), OFFSET MODE g_env/g_cutoff/g_res (D5), PLAY MODE all-types + LOOP RATE
> cluster (D15), SPECTRUM via getScopeData (D3), two-bank preset browser +
> save/rename/delete/load (D14), 218 waves via getWaveList, matrix DST 10 "LOOP
> RATE", tvf_kf bipolar (D10), SEMI (D7). The §1–§6 gap list below is now
> HISTORICAL (kept for provenance). **Remaining to verify:** (a) the README
> "replace fully" vs GUI_SPEC "renovate from v5" conflict is STILL unresolved —
> pick one; (b) the UTILITY LIMITER toggle must bind param id `limiter_on`.
>
> --- (historical gap list, closed by v15) ---

Checked originally against handoff **v14** ("Design markdown file2.zip", adds
support.js + @4x). v14 over v13 added: **SEMI knob** (`semi_[t]`), removed the
redundant row-2 LEVEL knob, LFO-shape `< >` steppers, FX TYPE/FOCUS header polish.

## 0. Doc conflict to resolve first
- `README.md` says "DELETE/REPLACE the editor fully, do not merge, wipe build".
- `GUI_SPEC.md` says "renovate FROM The_Dreamer_Vector_GUI_v5.html, do not start over".
These contradict. Pick one intent before regenerating. Either way, **do not
regress the live data below**.

## 1. DSP params the design has NO control for  → ADD to the panel
(All exist in the APVTS today, host-automatable, but unsurfaced in the design.)

| ID(s) | Task | What / suggested home |
|---|---|---|
| ~~`semi_[t]`~~ | D7 | ✅ **covered in v14** (SEMI knob between OCTAVE/FINE) |
| `detune_voices_[t]` (Int 1..4), `detune_amount_[t]` (0..1→±25c) | D9 | DETUNE VOICES stepper + DETUNE knob (dim knob when voices=1) |
| `g_env_a/d/s/r`, `g_cutoff`, `g_res` (bipolar −1..+1) | D5 | GLOBAL OFFSET mode (G4): a latching GLOBAL button re-targets the tone ADSR sliders + filter CUT/RES to these |
| `g_octave` (Int −2..+2) | D8 | GLOBAL OCTAVE stepper (header/master area) |
| `loop_rate_[t]` (0..1→0.25..4×), `loop_rate_sync_[t]` (bool), `loop_rate_beats_[t]` (12 divs), `loop_varispeed_[t]` (bool) | D15 | on a LOOP wave in STRETCH: LOOP RATE knob + SYNC + beats + VARISPEED toggle (see §3 play-mode) |

## 2. New native functions the design does NOT call  → WIRE
| Function | Task | Use |
|---|---|---|
| `getScopeData(n=2048)` → Array<float> | D3 | final-output tap; GUI computes the 2048-pt FFT (~20 fps). Design still shows only L/R meters — the SPECTRUM page is new. |
| `getUserPresetList()`, `saveUserPreset(name)`, `renameUserPreset(old,new)`, `deleteUserPreset(name)`, `loadUserPreset(name)` | D14 | USER preset bank (design has factory browser only). Factory is read-only. |
| `resetToInit()` | D4 | INIT button (host program 0 is already INIT). |

Native fns the design DOES use and already exist (verify wiring): `panic()`
(SOUND OFF), `getLimiterGR()` (LIM LED), `midiLearnStart/Cancel/ClearParam/
CcForParam/IsArmed()` (MIDI learn), `getWaveList()`, `getPresetList()`.

## 3. Changed semantics/ranges  → UPDATE existing controls
| Area | Design assumes | DSP now (v2.2/2.3) |
|---|---|---|
| `tvf_kf_[t]` | v14: bipolar −63…+63 ✓ | **bipolar −1..+1** (display −100..+100, detent 0). Confirm the knob WRITES bipolar (0.5-normalized = 0/center). |
| env A/D/R (`tvf/tva/aux_a/d/r`, `vec_penv_time`) | v14: real units ms/s ✓ | param returns the **formatted text** ("340 ms"/"1.2 s"). GUI must DISPLAY the param's text value, not compute 0–127. Law is log 1 ms–10 s. |
| Mod-matrix DST | 9 dests, ends "FX PARAM" | **10 dests** — added **"Loop Rate"** (index 9). Add it to the DST menu. "FX PARAM" stays reserved/inert. |
| PLAY MODE (`hit_play_[t]`) | HIT-only NORMAL/STRETCH; separate LOOP MODE for loops | **applies to ALL wave types** (display "Play Mode"). CYCLE+STRETCH = inert; LOOP+STRETCH exposes the D15 LOOP RATE controls; HIT+STRETCH = varispeed (STRETCH/P.TRIM as before). `loop_mode_[t]` (FWD/PINGPONG) still exists for loops separately. |
| Presets | 24 factory, UI-side list | **47 factory + USER bank**, processor-owned; `getPresetList()` tags each `bank:"FACTORY"`, `getUserPresetList()` tags `"USER"`. **Host program 0 = INIT**, factory = programs 1..N (GUI applyPreset stays 0-based factory). |
| Wave ROM | "78 waves" (PAD/STR/VOX/BELL/MTL/BAS) | **218 waves** = 78 cycle + 130 loop("ENS") + 10 hit("SHOT"). Pull live via `getWaveList()` (already the 2.1.1 fix) — do NOT hardcode 78. Menu tags ""/ENS/SHOT. |

## 4. Design controls that map cleanly (no DSP change; just verify IDs)
**SEMI** `semi_[t]` (D7, new in v14 — Int −12..+12, reads "-12 st…+12 st"),
MIDI learn (D6), KEY FLW bipolar (D10), LIM + UTILITY LIMITER `limiter_on` (D12),
SOUND OFF panic (D13), real-unit envelopes (D1), per-LFO SHAPE (`lfoN_shape_[t]`,
TRI/SIN/SAW/SQR/S+H), FX PARAMS focus (`*_pfocus`/`*_p0..p3`), delay SYNC
(`dly_sync`), VOICING/DREAMY-spread, LOOP MODE. All present in Params.h.

## 5. Reserved/inert (render for parity, expect no audio effect)
- Matrix **FX PARAM** dst — reserved/inert (no focus target wired yet).
- `dly_p0..p3`, `rev_p0..p3` PARAMS extras — reserved/inert (ported FX can't take
  extra knobs). `modfx_p0..p3` ARE live.

## 6. Authoritative per-tone ID list (suffix _a/_b/_c/_d)
wave, on, level, oct, **semi**, fine, start, start_random, velo, pan, shape,
shape_depth, noise, noise_color, dir, vint, voicing, dreamy_spread,
**detune_voices, detune_amount**, loop_mode, hit_play, hit_stretch,
hit_pitchtrim, **loop_rate, loop_rate_sync, loop_rate_beats, loop_varispeed**,
aux_dest, aux_amt, tvf_type, tvf_cut, tvf_res, tvf_env, **tvf_kf(bipolar)**,
tvf_a/d/s/r, tva_a/d/s/r, aux_a/d/s/r, lfo1_rate/depth/sync/dest/shape,
lfo2_rate/depth/sync/dest/shape. (**bold** = new/changed vs the design.)

Global new/changed: **g_env_a/d/s/r, g_cutoff, g_res, g_octave, limiter_on**;
matrix dst gains **Loop Rate**.
