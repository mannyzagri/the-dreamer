// The Dreamer -- GUI v6 production logic (JUCE 8 WebView2 editor).
// v5 face + v6 renovation (design-handoff/v6/GUI_SPEC.md deltas), rebound
// to the canonical DSP_BUILD.md section-9 parameter IDs (per-tone params
// use SUFFIXES _a.._d; see plugin/Params.h -- IDs are LOCKED).
// House pattern: rubber-rhino gui.
//
// Standalone-mock fallback: when a param ID is not registered with the
// backend (window.__JUCE__ placeholder from check_native_interop.js, i.e.
// plain browser / headless Chrome), the state factories below return a
// local stand-in exposing the same minimal interface (local value +
// listener list), seeded from a demo state matching the handoff PNG, so
// the page is fully rendered and interactive without the plugin.
import * as Juce from "./index.js";

const $ = (id) => document.getElementById(id);
const el = (t, c) => { const d = document.createElement(t); if (c) d.className = c; return d; };
const NS = "http://www.w3.org/2000/svg";
const svgEl = (t) => document.createElementNS(NS, t);
const clamp01 = (v) => Math.max(0, Math.min(1, v));
const initData = window.__JUCE__.initialisationData;

//==============================================================================
// Choice lists -- order = APVTS choice index (LOCKED, Params.h). The wave
// list is design-handoff/v6/wave-list-bank3.js VERBATIM (104, bank3 order:
// 78 cycles + 16 [ENS] loops + 10 [SHOT] one-shots); display uppercases at
// render time, tag shows right-aligned in the overlay.
const WAVES = [
  { cat: "Basic", name: "Saw", tag: "" },          { cat: "Basic", name: "Square", tag: "" },
  { cat: "Basic", name: "Triangle", tag: "" },     { cat: "Basic", name: "Sine", tag: "" },
  { cat: "Pad", name: "SoftSaw 1", tag: "" },      { cat: "Pad", name: "SoftSaw 2", tag: "" },
  { cat: "Pad", name: "SoftSaw 3", tag: "" },      { cat: "Pad", name: "AsymSaw 1", tag: "" },
  { cat: "Pad", name: "AsymSaw 2", tag: "" },      { cat: "Pad", name: "Hollow 1", tag: "" },
  { cat: "Pad", name: "Hollow 2", tag: "" },       { cat: "Pad", name: "Hollow 3", tag: "" },
  { cat: "Pad", name: "Hollow 4", tag: "" },       { cat: "Pad", name: "Tannerin", tag: "" },
  { cat: "Pad", name: "Breath 1", tag: "" },       { cat: "Pad", name: "Breath 2", tag: "" },
  { cat: "Pad", name: "Breath 3", tag: "" },       { cat: "Pad", name: "Glass Organ 1", tag: "" },
  { cat: "Pad", name: "Glass Organ 2", tag: "" },  { cat: "Pad", name: "Glass Organ 3", tag: "" },
  { cat: "Pad", name: "SinHarm 1", tag: "" },      { cat: "Pad", name: "SinHarm 2", tag: "" },
  { cat: "String", name: "StringBox 1", tag: "" }, { cat: "String", name: "StringBox 2", tag: "" },
  { cat: "String", name: "StringBox 3", tag: "" }, { cat: "String", name: "Violin 1", tag: "" },
  { cat: "String", name: "Violin 2", tag: "" },    { cat: "String", name: "Cello 1", tag: "" },
  { cat: "String", name: "Cello 2", tag: "" },     { cat: "String", name: "Cello 3", tag: "" },
  { cat: "Vox", name: "Voice 1", tag: "" },        { cat: "Vox", name: "Voice 2", tag: "" },
  { cat: "Vox", name: "Voice 3", tag: "" },        { cat: "Vox", name: "Voice 4", tag: "" },
  { cat: "Vox", name: "Voice 5", tag: "" },        { cat: "Vox", name: "Voice Bright 1", tag: "" },
  { cat: "Vox", name: "Voice Bright 2", tag: "" }, { cat: "Bell", name: "FM Bell 1", tag: "" },
  { cat: "Bell", name: "FM Bell 2", tag: "" },     { cat: "Bell", name: "FM Bell 3", tag: "" },
  { cat: "Bell", name: "FM Bell 4", tag: "" },     { cat: "Bell", name: "FM Bell 5", tag: "" },
  { cat: "Bell", name: "FM Bell 6", tag: "" },     { cat: "Bell", name: "Tine Bright", tag: "" },
  { cat: "Bell", name: "EP Tine 1", tag: "" },     { cat: "Bell", name: "EP Tine 2", tag: "" },
  { cat: "Bell", name: "DigiBell 1", tag: "" },    { cat: "Bell", name: "DigiBell 2", tag: "" },
  { cat: "Bell", name: "DigiBell 3", tag: "" },    { cat: "Bell", name: "DigiBell 4", tag: "" },
  { cat: "Bell", name: "DigiBell 5", tag: "" },    { cat: "Bell", name: "Chime 1", tag: "" },
  { cat: "Bell", name: "Chime 2", tag: "" },       { cat: "Bell", name: "Chime 3", tag: "" },
  { cat: "Bell", name: "Hollow Bell 1", tag: "" }, { cat: "Bell", name: "Hollow Bell 2", tag: "" },
  { cat: "Bell", name: "Hollow Bell 3", tag: "" }, { cat: "Metal", name: "Spectrum 1", tag: "" },
  { cat: "Metal", name: "Spectrum 2", tag: "" },   { cat: "Metal", name: "Spectrum 3", tag: "" },
  { cat: "Metal", name: "Spectrum 4", tag: "" },   { cat: "Metal", name: "Spectrum 5", tag: "" },
  { cat: "Metal", name: "Spectrum 6", tag: "" },   { cat: "Metal", name: "Spectrum 7", tag: "" },
  { cat: "Metal", name: "RawMetal 1", tag: "" },   { cat: "Metal", name: "RawMetal 2", tag: "" },
  { cat: "Metal", name: "RawMetal 3", tag: "" },   { cat: "Metal", name: "RawMetal 4", tag: "" },
  { cat: "Metal", name: "Clang 1", tag: "" },      { cat: "Metal", name: "Clang 2", tag: "" },
  { cat: "Metal", name: "Clang 3", tag: "" },      { cat: "Metal", name: "BitHash 1", tag: "" },
  { cat: "Metal", name: "BitHash 2", tag: "" },    { cat: "Metal", name: "BitHash 3", tag: "" },
  { cat: "Metal", name: "BitHash 4", tag: "" },    { cat: "Metal", name: "GrainMetal 1", tag: "" },
  { cat: "Metal", name: "GrainMetal 2", tag: "" }, { cat: "Metal", name: "GrainMetal 3", tag: "" },
  { cat: "Ens", name: "STRINGBOX_ENS", tag: "ENS" },   { cat: "Ens", name: "CATHEDRAL", tag: "ENS" },
  { cat: "Ens", name: "CHOIR_MORPH", tag: "ENS" },     { cat: "Ens", name: "GLASS_CHOIR", tag: "ENS" },
  { cat: "Ens", name: "SOLINA_DREAM", tag: "ENS" },    { cat: "Ens", name: "CELLO_SECTION", tag: "ENS" },
  { cat: "Ens", name: "THEREMIN_SWARM", tag: "ENS" },  { cat: "Ens", name: "DARK_BREATH", tag: "ENS" },
  { cat: "Ens", name: "FM_HALO", tag: "ENS" },         { cat: "Ens", name: "SPECTRAL_TIDE", tag: "ENS" },
  { cat: "Ens", name: "SAW_CLOUD", tag: "ENS" },       { cat: "Ens", name: "TANNERIN_GHOST", tag: "ENS" },
  { cat: "Ens", name: "ORGAN_MASS", tag: "ENS" },      { cat: "Ens", name: "VOICE_OF_STEEL", tag: "ENS" },
  { cat: "Ens", name: "BELL_GARDEN", tag: "ENS" },     { cat: "Ens", name: "ACID_MIRAGE", tag: "ENS" },
  { cat: "Shot", name: "CHIFF", tag: "SHOT" },         { cat: "Shot", name: "BREATH", tag: "SHOT" },
  { cat: "Shot", name: "CLICK", tag: "SHOT" },         { cat: "Shot", name: "MALLET_TICK", tag: "SHOT" },
  { cat: "Shot", name: "NOISE_SWELL", tag: "SHOT" },   { cat: "Shot", name: "GLASS_TICK", tag: "SHOT" },
  { cat: "Shot", name: "VOX_PLOSIVE", tag: "SHOT" },   { cat: "Shot", name: "METAL_HIT", tag: "SHOT" },
  { cat: "Shot", name: "SUB_THUMP", tag: "SHOT" },     { cat: "Shot", name: "TAPE_START", tag: "SHOT" },
];
const SHAPES    = ["OFF", "SOFT FOLD", "HARD FOLD", "SINE FOLD", "ASYM", "DRIVE"];
const TVFTYPES  = ["LP24", "LP12", "BP", "HP"];              // LCD short form
const TVFFULL   = ["LP 24", "LP 12", "BP", "HP"];            // menu rows
const FTYPES    = ["LP 24", "LP 12", "BP", "HP", "LIQUID", "CLASSIC", "LADDER",
                   "NOTCH", "COMB +", "COMB -", "NL3 N+LP", "FORMANT", "ALLPASS", "DREAMPLN"];
