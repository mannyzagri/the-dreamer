# GUI_PROD_MISMATCH.md — production app.js ↔ shipped DSP (v2.3.0)

> ## UPDATE — full binding cross-check of production/app.js (GLOBAL_ID/TONE_ID + KIND)
> DSP side of A/B/C is DONE + validated (dsp 10/10, pluginval 8) on branch
> `port/gui-dsp-align`: **flt2_env, lfo_sync, modfx/dly/rev/lofi/talk `_param` +
> lofi/talk `pfocus` (focus-shadow engine), detune_voices → Choice**. `width_width`
> fixed to `width` in app.js. BUT app.js still has binding issues GUI-Claude must
> fix before a faithful build — grouped below. Until then the new GUI is NOT
> shipped (would regress: P-ENV + mod-matrix would not drive the DSP).
>
> ### G1. Relay-KIND mismatches (app.js kind ≠ shipped param class)
> - `flt_route` (KIND `route:'toggle'`) — DSP is **Choice** {Ser,Par}. Bind as
>   choice, OR ask DSP to make it Bool. (2-state; either works.)
> - `fx_prepost` (KIND `prePost:'toggle'`) — DSP is **Choice** {Post,Pre}. Same.
> - `detune_voices` — RESOLVED (DSP is now Choice {1..4}); keep `detVoices:'choice'`.
>
> ### G2. Shipped DSP params the GUI leaves EDITOR-ONLY (no handle → never written)
> These have NO entry in GLOBAL_ID, so the modal/rows only mutate `UI.*` and never
> reach the APVTS — the control is a dead mock. Add handles + bind:
> - **P-ENV**: `vec_penv_start`, `vec_penv_end`, `vec_penv_time`, `vec_penv_loop`
>   (the P-ENV EDIT modal is UI-only — `UI.penv`).
> - **MOD MATRIX**: `mtx1_src/dst/amt`, `mtx2_…`, `mtx3_…` (rows are UI-only —
>   `UI.matrix`). The whole matrix currently does nothing to the sound.
> - `vec_orbit_voice` (orbit per-voice) and `vec_penv_loop` — unbound toggles.
> - `lofi_alias` — folded into lofi focus, but as a **bool** it can't be a focus
>   target cleanly (the DSP now treats lofi focus idx 3 = alias via `lofi_param`>0.5,
>   so this works IF the GUI drives `lofi_param`+`lofi_pfocus`; the standalone
>   `lofi_alias` toggle is otherwise unreachable).
> - `interp`, `engine` — host-only carryovers, fine to leave unbound.
>
> ### G3. Confirmed wired-and-correct
> All ~300 other ids + kinds match; native fns present. Once G1/G2 are fixed I
> extend the C++ relay lists to match, register the native fns (getScopeData,
> getLimiterGR, panic, user-preset set), fix `loadPreset`→`applyPreset` (D4
> program-0-is-INIT off-by-one), bundle fonts locally, and build/deploy.
>
> --- (original param-existence report below; A/B resolved by the DSP additions) ---


The v15 `production/app.js` binds parameter ids/kinds that DO NOT match the
shipped APVTS (Params.h). Building as-is → those relays resolve to a null
parameter (dead controls, or a crash on attachment). Each must be resolved on
ONE side before the GUI can build. Grouped by fix owner.

## A. Params the design needs that the DSP does NOT have  (new DSP features)
| app.js id | design use | DSP today | resolve |
|---|---|---|---|
| `flt2_env` | Filter 2 ENVELOPE knob (v14: "F2 now CUT/RES/ENV like F1") | F2 has cut/res/**morph**, no env | ADD `flt2_env` + wire F2 env modulation, OR GUI drops the F2 ENV knob |
| `lfo_sync` | GLOBAL LFO **SYNC** button (v14) | global LFO = `lfo_rate`,`lfo_shape` only | ADD `lfo_sync` (+ tempo-div rate), OR GUI drops global-LFO SYNC |

## B. FX PARAMS architecture mismatch  (design model ≠ shipped model)
The design uses ONE **PARAMS knob per FX slot** (`modfx_param`/`dly_param`/
`rev_param`/`lofi_param`/`talk_param`) whose target is chosen by a **focus**
selector, and adds `lofi_pfocus`/`talk_pfocus`.
The DSP ships the FEATURES.md model: **four raw extras** `modfx_p0..p3`,
`dly_p0..p3`, `rev_p0..p3` (host-automatable), and NAMED lofi/talk params
(`lofi_bits/srate/compand/alias`, `talk_va/vb/morph/sens`); lofi/talk have NO
pfocus. `dly_p*`/`rev_p*` are RESERVED/inert (ported FX).
Missing ids app.js expects: `modfx_param, dly_param, rev_param, lofi_param,
talk_param, lofi_pfocus, talk_pfocus`.
**Resolve (pick one):**
- DSP adds a single `<slot>_param` that proxies the focus-selected `p_i` (and a
  lofi/talk focus+param model). Non-trivial; `dly/rev` extras are inert so their
  PARAMS knob would be inert too.
- GUI binds the shipped ids instead: the focused `<slot>_p<0..3>` for modfx (and
  shows dly/rev PARAMS as inert), and the named lofi/talk knobs.

## C. Simple id / type fixes
| app.js | issue | resolve |
|---|---|---|
| `width_width` (GLOBAL_ID `widthAmt`) | DSP id is **`width`** | GUI: `widthAmt:'width'` (one-char fix) |
| `detune_voices` bound kind = **'choice'** (TONE_KIND `detVoices`) | DSP is `AudioParameterInt` 1..4 | EITHER DSP → `AudioParameterChoice {"1".."4"}` (engine reads index+1), OR GUI: `detVoices:'slider'` |

## D. Confirmed OK (no action)
All other ~300 ids match, incl. the whole UX round: semi, detune_amount, loop_rate
+ sync/beats/varispeed, g_env_a/d/s/r, g_cutoff/g_res, g_octave, limiter_on,
tvf_kf(bipolar), matrix DST 10 "LOOP RATE". Native fns all present in the
processor: getScopeData, getLimiterGR, panic, getPresetList, getUserPresetList,
save/rename/delete/loadUserPreset, getWaveList.

## E. Also to wire on integration (not mismatches, but required)
- **Preset load off-by-one (D4):** the GUI is 0-based over the factory bank, but
  the `loadPreset` native fn calls `setCurrentProgram`, and host program 0 is now
  INIT. The native fn must call `applyPreset(index)` (0-based factory), not
  `setCurrentProgram`. (Fix in PluginEditor.cpp.)
- **New native fns to register** in PluginEditor.cpp: getScopeData, getLimiterGR,
  panic, getUserPresetList, saveUserPreset, renameUserPreset, deleteUserPreset,
  loadUserPreset.
- **Relay lists** (makeSliderIds/Toggle/Combo) must gain the UX-round ids.
- **editor.html**: bundle fonts locally (`@font-face` in style.css → the shipped
  TTFs) — Google Fonts is CSP-blocked in the WebView; and set `window.Juce` from
  `index.js` before importing app.js (keep check_native_interop.js first).
