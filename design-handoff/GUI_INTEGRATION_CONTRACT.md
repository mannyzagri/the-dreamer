# GUI_INTEGRATION_CONTRACT.md — how to hand off a face that isn't disconnected

For **GUI-Claude**. During the v15 integration (shipped as The Dreamer 2.4.x) the
production `app.js` shipped with **disconnected controls** — bindings that didn't
match the shipped DSP, hardcoded data, and unwired actions. This doc lists every
fix that had to be applied at integration, the authoritative binding contract,
and a pre-handoff checklist so it doesn't recur. **The DSP (plugin/Params.h +
PluginProcessor) is the source of truth. Bind to it, don't assume.**

> These fixes currently live in the plugin's copy of `app.js`/`editor.html`.
> Your NEXT handoff will OVERWRITE them unless you fold them in upstream. Please
> incorporate all of §1–§5 into the production files you deliver.

---

## 1. Actions were never wired (the "preset won't load" bug)

Every processor action must call the native function via `Bridge.fn(name)(...)`.
The v15 face wired panic/save/rename/delete but **left LOAD unwired** — selection
only mutated `UI.preset` and the "LOAD SELECTED" button just closed the overlay.

Wire ALL of these (they exist as `withNativeFunction`s in PluginEditor.cpp):

| Native fn | Call it from |
|---|---|
| `loadPreset(index)` | header ▲/▼ steppers, factory browser rows, LOAD SELECTED (factory) — index is **0-based over the FACTORY bank** |
| `loadUserPreset(name)` | user browser rows, LOAD SELECTED (user) |
| `saveUserPreset(name)` / `renameUserPreset(old,new)` / `deleteUserPreset(name)` | SAVE / RENAME / DELETE |
| `panic()` | SOUND OFF |
| `getScopeData(2048)` | spectrum tick (FFT) |
| `getLimiterGR()` | LIM LED poll |

Do NOT call `setCurrentProgram` from the GUI: host program 0 is the synthetic
INIT patch (D4), so it is offset by one from the factory list. Use `loadPreset`.

## 2. Processor-owned data must be FETCHED, not hardcoded

The face hardcoded `WAVES` and `PRESETS` (the source even said *"PRODUCTION:
replace with getWaveList() … do NOT hardcode"*). The hardcoded PRESETS order
**diverged** from `presets.json`, so an index load recalled the WRONG patch.

On boot, replace the local arrays from the processor (mock returns them unchanged
so browser QA still works):

```js
Bridge.fn('getPresetList')().then(l => { if (l?.length) { PRESETS.length=0;
  l.forEach(p => PRESETS.push([p.category, p.name])); refreshHeader(); } });
Bridge.fn('getUserPresetList')().then(l => { if (Array.isArray(l)) { USER_PRESETS.length=0;
  l.forEach(p => USER_PRESETS.push({name:p.name, category:p.category||'USER', bank:'USER'})); } });
Bridge.fn('getWaveList')().then(l => { if (l?.length) { WAVES.length=0;
  l.forEach(w => WAVES.push([w.category, w.name, w.bank||''])); } });
```

Wave ROM = **218** (78 cycle + 130 loop["ENS"] + 10 hit["SHOT"]); factory presets
= **47** (bank tag FACTORY). Never hardcode these counts/names/order.

## 3. Relay KIND must match the shipped param CLASS

A control bound with the wrong relay kind is dead (or crashes the editor). The
KIND you pick MUST match the param's class in Params.h:

- `AudioParameterFloat` / `AudioParameterInt` → **slider** (`getSliderState`)
- `AudioParameterChoice` → **choice** (`getComboBoxState`)
- `AudioParameterBool` → **toggle** (`getToggleState`)

Mismatches found and fixed at integration — set these upstream:

| Key (app.js) | id | Shipped class | Correct KIND |
|---|---|---|---|
| `orbitVoice` | `vec_orbit_voice` | Bool | **toggle** (was 'choice') |
| `widthAmt` | **`width`** | Float | slider — the id was typo'd `width_width`; the DSP id is `width` |
| `detVoices` | `detune_voices` | Choice{1,2,3,4} | choice ✓ (DSP was changed Int→Choice to match) |
| `route` | `flt_route` | Bool | toggle ✓ (DSP changed Choice→Bool) |
| `prePost` | `fx_prepost` | Bool | toggle ✓ (DSP changed Choice→Bool) |

Bipolar params (pan, tvf_kf, aux_amt, flt_bal, mtx*_amt, g_env_*/g_cutoff/g_res)
travel the relay **normalised 0..1 with 0.5 = centre**; format ±63/±100 for
display only.

## 4. FX PARAMS model is NOT uniform — match each slot

The DSP wires two different PARAMS models; bind each slot the way it expects:

- **MOD / DELAY / REVERB / TALK** → the single **proxy** knob `modfx_param` /
  `dly_param` / `rev_param` / `talk_param` (+ `*_pfocus`). The DSP "focus-shadow"
  routes that one knob to the focus-selected extra. Keep binding the proxy.
  (DELAY/REVERB proxies are **inert** — ported FX can't take extra knobs.)
- **LO-FI** → the PARAMS knob focus-routes on the GUI side to the **raw named
  params** `lofi_bits` / `lofi_srate` / `lofi_compand` / `lofi_alias`
  (`lofi_param` exists but is engine-inert — do not rely on it). This is the ONE
  slot that uses raw params, deliberately.

## 5. editor.html / fonts / bootstrap (offline WebView)

- **Google Fonts is CSP-blocked** in the plugin WebView. Ship fonts as local
  `@font-face` in style.css pointing at the bundled TTFs (BarlowSemiCondensed-
  SemiBold/Bold, Doto-Variable, Michroma-Regular). No `<link href="fonts.g…">`.
- Bootstrap order in editor.html: `check_native_interop.js` (classic script)
  first, then a module that does `import * as Juce from "./index.js";
  window.Juce = Juce; import("./app.js");`. Mount into `#root`.
- Don't ship `support.js` (design-tool artifact; the production app.js is
  self-contained).

## 6. Params the DSP grew to match your design (rely on these now)

`flt2_env` (F2 envelope), `lfo_sync` (global LFO tempo sync), `modfx_param /
dly_param / rev_param / talk_param` + `lofi_pfocus / talk_pfocus`,
`detune_voices` (Choice), `flt_route` & `fx_prepost` (Bool). All host-automatable
and in Params.h.

## 7. Pre-handoff self-check (run before every GUI handoff)

1. Every `GLOBAL_ID`/`TONE_ID` value exists in Params.h **and** its KIND matches
   the param class. (Grep Params.h; don't guess.)
2. No hardcoded wave/preset data drives behaviour — all fetched via native fns.
3. Every action button calls its `Bridge.fn(...)` (load/save/rename/delete/panic).
4. No control only mutates `UI.*` when it represents a real APVTS param.
5. Fonts local; bootstrap loads check_native_interop → window.Juce → app.js.
6. Open editor.html in a browser: no console errors; every knob/menu moves.

The DSP↔GUI diffs from this round are in `GUI_DSP_DIFF.md` (design vs DSP) and
`GUI_PROD_MISMATCH.md` (production app.js binding cross-check) — same folder.