const GLFOWAVES = ["TRI", "SIN", "SAW", "SQR", "S+H"];
const ORBSHAPES = ["SAW", "TRI", "SIN", "SQR", "S+H"];
const AUXDESTS  = ["PITCH", "START", "SHAPE", "PAN", "NOISE"];
const LFODESTS  = ["PITCH", "CUTOFF", "SHAPE", "LEVEL"];
const MSRC      = ["G-LFO", "VEC PHS", "AUX", "VELO", "WHEEL"];
const MDST      = ["PITCH", "CUT 1", "CUT 2", "MORPH", "SHAPE", "VEC PHS", "PAN", "NOISE"];
const MODFX     = ["CHORUS", "FLANGER", "PHASER", "ENSEMBLE"];
const DLYMODES  = ["DIGITAL", "TAPE", "PONG"];
const REVTYPES  = ["ROOM", "HALL", "PLATE"];
const TONES     = ["A", "B", "C", "D"];
const SFX       = ["_a", "_b", "_c", "_d"];   // per-tone id SUFFIX (Params.h tid)

//==============================================================================
// Standalone-mock demo defaults (normalised sliders / bools / choice indices)
// -- cosmetics only, mirrors the prototype's boot state / handoff PNG. The
// real plugin never reads these (relay initial-update wins).
const MOCK = { sliders: {}, toggles: {}, combos: {} };
{
  const lv = [.8, .62, .5, .4], st = [.1, .2, .05, .15];
  const ct = [.5, .6, .45, .55], wv = [11, 18, 28, 37];
  SFX.forEach((sx, i) => {
    Object.assign(MOCK.sliders, {
      ["oct" + sx]: .5, ["fine" + sx]: .5, ["level" + sx]: lv[i],
      ["velo" + sx]: .4, ["start" + sx]: st[i], ["pan" + sx]: .5,
      ["dir" + sx]: i * .25, ["vint" + sx]: .6, ["shape_depth" + sx]: 0,
      ["noise" + sx]: 0, ["noise_color" + sx]: 0,
      ["tvf_cut" + sx]: ct[i], ["tvf_res" + sx]: .25, ["tvf_env" + sx]: .5,
      ["tvf_kf" + sx]: .3,
      ["tvf_a" + sx]: .05, ["tvf_d" + sx]: .6, ["tvf_s" + sx]: .5, ["tvf_r" + sx]: .55,
      ["tva_a" + sx]: .2,  ["tva_d" + sx]: .7, ["tva_s" + sx]: .8, ["tva_r" + sx]: .65,
      ["aux_a" + sx]: 0,   ["aux_d" + sx]: .4, ["aux_s" + sx]: 0,  ["aux_r" + sx]: .3,
      ["aux_amt" + sx]: .5, ["lfo_depth" + sx]: .15,
    });
    MOCK.toggles["on" + sx] = true;
    MOCK.toggles["start_random" + sx] = false;
    Object.assign(MOCK.combos, {
      ["wave" + sx]: wv[i], ["shape" + sx]: 0, ["tvf_type" + sx]: 0,
      ["aux_dest" + sx]: 0, ["lfo_dest" + sx]: 0,
    });
  });
  Object.assign(MOCK.sliders, {
    vec_phase: .12, vec_orbit_rate: .3,
    vec_penv_start: 0, vec_penv_end: .5, vec_penv_time: .5,
    lfo_rate: .4, mtx1_amt: .8, mtx2_amt: .32, mtx3_amt: .5,
    flt1_cut: .52, flt1_res: .35, flt1_env: .5,
    flt2_cut: .7, flt2_res: .2, flt2_morph: .4,
    modfx_rate: .3, modfx_depth: .5, modfx_mix: .45,
    dly_time: .5, dly_fb: .35, dly_mix: .3,
    rev_size: .6, rev_damp: .4, rev_mix: .35, master: .78, drift: 0,
  });
  Object.assign(MOCK.toggles, {
    vec_orbit_on: true, vec_penv_on: false, vec_penv_loop: false,
    vec_orbit_voice: false,
    modfx_on: true, dly_on: true, rev_on: true,
  });
  Object.assign(MOCK.combos, {
    lfo_shape: 0, vec_orbit_shape: 0,
    mtx1_src: 0, mtx2_src: 1, mtx3_src: 2,
    mtx1_dst: 3, mtx2_dst: 4, mtx3_dst: 7,
    flt1_type: 6, flt2_type: 13, flt_route: 0,
    modfx_type: 0, dly_mode: 0, rev_type: 1,
  });
}

