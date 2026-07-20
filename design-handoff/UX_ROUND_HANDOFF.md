# UX_ROUND_HANDOFF.md — DSP→GUI contract for the UX polish round (v2.2.0)

DSP/VM side of UX_DSP_TASKS D1–D14 + D11 is implemented, validated (dsp 10/10,
pluginval 8), and deployed as **v2.2.0**. This file is the hand-off for
GUI-Claude (UX_GUI_TASKS G1–G13). **Param-list changed → Cubase FULL RE-SCAN.**

## New automatable parameters (bind by these exact IDs)

| ID | Type / range | Default | Task | Notes |
|---|---|---|---|---|
| `semi_[a..d]` | Int −12..+12 | 0 | D7 | per-tone semitone; sits by OCTV/FINE |
| `detune_voices_[a..d]` | Int 1..4 | 1 | D9 | 1 = off (dim the DETUNE knob) |
| `detune_amount_[a..d]` | Float 0..1 | 0 | D9 | → 0..±25 c; near OCTV/SEMI/FINE |
| `g_env_a` `g_env_d` `g_env_s` `g_env_r` | Float −1..+1 | 0 | D5 | GLOBAL offsets to every tone's **TVA** env |
| `g_cutoff` `g_res` | Float −1..+1 | 0 | D5 | GLOBAL offsets to every tone's **TVF** |
| `g_octave` | Int −2..+2 | 0 | D8 | one global octave, shifts all tones |
| `limiter_on` | Bool | true | D12 | output soft-clip+ceiling enable |

**Re-ranged:** `tvf_kf_[a..d]` is now **bipolar −1..+1** (was 0..1). Display
**−100..+100**, centre detent 0; the param already formats its text value that
way. Negative = darker up the keyboard. (Factory presets migrated to preserve
key-follow; user projects on old state load fine.)

**Env-time display (D1):** `tvf_a/d/r`, `tva_a/d/r`, `aux_a/d/r`, `vec_penv_time`
now return a real-unit **text value** ("340 ms" / "1.2 s"). GUI G1: show the
parameter's formatted text, don't compute 0–127.

## New processor functions to wire (native bridge; GUI is the viewer)

- **D3 analyzer:** `getScopeData(n=2048)` → `Array<var>` of the last n final-output
  samples (post-master/limiter). GUI computes the 2048-pt FFT at ~20 fps (G2).
- **D12 limiter LED:** `getLimiterGR()` → gain reduction in dB (0 = not limiting)
  for the activity LED near MASTER (G13).
- **D13 panic:** `panic()` — all-notes-off + FX flush (SOUND OFF button, G11).
- **D4 INIT:** host **program 0 = INIT**; `resetToInit()` for a GUI button.
  Factory presets are host programs 1..N. The GUI's `getPresetList()`/`applyPreset()`
  stay 0-based over the factory bank (unchanged). To show INIT in the browser,
  prepend it and call `resetToInit()` for that row.
- **D6 MIDI learn** (right-click menu, G8): `midiLearnStart(id)` / `midiLearnCancel()`
  / `midiLearnClearParam(id)` / `midiLearnCcForParam(id)` (−1 = none) /
  `midiLearnIsArmed()`. While armed, the control should pulse; the next incoming
  CC binds. Map persists in plugin state.
- **D14 user presets** (SAVE/RENAME, G12): `saveUserPreset(name)` /
  `renameUserPreset(old,new)` / `deleteUserPreset(name)` / `loadUserPreset(name)` /
  `getUserPresetList()` → `Array<{name,category,bank:"USER"}>`. `getPresetList()`
  now tags factory rows `bank:"FACTORY"`. Factory bank is read-only (SAVE on a
  factory patch = save-as-user-copy).

## Flags / deviations (recorded, not silent)
- **D5** global offsets target **TVA** env + **TVF** cutoff/res only (a TVF-env
  target switch is a future add if wanted).
- **D11** loudness is a **playback gain** (bank bytes untouched, rule 3); overall
  level dropped ~10 dB to −14 dBFS RMS reference. Target is a one-liner in
  `tools/bake_wave_norm.cpp` if the ear pass wants it hotter.
- **D6** native Steinberg **IMidiMapping** (host-visible CC assignments) is left
  to JUCE's VST3 wrapper; the internal learn routes CCs host-agnostically.
- **D16** `start_random_[t]` already exposes the automation name **"Start Random"**
  (not "RND"); GUI just relabels the button.
- **D15/D16 shipped in v2.3.0** (see below).

## v2.3.0 additions (D15 PLAY MODE all-types + LOOP RATE; D16 naming)

Param-list changed again → **RE-SCAN**. New per-tone params (bind by ID):

| ID | Type / range | Default | Notes |
|---|---|---|---|
| `hit_play_[t]` | Choice Normal/Stretch | Normal | **now "Play Mode", all wave types** (was hit-only) |
| `loop_rate_[t]` | Float 0..1 | 0.5 | LOOP+STRETCH morph speed → 0.25×..4× (log, 0.5=1×); mod-matrix dest "Loop Rate" |
| `loop_rate_sync_[t]` | Bool | false | lock traversal to host tempo |
| `loop_rate_beats_[t]` | Choice (12 divs) | 1/4 | beat-division when synced (4/1…1/32) |
| `loop_varispeed_[t]` | Bool | false | ON = plain pitch-follows varispeed instead of the decoupled granular sweep |

GUI (UX_GUI): expose PLAY MODE on all tones; on a LOOP wave + STRETCH show a
LOOP RATE knob (+ SYNC toggle / beats stepper) and a VARISPEED toggle. On a HIT
wave STRETCH keeps HIT STRETCH/PITCH TRIM; on a CYCLE wave STRETCH is inert
(dim/hide). `loop_rate` also appears as mod-matrix destination "Loop Rate".

- **D16** `start_random_[t]` automation name is already "Start Random" — GUI just
  relabels the RND button; no new param.
