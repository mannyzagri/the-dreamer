// The Dreamer -- GUI v5 production logic (JUCE 8 WebView2 editor).
// Binds every panel control to the JUCE WebView relays by APVTS ID
// (see plugin/Params.h -- IDs are LOCKED). House pattern: rubber-rhino gui.
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
// list is design-handoff/gui-v5/wave-list-bank-order.js VERBATIM (78, bank
// order); display uppercases at render time.
const WAVES = [
  { cat: "Basic", name: "Saw" },          { cat: "Basic", name: "Square" },
  { cat: "Basic", name: "Triangle" },     { cat: "Basic", name: "Sine" },
  { cat: "Pad", name: "SoftSaw 1" },      { cat: "Pad", name: "SoftSaw 2" },
  { cat: "Pad", name: "SoftSaw 3" },      { cat: "Pad", name: "AsymSaw 1" },
  { cat: "Pad", name: "AsymSaw 2" },      { cat: "Pad", name: "Hollow 1" },
  { cat: "Pad", name: "Hollow 2" },       { cat: "Pad", name: "Hollow 3" },
  { cat: "Pad", name: "Hollow 4" },       { cat: "Pad", name: "Tannerin" },
  { cat: "Pad", name: "Breath 1" },       { cat: "Pad", name: "Breath 2" },
  { cat: "Pad", name: "Breath 3" },       { cat: "Pad", name: "Glass Organ 1" },
  { cat: "Pad", name: "Glass Organ 2" },  { cat: "Pad", name: "Glass Organ 3" },
  { cat: "Pad", name: "SinHarm 1" },      { cat: "Pad", name: "SinHarm 2" },
  { cat: "String", name: "StringBox 1" }, { cat: "String", name: "StringBox 2" },
  { cat: "String", name: "StringBox 3" }, { cat: "String", name: "Violin 1" },
  { cat: "String", name: "Violin 2" },    { cat: "String", name: "Cello 1" },
  { cat: "String", name: "Cello 2" },     { cat: "String", name: "Cello 3" },
  { cat: "Vox", name: "Voice 1" },        { cat: "Vox", name: "Voice 2" },
  { cat: "Vox", name: "Voice 3" },        { cat: "Vox", name: "Voice 4" },
  { cat: "Vox", name: "Voice 5" },        { cat: "Vox", name: "Voice Bright 1" },
  { cat: "Vox", name: "Voice Bright 2" }, { cat: "Bell", name: "FM Bell 1" },
  { cat: "Bell", name: "FM Bell 2" },     { cat: "Bell", name: "FM Bell 3" },
  { cat: "Bell", name: "FM Bell 4" },     { cat: "Bell", name: "FM Bell 5" },
  { cat: "Bell", name: "FM Bell 6" },     { cat: "Bell", name: "Tine Bright" },
  { cat: "Bell", name: "EP Tine 1" },     { cat: "Bell", name: "EP Tine 2" },
  { cat: "Bell", name: "DigiBell 1" },    { cat: "Bell", name: "DigiBell 2" },
  { cat: "Bell", name: "DigiBell 3" },    { cat: "Bell", name: "DigiBell 4" },
  { cat: "Bell", name: "DigiBell 5" },    { cat: "Bell", name: "Chime 1" },
  { cat: "Bell", name: "Chime 2" },       { cat: "Bell", name: "Chime 3" },
  { cat: "Bell", name: "Hollow Bell 1" }, { cat: "Bell", name: "Hollow Bell 2" },
  { cat: "Bell", name: "Hollow Bell 3" }, { cat: "Metal", name: "Spectrum 1" },
  { cat: "Metal", name: "Spectrum 2" },   { cat: "Metal", name: "Spectrum 3" },
  { cat: "Metal", name: "Spectrum 4" },   { cat: "Metal", name: "Spectrum 5" },
  { cat: "Metal", name: "Spectrum 6" },   { cat: "Metal", name: "Spectrum 7" },
  { cat: "Metal", name: "RawMetal 1" },   { cat: "Metal", name: "RawMetal 2" },
  { cat: "Metal", name: "RawMetal 3" },   { cat: "Metal", name: "RawMetal 4" },
  { cat: "Metal", name: "Clang 1" },      { cat: "Metal", name: "Clang 2" },
  { cat: "Metal", name: "Clang 3" },      { cat: "Metal", name: "BitHash 1" },
  { cat: "Metal", name: "BitHash 2" },    { cat: "Metal", name: "BitHash 3" },
  { cat: "Metal", name: "BitHash 4" },    { cat: "Metal", name: "GrainMetal 1" },
  { cat: "Metal", name: "GrainMetal 2" }, { cat: "Metal", name: "GrainMetal 3" },
];
const SHAPES    = ["OFF", "SOFT FOLD", "HARD FOLD", "SINE FOLD", "ASYM", "DRIVE"];
const TVFTYPES  = ["LP24", "LP12", "BP", "HP"];              // LCD short form
const TVFFULL   = ["LP 24", "LP 12", "BP", "HP"];            // menu rows
const FTYPES    = ["LP 24", "LP 12", "BP", "HP", "LIQUID", "CLASSIC", "LADDER",
                   "NOTCH", "COMB +", "COMB -", "NL3 N+LP", "FORMANT", "ALLPASS", "DREAMPLN"];