//==============================================================================
// State factories: real JUCE relay state when the backend registered the ID,
// local mock otherwise. Cached per ID so controls sharing a param (tone
// strip fader + LEVEL knob, on-LED + column dim) share one state.
function mockEvents() {
  const l = [];
  return { addListener: (f) => l.push(f), fire: () => l.forEach((f) => f()) };
}
const sCache = new Map(), tCache = new Map(), cCache = new Map();

function sliderState(id) {
  if (sCache.has(id)) return sCache.get(id);
  let st;
  if (initData.__juce__sliders.includes(id)) {
    st = Juce.getSliderState(id);
  } else {
    const ev = mockEvents();
    let v = (id in MOCK.sliders) ? MOCK.sliders[id] : .5;
    st = {
      getNormalisedValue: () => v,
      setNormalisedValue(n) { v = clamp01(n); ev.fire(); },
      getScaledValue: () => v,
      sliderDragStarted() {}, sliderDragEnded() {},
      valueChangedEvent: ev, propertiesChangedEvent: mockEvents(),
    };
  }
  sCache.set(id, st);
  return st;
}

function toggleState(id) {
  if (tCache.has(id)) return tCache.get(id);
  let st;
  if (initData.__juce__toggles.includes(id)) {
    st = Juce.getToggleState(id);
  } else {
    const ev = mockEvents();
    let v = !!MOCK.toggles[id];
    st = {
      getValue: () => v,
      setValue(n) { v = !!n; ev.fire(); },
      valueChangedEvent: ev, propertiesChangedEvent: mockEvents(),
    };
  }
  tCache.set(id, st);
  return st;
}

function comboState(id) {
  if (cCache.has(id)) return cCache.get(id);
  let st;
  if (initData.__juce__comboBoxes.includes(id)) {
    st = Juce.getComboBoxState(id);
  } else {
    const ev = mockEvents();
    let v = MOCK.combos[id] || 0;
    st = {
      getChoiceIndex: () => v,
      setChoiceIndex(i) { v = i; ev.fire(); },
      valueChangedEvent: ev, propertiesChangedEvent: mockEvents(),
    };
  }
  cCache.set(id, st);
  return st;
}

// Safe choice index (real ComboBoxState can report junk before its
// properties arrive; wrap/step math always uses OUR locked list length).
const idx = (st, len) => {
  const i = st.getChoiceIndex();
  return Math.max(0, Math.min(len - 1, isFinite(i) ? Math.round(i) : 0));
};

// Per-tone state array (base id + _a.._d suffix, Params.h tid()).
const tSliders = (base) => SFX.map((sx) => sliderState(base + sx));
const tToggles = (base) => SFX.map((sx) => toggleState(base + sx));
const tCombos  = (base) => SFX.map((sx) => comboState(base + sx));

//==============================================================================
// UI-only state: tone selection + redraw hooks run when it changes.
const SEL = { i: 0 };
const rebinds = [];

//==============================================================================
// Main-LCD touched line: name + value + 14-cell meter. Coarse display:
// 0-127 unipolar, -63..+63 bipolar (fmt "bip": pan/fine/amounts),
// -2..+2 octaves (fmt "oct").
const meterCells = [];
for (let i = 0; i < 14; i++) meterCells.push($("lcdMeter").appendChild(el("span")));
function setTouched(label, norm, fmt) {
  let txt;
  if (fmt === "bip") {
    const v = Math.round((norm - .5) * 126);
    txt = (v > 0 ? "+" : "") + v;
  } else if (fmt === "oct") {
    const v = Math.round(norm * 4) - 2;
    txt = (v > 0 ? "+" : "") + v;
  } else {
    txt = String(Math.round(norm * 127));
  }
  $("lcdTouched").textContent = label + " " + txt.padStart(3, " ");
  const fill = Math.round(norm * 14);
  meterCells.forEach((c, i) => { c.style.background = i < fill ? "#ffd23f" : "#1c1a10"; });
}

//==============================================================================
// Drag law: vertical, 140px full range, pointer capture, no easing.
// opts: { fmt } touched-line format, { quant } value map applied while
// dragging (octave detents, matrix center snap).
function bindVDrag(elm, stFn, labelFn, after, opts) {
  const o = opts || {};
  elm.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    const s = stFn();
    s.sliderDragStarted();
    const y0 = e.clientY, n0 = s.getNormalisedValue();
    try { elm.setPointerCapture(e.pointerId); } catch (err) {}
    const mv = (ev) => {
      let v = clamp01(n0 + (y0 - ev.clientY) / 140);
      if (o.quant) v = o.quant(v);
      s.setNormalisedValue(v);
      setTouched(labelFn(), v, o.fmt);
      if (after) after();
    };
    const up = () => {
      elm.removeEventListener("pointermove", mv);
      elm.removeEventListener("pointerup", up);
      s.sliderDragEnded();
    };
    elm.addEventListener("pointermove", mv);
    elm.addEventListener("pointerup", up);
  });
}

//==============================================================================
// Knob factory: skirt / fluted cap (76%) / solid centre / 2px pointer
// rotating -135..+135 deg. o: {states, cur, label, ptr, size, touch}
function makeKnob(mount, o) {
  const size = o.size || 36;
  const inner = Math.round(size * .76);
  const wrap = el("div", "kwrap");
  wrap.style.width = (size + 12) + "px";
  const skirt = el("div", "kskirt");
  skirt.style.width = skirt.style.height = size + "px";
  const cap = el("div", "kcap");
  cap.style.width = cap.style.height = inner + "px";
  const capin = el("div", "kcapin");
  const rot = el("div", "krot");
  const ptr = el("div", "kptr");
  ptr.style.height = Math.round(inner * .34) + "px";
  if (o.ptr) ptr.style.background = o.ptr;
  rot.appendChild(ptr);
  cap.append(capin, rot);
  skirt.appendChild(cap);
  wrap.appendChild(skirt);
  if (o.label) {
    const l = el("div", "lbl");
    l.textContent = o.label;
    wrap.appendChild(l);
  }
  mount.appendChild(wrap);

  const cur = o.cur || (() => 0);
  const st = () => o.states[cur()];
  const draw = () => {
    rot.style.transform = "rotate(" + (-135 + st().getNormalisedValue() * 270) + "deg)";
  };
  o.states.forEach((s) => {
    s.valueChangedEvent.addListener(draw);
    s.propertiesChangedEvent.addListener(draw);
  });
  if (o.states.length > 1) rebinds.push(draw);
  const labelFn = typeof o.touch === "function" ? o.touch : () => (o.touch || o.label);
  bindVDrag(skirt, st, labelFn, draw, { fmt: o.fmt, quant: o.quant });
  draw();
  return draw;
}

// Vertical slider factory (9px track, 19x9 cap; same 140px drag law).
function makeVSlider(mount, o) {   // {states, cur, h, letter, touch}
  const col = el("div", "bcol");
  const sl = el("div", "vsl");
  sl.style.height = o.h + "px";
  const cap = el("div", "vcap");
  sl.appendChild(cap);
  col.appendChild(sl);
  if (o.letter) {
    const l = el("div", "plab7");
    l.textContent = o.letter;
    col.appendChild(l);
  }
  mount.appendChild(col);

  const cur = o.cur || (() => 0);
  const st = () => o.states[cur()];
  const draw = () => {
    cap.style.bottom = Math.round(st().getNormalisedValue() * (o.h - 9)) + "px";
  };
  o.states.forEach((s) => {
    s.valueChangedEvent.addListener(draw);
    s.propertiesChangedEvent.addListener(draw);
  });
  if (o.states.length > 1) rebinds.push(draw);
  const labelFn = typeof o.touch === "function" ? o.touch : () => o.touch;
  bindVDrag(sl, st, labelFn, draw);
  draw();
  return draw;
}

//==============================================================================
// Menu overlay (scrim, inverse-video cursor row, ESC/scrim close).
let menuOpen = false;
function openMenu(title, rows, curI, pick) {   // rows: [{cat, name, tag?}]
  menuOpen = true;
  $("menuTitle").textContent = title;
  const list = $("menuList");
  list.innerHTML = "";
  rows.forEach((r, i) => {
    const row = el("div", "mrow" + (i === curI ? " cur" : ""));
    const num = el("span", "mnum"); num.textContent = String(i + 1).padStart(2, "0");
    const cat = el("span", "mcat"); cat.textContent = r.cat || "";
    const nm = el("span"); nm.textContent = (i === curI ? "▸ " : "") + r.name;
    row.append(num, cat, nm);
    if (r.tag) {   // entry-type tag ([ENS]/[SHOT]), right-aligned, dim
      const tg = el("span", "mtag");
      tg.textContent = "[" + r.tag + "]";
      row.appendChild(tg);
    }
    row.addEventListener("click", () => { pick(i); closeMenu(); });
    list.appendChild(row);
  });
  $("menuOverlay").style.display = "flex";
  const cur = list.children[curI];
  if (cur) cur.scrollIntoView({ block: "center" });
}
function closeMenu() { menuOpen = false; $("menuOverlay").style.display = "none"; }
$("menuOverlay").addEventListener("click", (e) => { if (e.target === $("menuOverlay")) closeMenu(); });
window.addEventListener("keydown", (e) => { if (e.key === "Escape") { closeMenu(); closePenv(); } });
const rowsOf = (arr) => arr.map((n) => ({ cat: "", name: n }));
const waveRows = () => WAVES.map((w) =>
  ({ cat: w.cat.toUpperCase(), name: w.name.toUpperCase(), tag: w.tag }));

// Combo-display helper: draw now + on change + on tone rebind.
function comboDraw(states, perTone, draw) {
  states.forEach((s) => {
    s.valueChangedEvent.addListener(draw);
    s.propertiesChangedEvent.addListener(draw);
  });
  if (perTone) rebinds.push(draw);
  draw();
}

// Stepper pair: +-1 choice index with wrap.
function bindStep(dec, inc, stFn, len) {
  const bump = (d) => () => {
    const s = stFn();
    s.setChoiceIndex((idx(s, len) + d + len) % len);
  };
  dec.addEventListener("click", bump(-1));
  inc.addEventListener("click", bump(1));
}

//==============================================================================
// Header: wave glyph, MASTER knob (binds `master`), power LED is static.
const glyphBars = [];
for (let i = 0; i < 12; i++) glyphBars.push($("waveGlyph").appendChild(el("div")));
const waveStates = tCombos("wave");
function drawGlyph() {
  const w = idx(waveStates[SEL.i], WAVES.length);
  glyphBars.forEach((b, i) => {
    b.style.height =
      Math.max(3, Math.round(4 + 24 * Math.abs(Math.sin((i / 11) * Math.PI * (1 + (w % 3) * .5))))) + "px";
  });
}
makeKnob($("masterMount"), { states: [sliderState("master")], label: "MASTER", size: 40, touch: "MASTER" });

//==============================================================================
// TONES strip: per column select LED+button, 150px level fader, ON LED+button.
const selLeds = [];
{
  const mount = $("toneCols");
  TONES.forEach((name, i) => {
    const col = el("div", "tcol");
    const nm = el("div", "tname"); nm.textContent = name;
    const selLed = el("div", "led");
    const selBtn = el("div", "pbtn tbtn");
    col.append(nm, selLed, selBtn);
    selLeds.push(selLed);
    selBtn.addEventListener("click", () => select(i));

    makeVSlider(col, { states: [sliderState("level" + SFX[i])], h: 150, touch: () => "LEVEL " + name });

    const onLed = el("div", "led"); onLed.style.marginTop = "2px";
    const onBtn = el("div", "pbtn tbtn");
    const onLbl = el("div", "tonlbl"); onLbl.textContent = "ON";
    col.append(onLed, onBtn, onLbl);

    const onState = toggleState("on" + SFX[i]);
    const drawOn = () => {
      const on = !!onState.getValue();
      onLed.classList.toggle("on", on);
      col.style.opacity = on ? "1" : ".55";
    };
    onState.valueChangedEvent.addListener(drawOn);
    drawOn();
    onBtn.addEventListener("click", () => onState.setValue(!onState.getValue()));
    mount.appendChild(col);
  });
}