const GLFOWAVES = ["TRI", "SIN", "SAW", "SQR", "S+H"];
const AUXDESTS  = ["PITCH", "START", "SHAPE", "PAN"];
const LFODESTS  = ["PITCH", "CUTOFF", "SHAPE", "LEVEL"];
const MSRC      = ["G-LFO", "VEC PHS", "AUX", "VELO", "WHEEL"];
const MDST      = ["PITCH", "CUT 1", "CUT 2", "MORPH", "SHAPE", "VEC PHS", "PAN"];
const MODFX     = ["CHORUS", "FLANGER", "PHASER"];
const DLYMODES  = ["DIGITAL", "TAPE", "PONG"];
const REVTYPES  = ["ROOM", "HALL", "PLATE"];
const TONES     = ["A", "B", "C", "D"];
const PX        = ["a_", "b_", "c_", "d_"];

//==============================================================================
// Standalone-mock demo defaults (normalised sliders / bools / choice indices)
// -- cosmetics only, mirrors the prototype's boot state / handoff PNG. The
// real plugin never reads these (relay initial-update wins).
const MOCK = { sliders: {}, toggles: {}, combos: {} };
{
  const lv = [.8, .62, .5, .4], st = [.1, .2, .05, .15];
  const ct = [.5, .6, .45, .55], wv = [11, 18, 28, 37];
  PX.forEach((px, i) => {
    Object.assign(MOCK.sliders, {
      [px + "coarse"]: .5, [px + "fine"]: .5, [px + "level"]: lv[i],
      [px + "velsens"]: .4, [px + "start"]: st[i], [px + "pan"]: .5,
      [px + "dir"]: i * .25, [px + "int"]: .6, [px + "shape_depth"]: 0,
      [px + "cutoff"]: ct[i], [px + "reso"]: .25, [px + "env_amt"]: .5,
      [px + "keyfollow"]: .3,
      [px + "tvf_att"]: .05, [px + "tvf_dec"]: .6, [px + "tvf_sus"]: .5, [px + "tvf_rel"]: .55,
      [px + "tva_att"]: .2,  [px + "tva_dec"]: .7, [px + "tva_sus"]: .8, [px + "tva_rel"]: .65,
      [px + "aux_att"]: 0,   [px + "aux_dec"]: .4, [px + "aux_sus"]: 0,  [px + "aux_rel"]: .3,
      [px + "aux_amt"]: .5,  [px + "lfo_depth"]: .15,
    });
    MOCK.toggles[px + "on"] = true;
    Object.assign(MOCK.combos, {
      [px + "wave"]: wv[i], [px + "shape_type"]: 0, [px + "tvf_type"]: 0,
      [px + "aux_dest"]: 0, [px + "lfo_dest"]: 0,
    });
  });
  Object.assign(MOCK.sliders, {
    vec_phase: .12, orbit_rate: .3, penv_start: 0, penv_end: .5, penv_time: .5,
    glfo_rate: .4, mod1_amt: .6, mod2_amt: .35, mod3_amt: .2,
    f1_cutoff: .52, f1_reso: .35, f1_env: .5,
    f2_cutoff: .7, f2_reso: .2, f2_morph: .4,
    modfx_rate: .3, modfx_depth: .5, modfx_mix: .45,
    delay_time: .5, delay_fb: .35, delay_mix: .3,
    rev_size: .6, rev_damp: .4, rev_mix: .35, volume: .78,
  });
  Object.assign(MOCK.toggles, {
    orbit_on: true, penv_on: false, penv_loop: false,
    modfx_on: true, delay_on: true, rev_on: true,
  });
  Object.assign(MOCK.combos, {
    glfo_shape: 0, mod1_src: 0, mod2_src: 1, mod3_src: 2,
    mod1_dest: 3, mod2_dest: 4, mod3_dest: 0,
    f1_type: 6, f2_type: 13, filt_routing: 0,
    modfx_type: 0, delay_mode: 0, rev_type: 1,
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

// Per-tone state array (a_/b_/c_/d_ prefixes).
const tSliders = (sfx) => PX.map((px) => sliderState(px + sfx));
const tToggles = (sfx) => PX.map((px) => toggleState(px + sfx));
const tCombos  = (sfx) => PX.map((px) => comboState(px + sfx));

//==============================================================================
// UI-only state: tone selection + redraw hooks run when it changes.
const SEL = { i: 0 };
const rebinds = [];

//==============================================================================
// Main-LCD touched line: name + 0-127 coarse value + 14-cell meter.
const meterCells = [];
for (let i = 0; i < 14; i++) meterCells.push($("lcdMeter").appendChild(el("span")));
function setTouched(label, norm) {
  $("lcdTouched").textContent = label + " " + String(Math.round(norm * 127)).padStart(3, " ");
  const fill = Math.round(norm * 14);
  meterCells.forEach((c, i) => { c.style.background = i < fill ? "#ffd23f" : "#1c1a10"; });
}

//==============================================================================
// Drag law: vertical, 140px full range, pointer capture, no easing.
function bindVDrag(elm, stFn, labelFn, after) {
  elm.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    const s = stFn();
    s.sliderDragStarted();
    const y0 = e.clientY, n0 = s.getNormalisedValue();
    try { elm.setPointerCapture(e.pointerId); } catch (err) {}
    const mv = (ev) => {
      const v = clamp01(n0 + (y0 - ev.clientY) / 140);
      s.setNormalisedValue(v);
      setTouched(labelFn(), v);
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
  bindVDrag(skirt, st, labelFn, draw);
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
function openMenu(title, rows, curI, pick) {   // rows: [{cat, name}]
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
    row.addEventListener("click", () => { pick(i); closeMenu(); });
    list.appendChild(row);
  });
  $("menuOverlay").style.display = "flex";
  const cur = list.children[curI];
  if (cur) cur.scrollIntoView({ block: "center" });
}
function closeMenu() { menuOpen = false; $("menuOverlay").style.display = "none"; }
$("menuOverlay").addEventListener("click", (e) => { if (e.target === $("menuOverlay")) closeMenu(); });
window.addEventListener("keydown", (e) => { if (e.key === "Escape") closeMenu(); });
const rowsOf = (arr) => arr.map((n) => ({ cat: "", name: n }));
const waveRows = () => WAVES.map((w) => ({ cat: w.cat.toUpperCase(), name: w.name.toUpperCase() }));

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
// Header: wave glyph, MASTER knob (binds `volume`), power LED is static.
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
makeKnob($("masterMount"), { states: [sliderState("volume")], label: "MASTER", size: 40, touch: "MASTER" });

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

    makeVSlider(col, { states: [sliderState(PX[i] + "level")], h: 150, touch: () => "LEVEL " + name });

    const onLed = el("div", "led"); onLed.style.marginTop = "2px";
    const onBtn = el("div", "pbtn tbtn");
    const onLbl = el("div", "tonlbl"); onLbl.textContent = "ON";
    col.append(onLed, onBtn, onLbl);

    const onState = toggleState(PX[i] + "on");
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
const shapeStates = tCombos("shape_type");
comboDraw(shapeStates, true, () => {
  $("shapeLcd").textContent = SHAPES[idx(shapeStates[SEL.i], SHAPES.length)];
});
bindStep($("shapeDec"), $("shapeInc"), () => shapeStates[SEL.i], SHAPES.length);
$("shapeLcd").addEventListener("click", () =>
  openMenu("SHAPER — TONE " + letter(), rowsOf(SHAPES),
    idx(shapeStates[SEL.i], SHAPES.length), (i) => shapeStates[SEL.i].setChoiceIndex(i)));

// 7-knob row: OCTV FINE START LEVEL PAN VELO SHP-DPT (Ø36, SHP DPT yellow)
[["coarse", "OCTV"], ["fine", "FINE"], ["start", "START"], ["level", "LEVEL"],
 ["pan", "PAN"], ["velsens", "VELO"], ["shape_depth", "SHP DPT"]].forEach(([sfx, lbl]) => {
  makeKnob($("teKnobs"), {
    states: tSliders(sfx), cur: curTone, label: lbl, size: 36,
    ptr: sfx === "shape_depth" ? "#ffd23f" : null, touch: lbl,
  });
});

// TVF: TYPE LCD + 4 red knobs + three ADSR banks
const tvfTypeStates = tCombos("tvf_type");
comboDraw(tvfTypeStates, true, () => {
  $("tvfTypeLcd").textContent = TVFTYPES[idx(tvfTypeStates[SEL.i], TVFTYPES.length)];
});
$("tvfTypeLcd").addEventListener("click", () =>
  openMenu("TVF TYPE — TONE " + letter(), rowsOf(TVFFULL),
    idx(tvfTypeStates[SEL.i], TVFTYPES.length), (i) => tvfTypeStates[SEL.i].setChoiceIndex(i)));

[["cutoff", "CUT"], ["reso", "RES"], ["env_amt", "ENV"], ["keyfollow", "KF"]].forEach(([sfx, lbl]) => {
  makeKnob($("tvfKnobs"), {
    states: tSliders(sfx), cur: curTone, label: lbl, size: 34, ptr: "#ff5b6e", touch: "TVF " + lbl,
  });
});

function makeBank(mountId, prefix, title) {
  const bank = $(mountId);
  const row = el("div", "bankrow");
  bank.appendChild(row);
  [["att", "A"], ["dec", "D"], ["sus", "S"], ["rel", "R"]].forEach(([sfx, l]) => {
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
// AUX dest button (cycles PITCH/START/SHAPE/PAN) beside the AUX ENV title
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
const dirStates = tSliders("dir"), intStates = tSliders("int");
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
filterStrip("f1Lcd", "f1Dec", "f1Inc", "f1_type",
  [["f1_cutoff", "CUT", "#ff5b6e"], ["f1_reso", "RES", "#ff5b6e"], ["f1_env", "ENV", "#ff5b6e"]],
  "f1Knobs", "FILTER 1 TYPE");
filterStrip("f2Lcd", "f2Dec", "f2Inc", "f2_type",
  [["f2_cutoff", "CUT", "#ff5b6e"], ["f2_reso", "RES", "#ff5b6e"], ["f2_morph", "MORPH", "#ffd23f"]],
  "f2Knobs", "FILTER 2 TYPE");

const routeState = comboState("filt_routing");
comboDraw([routeState], false, () => {
  const r = idx(routeState, 2);
  $("serLed").classList.toggle("on", r === 0);
  $("parLed").classList.toggle("on", r === 1);
});
$("routeBtn").addEventListener("click", () => routeState.setChoiceIndex(1 - idx(routeState, 2)));

//==============================================================================
// DREAM VECTOR: PHASE/RATE knobs, ORBIT/P-ENV toggles, signal-flow display.
const vecPhaseState = sliderState("vec_phase");
const orbitRateState = sliderState("orbit_rate");
const orbitState = toggleState("orbit_on");
const penvState = toggleState("penv_on");
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

// Params with no v5 panel control stay wired (hidden states; report note).
["penv_start", "penv_end", "penv_time"].forEach(sliderState);
toggleState("penv_loop");
PX.forEach((px) => sliderState(px + "aux_amt"));

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
  if (menuOpen) return;
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
// MOD MATRIX: 3 slots (SRC > DST, Ø26 yellow AMT knob, amount bar) + G-LFO.
for (let n = 1; n <= 3; n++) {
  const row = el("div", "mrowline");
  const num = el("div", "mnumcol"); num.textContent = String(n);
  const srcBtn = el("div", "cbtn"); srcBtn.style.width = "52px";
  const arrow = el("div", "marrow"); arrow.textContent = ">";
  const dstBtn = el("div", "cbtn"); dstBtn.style.width = "52px";
  row.append(num, srcBtn, arrow, dstBtn);

  const srcState = comboState("mod" + n + "_src");
  const dstState = comboState("mod" + n + "_dest");
  const amtState = sliderState("mod" + n + "_amt");
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
    const v = amtState.getNormalisedValue();
    rot.style.transform = "rotate(" + (-135 + v * 270) + "deg)";
    fill.style.width = Math.round(v * 100) + "%";
  };
  amtState.valueChangedEvent.addListener(drawAmt);
  amtState.propertiesChangedEvent.addListener(drawAmt);
  drawAmt();
  bindVDrag(skirt, () => amtState, () => "AMT " + n, drawAmt);

  $("mtxRows").appendChild(row);
}

// GLOBAL LFO: RATE knob + wave cycle button
makeKnob($("glfoRateMount"), { states: [sliderState("glfo_rate")], label: "RATE", size: 30, touch: "G-LFO RATE" });
const glfoWaveState = comboState("glfo_shape");
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
fxRow("DELAY", "delay_mode", DLYMODES, "DELAY MODE",
  [["delay_time", "TIME"], ["delay_fb", "FB"], ["delay_mix", "MIX"]], "delay_on", true);
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

// ?menu: tooling flag (mock/dev only, house pattern like rubber-rhino's
// ?about) -- boots with the wave menu open so headless Chrome can
// screenshot the overlay. Never triggered inside the plugin.
if (location.search.indexOf("menu") >= 0)
  $("waveLcd").dispatchEvent(new MouseEvent("click"));

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