//==============================================================================
// TONE EDIT (rebinds to the selected tone).
const curTone = () => SEL.i;
const letter = () => TONES[SEL.i];

// WAVE LCD + steppers + menu
comboDraw(waveStates, true, () => {
  const w = WAVES[idx(waveStates[SEL.i], WAVES.length)];
  $("waveLcd").textContent = w.cat.toUpperCase() + " > " + w.name.toUpperCase();
  drawGlyph();
});
bindStep($("waveDec"), $("waveInc"), () => waveStates[SEL.i], WAVES.length);
$("waveLcd").addEventListener("click", () =>
  openMenu("WAVE SELECT — TONE " + letter(), waveRows(),
    idx(waveStates[SEL.i], WAVES.length), (i) => waveStates[SEL.i].setChoiceIndex(i)));

// SHAPE LCD + steppers + menu
const shapeStates = tCombos("shape");
comboDraw(shapeStates, true, () => {
  $("shapeLcd").textContent = SHAPES[idx(shapeStates[SEL.i], SHAPES.length)];
});
bindStep($("shapeDec"), $("shapeInc"), () => shapeStates[SEL.i], SHAPES.length);
$("shapeLcd").addEventListener("click", () =>
  openMenu("SHAPER — TONE " + letter(), rowsOf(SHAPES),
    idx(shapeStates[SEL.i], SHAPES.length), (i) => shapeStates[SEL.i].setChoiceIndex(i)));

// 9-knob row: OCTV FINE START [RND] LEVEL VELO SHP-DPT NOISE NOISE-COL PAN
// (Ø36 as v5; SHP DPT yellow; OCTV steps 5 detents -2..+2; FINE/PAN bipolar).
// The RND button+LED (start_random_[t]) sits next to START per GUI_SPEC.
[["oct", "OCTV", { fmt: "oct", quant: (v) => Math.round(v * 4) / 4 }],
 ["fine", "FINE", { fmt: "bip" }],
 ["start", "START", {}],
 ["level", "LEVEL", {}],
 ["velo", "VELO", {}],
 ["shape_depth", "SHP DPT", { ptr: "#ffd23f" }],
 ["noise", "NOISE", {}],
 ["noise_color", "NOISE COL", {}],
 ["pan", "PAN", { fmt: "bip" }]].forEach(([base, lbl, o]) => {
  makeKnob($("teKnobs"), {
    states: tSliders(base), cur: curTone, label: lbl, size: 36,
    ptr: o.ptr || null, touch: lbl, fmt: o.fmt, quant: o.quant,
  });
  if (base === "start") {   // START RND: per-tone note-on start randomize
    const col = el("div", "rndcol");
    const led = el("div", "led");
    const btn = el("div", "pbtn");
    const lb = el("div", "plab7"); lb.textContent = "RND";
    col.append(led, btn, lb);
    $("teKnobs").appendChild(col);
    const rndStates = tToggles("start_random");
    const drawRnd = () => led.classList.toggle("on", !!rndStates[SEL.i].getValue());
    rndStates.forEach((s) => s.valueChangedEvent.addListener(drawRnd));
    rebinds.push(drawRnd);
    drawRnd();
    btn.addEventListener("click", () => {
      const s = rndStates[SEL.i];
      s.setValue(!s.getValue());
    });
  }
});

// TVF: TYPE LCD (+ < > mini-stepper) + 4 red knobs + three ADSR banks
const tvfTypeStates = tCombos("tvf_type");
comboDraw(tvfTypeStates, true, () => {
  $("tvfTypeLcd").textContent = TVFTYPES[idx(tvfTypeStates[SEL.i], TVFTYPES.length)];
});
bindStep($("tvfTypeDec"), $("tvfTypeInc"), () => tvfTypeStates[SEL.i], TVFTYPES.length);
$("tvfTypeLcd").addEventListener("click", () =>
  openMenu("TVF TYPE — TONE " + letter(), rowsOf(TVFFULL),
    idx(tvfTypeStates[SEL.i], TVFTYPES.length), (i) => tvfTypeStates[SEL.i].setChoiceIndex(i)));

[["tvf_cut", "CUT"], ["tvf_res", "RES"], ["tvf_env", "ENV"], ["tvf_kf", "KF"]].forEach(([base, lbl]) => {
  makeKnob($("tvfKnobs"), {
    states: tSliders(base), cur: curTone, label: lbl, size: 34, ptr: "#ff5b6e", touch: "TVF " + lbl,
  });
});

function makeBank(mountId, prefix, title) {
  const bank = $(mountId);
  const row = el("div", "bankrow");
  bank.appendChild(row);
  [["a", "A"], ["d", "D"], ["s", "S"], ["r", "R"]].forEach(([sfx, l]) => {
    makeVSlider(row, {
      states: tSliders(prefix + "_" + sfx), cur: curTone, h: 62, letter: l,
      touch: () => title + " " + l,
    });
  });
  const t = el("div", "banktitle");
  const tl = el("div", "bttl");
  tl.textContent = title + " ENV";
  t.appendChild(tl);
  bank.appendChild(t);
  return t;
}
makeBank("bankTvf", "tvf", "TVF");
makeBank("bankTva", "tva", "TVA");
const auxTitleRow = makeBank("bankAux", "aux", "AUX");
// AUX dest button (cycles PITCH/START/SHAPE/PAN/NOISE) beside the AUX ENV title
const auxDestBtn = el("div", "cbtn");
auxDestBtn.style.width = "44px";
auxTitleRow.appendChild(auxDestBtn);
const auxDestStates = tCombos("aux_dest");
comboDraw(auxDestStates, true, () => {
  auxDestBtn.textContent = AUXDESTS[idx(auxDestStates[SEL.i], AUXDESTS.length)];
});
auxDestBtn.addEventListener("click", () => {
  const s = auxDestStates[SEL.i];
  s.setChoiceIndex((idx(s, AUXDESTS.length) + 1) % AUXDESTS.length);
});

// TONE LFO + per-tone VECTOR knobs
makeKnob($("lfoDepthMount"), { states: tSliders("lfo_depth"), cur: curTone, label: "DEPTH", size: 30, touch: "LFO DEPTH" });
const lfoDestStates = tCombos("lfo_dest");
comboDraw(lfoDestStates, true, () => {
  $("lfoDestBtn").textContent = LFODESTS[idx(lfoDestStates[SEL.i], LFODESTS.length)];
});
$("lfoDestBtn").addEventListener("click", () => {
  const s = lfoDestStates[SEL.i];
  s.setChoiceIndex((idx(s, LFODESTS.length) + 1) % LFODESTS.length);
});
const dirStates = tSliders("dir"), intStates = tSliders("vint");
makeKnob($("vecMount"), { states: dirStates, cur: curTone, label: "DIR", size: 30, ptr: "#ffd23f", touch: "DIR" });
makeKnob($("vecMount"), { states: intStates, cur: curTone, label: "INT", size: 30, ptr: "#ffd23f", touch: "INT" });

//==============================================================================
// FILTERS: two strips + SER/PAR routing.
function filterStrip(lcdId, decId, incId, typeId, knobDefs, knobMountId, menuTitle) {
  const state = comboState(typeId);
  comboDraw([state], false, () => {
    $(lcdId).textContent = FTYPES[idx(state, FTYPES.length)];
  });
  bindStep($(decId), $(incId), () => state, FTYPES.length);
  $(lcdId).addEventListener("click", () =>
    openMenu(menuTitle, rowsOf(FTYPES), idx(state, FTYPES.length), (i) => state.setChoiceIndex(i)));
  knobDefs.forEach(([id, lbl, ptr]) => {
    makeKnob($(knobMountId), { states: [sliderState(id)], label: lbl, size: 36, ptr, touch: lbl });
  });
}
filterStrip("f1Lcd", "f1Dec", "f1Inc", "flt1_type",
  [["flt1_cut", "CUT", "#ff5b6e"], ["flt1_res", "RES", "#ff5b6e"], ["flt1_env", "ENV", "#ff5b6e"]],
  "f1Knobs", "FILTER 1 TYPE");
filterStrip("f2Lcd", "f2Dec", "f2Inc", "flt2_type",
  [["flt2_cut", "CUT", "#ff5b6e"], ["flt2_res", "RES", "#ff5b6e"], ["flt2_morph", "MORPH", "#ffd23f"]],
  "f2Knobs", "FILTER 2 TYPE");

const routeState = comboState("flt_route");
comboDraw([routeState], false, () => {
  const r = idx(routeState, 2);
  $("serLed").classList.toggle("on", r === 0);
  $("parLed").classList.toggle("on", r === 1);
});
$("routeBtn").addEventListener("click", () => routeState.setChoiceIndex(1 - idx(routeState, 2)));

//==============================================================================
// DREAM VECTOR: PHASE/RATE knobs, SHAPE stepper, ORBIT/P-ENV toggles,
// P-ENV mini page, signal-flow display.
const vecPhaseState = sliderState("vec_phase");
const orbitRateState = sliderState("vec_orbit_rate");
const orbitState = toggleState("vec_orbit_on");
const penvState = toggleState("vec_penv_on");
makeKnob($("phaseMount"), { states: [vecPhaseState], label: "PHASE", size: 32, ptr: "#ffd23f", touch: "PHASE" });
makeKnob($("rateMount"), { states: [orbitRateState], label: "RATE", size: 32, touch: "RATE" });
function bindToggleRow(ledId, btnId, state) {
  const draw = () => $(ledId).classList.toggle("on", !!state.getValue());
  state.valueChangedEvent.addListener(draw);
  draw();
  $(btnId).addEventListener("click", () => state.setValue(!state.getValue()));
}
bindToggleRow("orbitLed", "orbitBtn", orbitState);
bindToggleRow("penvLed", "penvBtn", penvState);

// ORBIT SHAPE stepper (wraps SAW/TRI/SIN/SQR/S+H)
const orbitShapeState = comboState("vec_orbit_shape");
comboDraw([orbitShapeState], false, () => {
  $("orbitShapeBtn").textContent = ORBSHAPES[idx(orbitShapeState, ORBSHAPES.length)];
});
$("orbitShapeBtn").addEventListener("click", () =>
  orbitShapeState.setChoiceIndex((idx(orbitShapeState, ORBSHAPES.length) + 1) % ORBSHAPES.length));

// P-ENV mini page: START/END angle + TIME knobs, LOOP toggle. LCD page on
// the scrim like the wave menu, instant open/close (no easing).
let penvOpen = false;
function openPenv() { penvOpen = true; $("penvOverlay").style.display = "flex"; }
function closePenv() { penvOpen = false; $("penvOverlay").style.display = "none"; }
$("penvEditBtn").addEventListener("click", openPenv);
$("penvOverlay").addEventListener("click", (e) => { if (e.target === $("penvOverlay")) closePenv(); });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_start")], label: "START", size: 32, ptr: "#ffd23f", touch: "P-ENV START" });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_end")], label: "END", size: 32, ptr: "#ffd23f", touch: "P-ENV END" });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_time")], label: "TIME", size: 32, touch: "P-ENV TIME" });
bindToggleRow("penvLoopLed", "penvLoopBtn", toggleState("vec_penv_loop"));

// Params with no panel control stay wired (hidden states; report note):
// aux env depth, global humanize drift, per-voice orbit free-run.
SFX.forEach((sx) => sliderState("aux_amt" + sx));
sliderState("drift");
toggleState("vec_orbit_voice");

// -- signal-flow "SIGNAL FLOW · PHASE" neural-net display -------------------
const toneOnStates = tToggles("on");
const flowEls = [];
let phaseBar;
{
  const svg = $("flowSvg");
  const ys = [22, 52, 82, 112], x0 = 22, x1 = 117, y1 = 66;
  for (let i = 0; i < 4; i++) {
    const y = ys[i];
    const path = svgEl("path");
    path.setAttribute("d", `M ${x0 + 9} ${y} C ${x0 + 45} ${y}, ${x1 - 40} ${y1}, ${x1} ${y1}`);
    path.setAttribute("fill", "none");
    path.setAttribute("stroke", "#ffd23f");
    svg.appendChild(path);
    const node = svgEl("circle");
    node.setAttribute("cx", x0); node.setAttribute("cy", y);
    svg.appendChild(node);
    const lt = svgEl("text");
    lt.setAttribute("x", x0); lt.setAttribute("y", y + 3);
    lt.setAttribute("text-anchor", "middle");
    lt.setAttribute("font-family", "Doto"); lt.setAttribute("font-weight", "800");
    lt.setAttribute("font-size", "8");
    lt.textContent = TONES[i];
    svg.appendChild(lt);
    const dot = svgEl("circle");
    dot.setAttribute("fill", "#ff2b3e");
    dot.setAttribute("visibility", "hidden");
    svg.appendChild(dot);
    flowEls.push({ path, node, letter: lt, dot, y });
  }
  const sum = svgEl("circle");
  sum.setAttribute("cx", 126); sum.setAttribute("cy", 66); sum.setAttribute("r", 9);
  sum.setAttribute("fill", "none"); sum.setAttribute("stroke", "#464e94");
  sum.setAttribute("stroke-width", "1.5");
  svg.appendChild(sum);
  const sig = svgEl("text");
  sig.setAttribute("x", 126); sig.setAttribute("y", 69);
  sig.setAttribute("text-anchor", "middle");
  sig.setAttribute("font-family", "Doto"); sig.setAttribute("font-weight", "800");
  sig.setAttribute("font-size", "9"); sig.setAttribute("fill", "#ffd23f");
  sig.textContent = "Σ";
  svg.appendChild(sig);
  const mix = svgEl("text");
  mix.setAttribute("x", 126); mix.setAttribute("y", 88);
  mix.setAttribute("text-anchor", "middle");
  mix.setAttribute("font-family", "Doto"); mix.setAttribute("font-weight", "800");
  mix.setAttribute("font-size", "7"); mix.setAttribute("fill", "#9aa1d8");
  mix.textContent = "MIX";
  svg.appendChild(mix);
  const track = svgEl("rect");
  track.setAttribute("x", 6); track.setAttribute("y", 122);
  track.setAttribute("width", 138); track.setAttribute("height", 3);
  track.setAttribute("fill", "#1c2040");
  svg.appendChild(track);
  phaseBar = svgEl("rect");
  phaseBar.setAttribute("y", 122);
  phaseBar.setAttribute("width", 10); phaseBar.setAttribute("height", 3);
  phaseBar.setAttribute("fill", "#ff2b3e");
  svg.appendChild(phaseBar);
}

// Cosmetic orbit preview: the real DSP phase lives in the plugin; while
// ORBIT is on the display advances a local phase, the vec_phase param is
// never written by the animation.
let uiPhase = vecPhaseState.getNormalisedValue();
vecPhaseState.valueChangedEvent.addListener(() => { uiPhase = vecPhaseState.getNormalisedValue(); });
const flowPos = [0, .25, .5, .75];
setInterval(() => {
  if (menuOpen || penvOpen) return;
  if (orbitState.getValue())
    uiPhase = (uiPhase + .003 + orbitRateState.getNormalisedValue() * .012) % 1;
  else
    uiPhase = vecPhaseState.getNormalisedValue();
  const phi = uiPhase * 2 * Math.PI;
  for (let i = 0; i < 4; i++) {
    const e = flowEls[i];
    const on = !!toneOnStates[i].getValue();
    const dir = dirStates[i].getNormalisedValue();
    const vint = intStates[i].getNormalisedValue();
    const c = Math.max(0, Math.cos(phi - dir * 2 * Math.PI));
    const gain = on ? (1 - vint) + vint * c * c : 0;
    e.path.setAttribute("stroke-width", (0.5 + gain * 2.5).toFixed(1));
    e.path.setAttribute("opacity", (0.08 + gain * 0.8).toFixed(2));
    e.node.setAttribute("r", (6 + gain * 3).toFixed(1));
    e.node.setAttribute("fill", on ? "#ffd23f" : "#3a3520");
    e.node.setAttribute("opacity", on ? (0.3 + gain * 0.7).toFixed(2) : "0.25");
    e.letter.setAttribute("fill", i === SEL.i ? "#ff5b6e" : "#07070a");
    if (on && gain > .05) {
      flowPos[i] = (flowPos[i] + .01 + gain * .05) % 1;
      const t = flowPos[i], u = 1 - t;
      const bx = u * u * u * 31 + 3 * u * u * t * 67 + 3 * u * t * t * 77 + t * t * t * 117;
      const by = u * u * u * e.y + 3 * u * u * t * e.y + 3 * u * t * t * 66 + t * t * t * 66;
      e.dot.setAttribute("cx", bx.toFixed(1));
      e.dot.setAttribute("cy", by.toFixed(1));
      e.dot.setAttribute("r", (1.5 + gain * 2).toFixed(1));
      e.dot.setAttribute("opacity", (0.3 + gain * 0.7).toFixed(2));
      e.dot.setAttribute("visibility", "visible");
    } else {
      e.dot.setAttribute("visibility", "hidden");
    }
  }
  phaseBar.setAttribute("x", (6 + uiPhase * 128).toFixed(1));
}, 40);

//==============================================================================
// MOD MATRIX: 3 slots (SRC > DST, Ø26 BIPOLAR AMT knob w/ center detent,
// |amt| bar yellow=positive / red=negative) + G-LFO.
for (let n = 1; n <= 3; n++) {
  const row = el("div", "mrowline");
  const num = el("div", "mnumcol"); num.textContent = String(n);
  const srcBtn = el("div", "cbtn"); srcBtn.style.width = "52px";
  const arrow = el("div", "marrow"); arrow.textContent = ">";
  const dstBtn = el("div", "cbtn"); dstBtn.style.width = "52px";
  row.append(num, srcBtn, arrow, dstBtn);

  const srcState = comboState("mtx" + n + "_src");
  const dstState = comboState("mtx" + n + "_dst");
  const amtState = sliderState("mtx" + n + "_amt");
  comboDraw([srcState], false, () => { srcBtn.textContent = MSRC[idx(srcState, MSRC.length)]; });
  comboDraw([dstState], false, () => { dstBtn.textContent = MDST[idx(dstState, MDST.length)]; });
  srcBtn.addEventListener("click", () =>
    openMenu("MOD SOURCE — SLOT " + n, rowsOf(MSRC), idx(srcState, MSRC.length),
      (i) => srcState.setChoiceIndex(i)));
  dstBtn.addEventListener("click", () =>
    openMenu("MOD DEST — SLOT " + n, rowsOf(MDST), idx(dstState, MDST.length),
      (i) => dstState.setChoiceIndex(i)));

  // Ø26 AMT knob (20px cap, 4px centre inset, 7px yellow pointer, no label)
  const skirt = el("div", "kskirt");
  skirt.style.width = skirt.style.height = "26px";
  const cap = el("div", "kcap");
  cap.style.width = cap.style.height = "20px";
  const capin = el("div", "kcapin"); capin.style.inset = "4px";
  const rot = el("div", "krot");
  const ptr = el("div", "kptr");
  ptr.style.height = "7px"; ptr.style.top = "1px"; ptr.style.background = "#ffd23f";
  rot.appendChild(ptr);
  cap.append(capin, rot);
  skirt.appendChild(cap);
  row.appendChild(skirt);

  const bar = el("div", "mbar");
  const fill = el("div", "mfill");
  bar.appendChild(fill);
  row.appendChild(bar);

  const drawAmt = () => {
    const v = amtState.getNormalisedValue();      // 0..1 norm, .5 = amt 0
    const amt = v - .5;
    rot.style.transform = "rotate(" + (-135 + v * 270) + "deg)";
    fill.style.width = Math.round(Math.abs(amt) * 2 * 100) + "%";
    fill.style.background = amt < 0 ? "#ff2b3e" : "#ffd23f";
  };
  amtState.valueChangedEvent.addListener(drawAmt);
  amtState.propertiesChangedEvent.addListener(drawAmt);
  drawAmt();
  bindVDrag(skirt, () => amtState, () => "AMT " + n, drawAmt,
    { fmt: "bip", quant: (v) => (Math.abs(v - .5) < .015 ? .5 : v) });   // ±.03 amt detent

  $("mtxRows").appendChild(row);
}

// GLOBAL LFO: RATE knob + wave cycle button
makeKnob($("glfoRateMount"), { states: [sliderState("lfo_rate")], label: "RATE", size: 30, touch: "G-LFO RATE" });
const glfoWaveState = comboState("lfo_shape");
comboDraw([glfoWaveState], false, () => {
  $("glfoWaveBtn").textContent = GLFOWAVES[idx(glfoWaveState, GLFOWAVES.length)];
});
$("glfoWaveBtn").addEventListener("click", () =>
  glfoWaveState.setChoiceIndex((idx(glfoWaveState, GLFOWAVES.length) + 1) % GLFOWAVES.length));

//==============================================================================
// FX: MOD / DELAY / REVERB rows (type button opens menu, 3 knobs, LED+on).
function fxRow(name, typeId, typeArr, menuTitle, knobDefs, onId, divider) {
  const row = el("div", "fxrow");
  const nm = el("div", "fxname"); nm.textContent = name;
  const typeBtn = el("div", "cbtn"); typeBtn.style.width = "56px";
  row.append(nm, typeBtn);

  const typeState = comboState(typeId);
  comboDraw([typeState], false, () => {
    typeBtn.textContent = typeArr[idx(typeState, typeArr.length)];
  });
  typeBtn.addEventListener("click", () =>
    openMenu(menuTitle, rowsOf(typeArr), idx(typeState, typeArr.length),
      (i) => typeState.setChoiceIndex(i)));

  knobDefs.forEach(([id, lbl]) => {
    makeKnob(row, { states: [sliderState(id)], label: lbl, size: 32, touch: name + " " + lbl });
  });

  const right = el("div", "fxright");
  const led = el("div", "led");
  const onBtn = el("div", "pbtn tog");
  right.append(led, onBtn);
  row.appendChild(right);
  const onState = toggleState(onId);
  const drawOn = () => led.classList.toggle("on", !!onState.getValue());
  onState.valueChangedEvent.addListener(drawOn);
  drawOn();
  onBtn.addEventListener("click", () => onState.setValue(!onState.getValue()));

  $("fxRows").appendChild(row);
  if (divider) $("fxRows").appendChild(el("div", "fxdiv"));
}
fxRow("MOD", "modfx_type", MODFX, "MOD FX TYPE",
  [["modfx_rate", "RATE"], ["modfx_depth", "DEPTH"], ["modfx_mix", "MIX"]], "modfx_on", true);
fxRow("DELAY", "dly_mode", DLYMODES, "DELAY MODE",
  [["dly_time", "TIME"], ["dly_fb", "FB"], ["dly_mix", "MIX"]], "dly_on", true);
fxRow("REVERB", "rev_type", REVTYPES, "REVERB TYPE",
  [["rev_size", "SIZE"], ["rev_damp", "DAMP"], ["rev_mix", "MIX"]], "rev_on", false);

//==============================================================================
// Tone selection (UI-only): rebinds the TONE EDIT group + LCD tone label.
function select(i) {
  SEL.i = i;
  $("editToneName").textContent = TONES[i];
  $("lcdTone").textContent = "TONE " + TONES[i];
  selLeds.forEach((l, j) => l.classList.toggle("on", j === i));
  rebinds.forEach((f) => f());
  drawGlyph();
}
select(0);
setTouched("MORPH", .52);   // boot readout, matches the handoff PNG

//==============================================================================
// Version/build stamp via the getInfo native function (guarded: in the
// standalone mock the promise never resolves and the stamp stays empty).
let infoPromise = null;
try { infoPromise = Juce.getNativeFunction("getInfo")(); } catch (e) {}
if (infoPromise)
  infoPromise.then((info) => {
    if (info) $("verStamp").textContent = "v" + info.version + " " + info.build;
  }).catch(() => {});

// Tooling flags (mock/dev only, house pattern like rubber-rhino's ?about;
// never triggered inside the plugin):
//   ?menu  boots with the wave menu open (headless overlay screenshot)
//   ?penv  boots with the P-ENV mini page open
//   ?ens   demo state: tone A on an [ENS] loop + MOD FX on ENSEMBLE
if (location.search.indexOf("ens") >= 0) {
  waveStates[0].setChoiceIndex(82);              // Ens: SOLINA_DREAM
  comboState("modfx_type").setChoiceIndex(3);    // ENSEMBLE
}
if (location.search.indexOf("menu") >= 0)
  $("waveLcd").dispatchEvent(new MouseEvent("click"));
if (location.search.indexOf("penv") >= 0)
  openPenv();

//==============================================================================
// Uniform scaling of the fixed 1140x660 canvas (no scrollbars, centered).
const BASE_W = 1140, BASE_H = 660;
function fit() {
  const s = Math.min(window.innerWidth / BASE_W, window.innerHeight / BASE_H);
  const f = $("frame");
  f.style.transform = "scale(" + s + ")";
  f.style.left = Math.max(0, (window.innerWidth - BASE_W * s) / 2) + "px";
  f.style.top = Math.max(0, (window.innerHeight - BASE_H * s) / 2) + "px";
}
window.addEventListener("resize", fit);
fit();
