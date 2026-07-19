// The Dreamer -- WebView editor logic, REGENERATED from design-handoff/v13
// "The Dreamer GUI" master (interactive prototype). Every control's geometry,
// color, typography and behavior is reproduced verbatim from that master; the
// only things carried from the prior editor are the JUCE bridge plumbing (the
// import below, window.__JUCE__.initialisationData, the get*State relay
// factories, the getInfo/keyboardFold/noteOn.. native functions and the
// window.uiMeters feed) and the bank3-ordered wave list (a DSP-contract datum,
// not layout). Layout is LOCKED to the master: fixed absolute/flex geometry,
// no responsive recompute; the sole transform is the uniform window-derived
// scale in fit().
//
// Parameter wiring is 1:1 with the APVTS relays declared in
// plugin/PluginEditor.cpp (makeSliderIds / makeToggleIds / makeComboIds). The
// id lists below are constructed with the SAME per-tone-suffix + global loop
// structure, so the set requested here equals the set the C++ registers.
import * as Juce from "./index.js";

const $ = (id) => document.getElementById(id);
const NS = "http://www.w3.org/2000/svg";
const clamp01 = (v) => Math.max(0, Math.min(1, v));
const initData = window.__JUCE__.initialisationData;
const HAS_BACKEND = (initData.__juce__sliders || []).length > 0;
const MOCKMODE = !HAS_BACKEND;

// tiny DOM helpers (cssText = the master's inline style string, verbatim)
function mk(tag, css, txt) { const d = document.createElement(tag); if (css) d.style.cssText = css; if (txt != null) d.textContent = txt; return d; }
function mkNS(tag) { return document.createElementNS(NS, tag); }
function setAttrs(e, o) { for (const k in o) e.setAttribute(k, o[k]); }

//============================================================================
// Choice lists -- order = APVTS choice index (LOCKED, plugin/Params.h). Shown
// upper-case on the silkscreen/LCD like the master. These are used for display
// + wrap math; selection always goes through the combo relay by index.
const FTYPES   = ['LP 24','LP 12','BP','HP','LIQUID','CLASSIC','LADDER','NOTCH','COMB +','COMB -','N+LP','FORMANT','ALLPASS','DREAMPLN'];
const SHAPES   = ['OFF','SOFT FOLD','HARD FOLD','SINE FOLD','ASYM','DRIVE'];
const TVFTYPES = ['LP24','LP12','BP','HP'];
const AUXDESTS = ['PITCH','START','SHAPE','PAN','NOISE'];
const LFODESTS = ['PITCH','CUTOFF','SHAPE','LEVEL'];
const LFOSHAPES= ['TRI','SIN','SAW','SQR','S+H'];            // lfoN_shape, lfo_shape (Params lfoShapes)
const ORBITSHAPES = ['SAW','TRI','SIN','SQR','S+H'];          // vec_orbit_shape (Params orbitShapes)
const MSRC     = ['G-LFO','VEC PHS','AUX','VELO','WHEEL'];
const MDST     = ['PITCH','CUT 1','CUT 2','MORPH','SHAPE','VEC PHS','PAN','NOISE','FX PARAM'];
const VOICINGS = ['SINGLE','OCT','POWER','DREAMY'];
const SPREADS  = ['ADD9','MIN7','SUS2'];
const LOOPMODES= ['FWD','PINGPONG'];
const PLAYMODES= ['NORMAL','STRETCH'];
const MODFX    = ['CHORUS','FLANGER','PHASER','ENSEMBLE','DIMENSION','ROTARY','BARBERPOLE'];
const DLYMODES = ['DIGITAL','TAPE','PONG'];
const REVTYPES = ['ROOM','HALL','PLATE'];
const PREPOST  = ['POST','PRE'];                             // fx_prepost (Params: Post, Pre)
// per-slot secondary-parameter (PARAMS FOCUS) display names -- the pfocus combo
// itself has 4 choices (P1..P4); these are the human labels shown for each.
const MODFXFOCUS = ['DELAY','WIDTH','FEEDBK','TONE'];
const DLYFOCUS   = ['WOW','FLUTTER','TONE','DUCK'];
const REVFOCUS   = ['PREDLY','WIDTH','LO CUT','HI CUT'];
const LOFIFOCUS  = ['BITS','SRATE','COMPAND','ALIAS'];       // UI-local focus -> lofi_bits/srate/compand + lofi_alias(toggle)
const TALKFOCUS  = ['VOWEL-A','VOWEL-B','MORPH','SENS'];     // UI-local focus -> talk_va/vb/morph/sens
const SYNCDIVS   = ['4/1','2/1','1/1','1/2','1/2T','1/4','1/4T','1/8','1/8T','1/16','1/16T','1/32'];
const TONELET    = ['A','B','C','D'];
// preset browser. The factory bank is PROCESSOR-OWNED (parsed from the embedded
// presets.json); on boot loadPresetList() replaces this array with the names/
// categories fetched via the getPresetList native fn, and selecting a preset
// calls the loadPreset native fn so the processor performs the APVTS recall.
// The array below is only the offline/mock fallback (no JUCE backend).
let PRESETS = [
  ['PAD','ETHEREAL DAWN'],['PAD','SLOW MEMORY'],['PAD','GHOST HARBOR'],['PAD','VIOLET SLEEP'],
  ['PAD','TIDE GARDEN'],['PAD','HALF LIGHT'],['SPLIT','GLASS RIVER'],['SPLIT','NIGHT DRIVE 84'],
  ['BELL','TINE CATHEDRAL'],['BELL','MUSICBOX MOON'],['BELL','COLD CHIMES'],
  ['STR','ORBITAL STRINGS'],['STR','SOLINA FIELDS'],['VOX','CHOIR OF WIRES'],['VOX','BREATH MACHINE'],
  ['BASS','RUBBER ORBIT'],['BASS','SUB DREAMS'],['LEAD','SYNC COMET'],['LEAD','12-BIT STAR'],
  ['KEY','EPIANO HAZE'],['KEY','ORGAN NEBULA'],['SFX','RE-ENTRY'],['SFX','STATIC BLOOM'],['INIT','INIT PATCH']
];

// Wave ROM (218 = bank3 order: 78 cycle "", 130 loop "ENS", 10 hit "SHOT").
// The 78 cycle names are the real bank3 names; the loop/hit entries carry the
// tag that drives the LOOP/PLAY selector swap. In-plugin the wave display could
// also come from the relay's choices, but the tag is not in that string, so the
// index-aligned table below is authoritative for the swap + menu grouping.
const WAVE_CYCLE = [
  ['Basic','Saw'],['Basic','Square'],['Basic','Triangle'],['Basic','Sine'],
  ['Pad','SoftSaw 1'],['Pad','SoftSaw 2'],['Pad','SoftSaw 3'],['Pad','AsymSaw 1'],['Pad','AsymSaw 2'],
  ['Pad','Hollow 1'],['Pad','Hollow 2'],['Pad','Hollow 3'],['Pad','Hollow 4'],['Pad','Tannerin'],
  ['Pad','Breath 1'],['Pad','Breath 2'],['Pad','Breath 3'],['Pad','Glass Organ 1'],['Pad','Glass Organ 2'],
  ['Pad','Glass Organ 3'],['Pad','SinHarm 1'],['Pad','SinHarm 2'],
  ['String','StringBox 1'],['String','StringBox 2'],['String','StringBox 3'],['String','Violin 1'],['String','Violin 2'],
  ['String','Cello 1'],['String','Cello 2'],['String','Cello 3'],
  ['Vox','Voice 1'],['Vox','Voice 2'],['Vox','Voice 3'],['Vox','Voice 4'],['Vox','Voice 5'],
  ['Vox','Voice Bright 1'],['Vox','Voice Bright 2'],
  ['Bell','FM Bell 1'],['Bell','FM Bell 2'],['Bell','FM Bell 3'],['Bell','FM Bell 4'],['Bell','FM Bell 5'],
  ['Bell','FM Bell 6'],['Bell','Tine Bright'],['Bell','EP Tine 1'],['Bell','EP Tine 2'],['Bell','DigiBell 1'],
  ['Bell','DigiBell 2'],['Bell','DigiBell 3'],['Bell','DigiBell 4'],['Bell','DigiBell 5'],['Bell','Chime 1'],
  ['Bell','Chime 2'],['Bell','Chime 3'],['Bell','Hollow Bell 1'],['Bell','Hollow Bell 2'],['Bell','Hollow Bell 3'],
  ['Metal','Spectrum 1'],['Metal','Spectrum 2'],['Metal','Spectrum 3'],['Metal','Spectrum 4'],['Metal','Spectrum 5'],
  ['Metal','Spectrum 6'],['Metal','Spectrum 7'],['Metal','RawMetal 1'],['Metal','RawMetal 2'],['Metal','RawMetal 3'],
  ['Metal','RawMetal 4'],['Metal','Clang 1'],['Metal','Clang 2'],['Metal','Clang 3'],['Metal','BitHash 1'],
  ['Metal','BitHash 2'],['Metal','BitHash 3'],['Metal','BitHash 4'],['Metal','GrainMetal 1'],['Metal','GrainMetal 2'],
  ['Metal','GrainMetal 3']
];
const WAVES = [];
WAVE_CYCLE.forEach(([c, n]) => WAVES.push({ cat: c, name: n, tag: '' }));
for (let i = 0; i < 130; i++) WAVES.push({ cat: 'Loop', name: 'Loop ' + String(i + 1).padStart(3, '0'), tag: 'ENS' });
for (let i = 0; i < 10;  i++) WAVES.push({ cat: 'Hit',  name: 'Hit '  + String(i + 1).padStart(2, '0'), tag: 'SHOT' });

//============================================================================
// Relay id lists -- MIRROR of plugin/PluginEditor.cpp makeSliderIds /
// makeToggleIds / makeComboIds (same 4 per-tone suffixes _a.._d + globals).
const SFX = ['_a', '_b', '_c', '_d'];
const SLIDER_PERTONE = ['level','oct','fine','start','velo','pan','shape_depth','noise','noise_color',
  'dir','vint','aux_amt','lfo1_rate','lfo1_depth','lfo2_rate','lfo2_depth',
  'tvf_cut','tvf_res','tvf_env','tvf_kf','tvf_a','tvf_d','tvf_s','tvf_r',
  'tva_a','tva_d','tva_s','tva_r','aux_a','aux_d','aux_s','aux_r','hit_stretch','hit_pitchtrim'];
const SLIDER_GLOBAL = ['master','vec_phase','vec_orbit_rate','vec_penv_start','vec_penv_end','vec_penv_time',
  'lfo_rate','mtx1_amt','mtx2_amt','mtx3_amt','flt1_cut','flt1_res','flt1_env','flt2_cut','flt2_res','flt2_morph','flt_bal',
  'modfx_rate','modfx_depth','modfx_mix','modfx_p0','modfx_p1','modfx_p2','modfx_p3',
  'dly_time','dly_fb','dly_mix','dly_p0','dly_p1','dly_p2','dly_p3',
  'rev_size','rev_damp','rev_mix','rev_p0','rev_p1','rev_p2','rev_p3',
  'lofi_bits','lofi_srate','lofi_compand','width','width_haas','talk_va','talk_vb','talk_morph','talk_sens','drift'];
const TOGGLE_PERTONE = ['on','start_random','lfo1_sync','lfo2_sync'];
const TOGGLE_GLOBAL  = ['vec_orbit_on','vec_orbit_voice','vec_penv_on','vec_penv_loop',
  'modfx_on','dly_on','dly_sync','rev_on','lofi_on','lofi_alias','width_on','width_bassmono','talk_on'];
const COMBO_PERTONE  = ['wave','shape','tvf_type','aux_dest','lfo1_dest','lfo2_dest','lfo1_shape','lfo2_shape',
  'voicing','dreamy_spread','loop_mode','hit_play'];
const COMBO_GLOBAL   = ['vec_orbit_shape','lfo_shape','mtx1_src','mtx2_src','mtx3_src','mtx1_dst','mtx2_dst','mtx3_dst',
  'flt_route','flt1_type','flt2_type','modfx_type','dly_mode','rev_type',
  'modfx_pfocus','dly_pfocus','rev_pfocus','fx_prepost','interp','engine'];
function buildIds(pertone, global) { const a = []; for (const sx of SFX) for (const b of pertone) a.push(b + sx); for (const g of global) a.push(g); return a; }
const SLIDER_IDS = buildIds(SLIDER_PERTONE, SLIDER_GLOBAL);   // 184
const TOGGLE_IDS = buildIds(TOGGLE_PERTONE, TOGGLE_GLOBAL);   // 29
const COMBO_IDS  = buildIds(COMBO_PERTONE, COMBO_GLOBAL);     // 68

//============================================================================
// Standalone-mock seed (only used when there is no JUCE backend, i.e. plain
// browser / headless verification). Values mirror the master's default patch
// so the offline render resembles the handoff PNG. Unseeded sliders default to
// .5 (center) -- correct for bipolar, a neutral mid for the rest.
const MOCK = { sliders: {}, toggles: {}, combos: {} };
(function seed() {
  const S = MOCK.sliders, T = MOCK.toggles, C = MOCK.combos;
  // tone A full edit state
  Object.assign(S, {
    level_a:.8, oct_a:.5, fine_a:.52, start_a:.1, velo_a:.4, pan_a:.5, shape_depth_a:0, noise_a:0, noise_color_a:.5,
    dir_a:0, vint_a:.6, aux_amt_a:.5, lfo1_rate_a:.4, lfo1_depth_a:.15, lfo2_rate_a:.25, lfo2_depth_a:0,
    tvf_cut_a:.5, tvf_res_a:.25, tvf_env_a:.5, tvf_kf_a:.3, tvf_a_a:.05, tvf_d_a:.6, tvf_s_a:.5, tvf_r_a:.55,
    tva_a_a:.2, tva_d_a:.7, tva_s_a:.8, tva_r_a:.65, aux_a_a:0, aux_d_a:.4, aux_s_a:0, aux_r_a:.3, hit_stretch_a:.5, hit_pitchtrim_a:.5,
    level_b:.62, level_c:.5, level_d:.4, dir_b:.25, dir_c:.5, dir_d:.75, vint_b:.6, vint_c:.6, vint_d:.6,
    master:.78, vec_phase:.12, vec_orbit_rate:.3, vec_penv_start:0, vec_penv_end:.5, vec_penv_time:.4, lfo_rate:.4,
    mtx1_amt:.9, mtx2_amt:.18, mtx3_amt:.5,
    flt1_cut:.52, flt1_res:.35, flt1_env:.5, flt2_cut:.7, flt2_res:.2, flt2_morph:.4, flt_bal:.5,
    modfx_rate:.3, modfx_depth:.5, modfx_mix:.45, dly_time:.5, dly_fb:.35, dly_mix:.3, rev_size:.6, rev_damp:.4, rev_mix:.35,
    width:.55, width_haas:.3,
  });
  Object.assign(T, { on_a:true, on_b:true, on_c:true, on_d:true, modfx_on:true, dly_on:true, rev_on:true, width_on:true, vec_orbit_on:true });
  Object.assign(C, {
    wave_a:11, wave_b:19, wave_c:27, wave_d:37, lfo1_dest_a:0, lfo2_dest_a:1, lfo1_shape_a:0, lfo2_shape_a:2,
    flt1_type:6, flt2_type:13, vec_orbit_shape:2, lfo_shape:0, modfx_type:0, dly_mode:0, rev_type:1,
    mtx1_src:0, mtx1_dst:3, mtx2_src:1, mtx2_dst:4, mtx3_src:2, mtx3_dst:0,
  });
})();

//============================================================================
// Bridge plumbing: real-or-mock relay state factories (house pattern). If an id
// is registered with the backend, use the JUCE state; else a local stand-in
// exposing the same interface, seeded from MOCK so the page renders offline.
function mockEvents() { const l = []; return { addListener: (f) => l.push(f), fire: () => l.forEach((f) => f()) }; }
const sCache = new Map(), tCache = new Map(), cCache = new Map();
function sliderState(id) {
  if (sCache.has(id)) return sCache.get(id);
  let st;
  if (HAS_BACKEND && initData.__juce__sliders.includes(id)) st = Juce.getSliderState(id);
  else { const ev = mockEvents(); let v = (id in MOCK.sliders) ? MOCK.sliders[id] : .5;
    st = { getNormalisedValue: () => v, setNormalisedValue(n) { v = clamp01(n); ev.fire(); }, getScaledValue: () => v,
      sliderDragStarted() {}, sliderDragEnded() {}, valueChangedEvent: ev, propertiesChangedEvent: mockEvents() }; }
  sCache.set(id, st); return st;
}
function toggleState(id) {
  if (tCache.has(id)) return tCache.get(id);
  let st;
  if (HAS_BACKEND && initData.__juce__toggles.includes(id)) st = Juce.getToggleState(id);
  else { const ev = mockEvents(); let v = !!MOCK.toggles[id];
    st = { getValue: () => v, setValue(n) { v = !!n; ev.fire(); }, valueChangedEvent: ev, propertiesChangedEvent: mockEvents() }; }
  tCache.set(id, st); return st;
}
function comboState(id) {
  if (cCache.has(id)) return cCache.get(id);
  let st;
  if (HAS_BACKEND && initData.__juce__comboBoxes.includes(id)) st = Juce.getComboBoxState(id);
  else { const ev = mockEvents(); let v = MOCK.combos[id] || 0;
    st = { getChoiceIndex: () => v, setChoiceIndex(i) { v = i; ev.fire(); }, valueChangedEvent: ev, propertiesChangedEvent: mockEvents() }; }
  cCache.set(id, st); return st;
}
// safe combo index against OUR locked list length (real state can report junk
// before properties arrive)
const cIdx = (id, len) => { const i = comboState(id).getChoiceIndex(); return Math.max(0, Math.min(len - 1, isFinite(i) ? Math.round(i) : 0)); };

// Request every relay id up-front -> guarantees the requested set equals the
// C++ makeSliderIds/makeToggleIds/makeComboIds set (1:1 relay parity), incl.
// the flagged/deferred ids that carry no panel control (drift, vec_orbit_voice,
// interp, engine).
SLIDER_IDS.forEach(sliderState);
TOGGLE_IDS.forEach(toggleState);
COMBO_IDS.forEach(comboState);

//============================================================================
// UI-only state + shared refresh machinery
let sel = 0;                 // selected tone (TONE EDIT target) -- UI state, not a param
let preset = 0;              // preset browser index -- UI-only
let midiLearn = false;       // MIDI LEARN cosmetic toggle (feature deferred)
let kbdOpen = false;         // keyboard fold (boot collapsed = 660)
let menu = null;             // {title, rows, cur, pick}
let utilOpen = false, penvOpen = false;
let lofiFocus = 0, talkFocus = 0;   // UTIL LO-FI / TALK focus (UI-local)
let previewPhase = sliderState('vec_phase').getNormalisedValue();
const flow = [0, .25, .5, .75];     // constellation dot progress per tone
let touched = { label: 'MORPH', v: .52, bip: false };
let pitchVal = 0, modVal = 0;       // wheels

const refreshers = [];
let _raf = 0;
function scheduleRefresh() { if (_raf) return; _raf = requestAnimationFrame(() => { _raf = 0; refresh(); }); }
function refresh() { for (let i = 0; i < refreshers.length; i++) refreshers[i](); }
function setTouched(label, v, bip) { touched = { label, v, bip: !!bip }; }
const ledCss = (on) => `background:${on ? '#ff2b3e' : '#4a1020'};box-shadow:${on ? '0 0 8px rgba(255,43,62,.85)' : 'none'}`;

//============================================================================
// Generic components (exact master geometry). Each registers an updater.

// vertical drag law: full range over 140px, pointer capture, bipolar centre
// detent. adapter = fn -> object with getNormalisedValue/setNormalisedValue/
// sliderDragStarted/sliderDragEnded (a relay slider state or a small shim).
function attachDrag(elm, adapter, labelFn, bip) {
  elm.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    if (midiLearn) return;                      // MIDI LEARN mode: control edits suppressed (feature deferred)
    const st = adapter();
    try { elm.setPointerCapture(e.pointerId); } catch (_) {}
    const sy = e.clientY, sv = clamp01(st.getNormalisedValue());
    if (st.sliderDragStarted) st.sliderDragStarted();
    const mv = (ev) => {
      let v = clamp01(sv + (sy - ev.clientY) / 140);
      if (bip && Math.abs(v - .5) < .035) v = .5;
      st.setNormalisedValue(v); setTouched(labelFn(), v, bip); scheduleRefresh();
    };
    const up = () => { elm.removeEventListener('pointermove', mv); elm.removeEventListener('pointerup', up); elm.removeEventListener('pointercancel', up); if (st.sliderDragEnded) st.sliderDragEnded(); };
    elm.addEventListener('pointermove', mv);
    elm.addEventListener('pointerup', up);
    elm.addEventListener('pointercancel', up);
  });
}

// o: { adapter, label, ptr, size, w, inset, ptrTop, labelSize, bip, dynLabel, dynPtr, row }
function knob(o) {
  const size = o.size, inner = Math.round(size * .76), ptrH = Math.round(inner * .34);
  const w = (o.w != null) ? o.w : (size + 12);
  const inset = (o.inset != null) ? o.inset : (size >= 30 ? 5 : 4);
  const ptrTop = (o.ptrTop != null) ? o.ptrTop : 2;
  const labelSize = (o.labelSize != null) ? o.labelSize : 8;
  const col = mk('div', `display:flex;flex-direction:${o.row ? 'row' : 'column'};align-items:center;gap:3px;width:${w}px;flex:none`);
  const skirt = mk('div', `border-radius:50%;background:#14162c;display:flex;align-items:center;justify-content:center;cursor:ns-resize;box-shadow:0 2px 4px rgba(0,0,0,.5);width:${size}px;height:${size}px;flex:none`);
  const fluted = mk('div', `border-radius:50%;background:repeating-conic-gradient(#2a2d48 0deg 12deg, #20233c 12deg 24deg);position:relative;width:${inner}px;height:${inner}px`);
  fluted.appendChild(mk('div', `position:absolute;inset:${inset}px;border-radius:50%;background:#2a2d48`));
  const ptrWrap = mk('div', `position:absolute;inset:0`);
  const ptr = mk('div', `position:absolute;left:50%;top:${ptrTop}px;width:2px;margin-left:-1px;border-radius:1px;background:${o.ptr || '#e8eaff'};height:${ptrH}px`);
  ptrWrap.appendChild(ptr); fluted.appendChild(ptrWrap); skirt.appendChild(fluted);
  const label = mk('div', `font:600 ${labelSize}px 'Barlow Semi Condensed';letter-spacing:.08em;color:#9aa1d8;white-space:nowrap`, o.label);
  if (!o.bare) { col.appendChild(skirt); col.appendChild(label); }
  attachDrag(skirt, o.adapter, () => (o.dynLabel ? o.dynLabel() : o.label), o.bip);
  refreshers.push(() => {
    const learn = midiLearn;
    const n = clamp01(o.adapter().getNormalisedValue());
    ptrWrap.style.transform = `rotate(${Math.round(-135 + n * 270)}deg)`;
    if (!o.bare && o.dynLabel) label.textContent = o.dynLabel();
    ptr.style.background = learn ? '#464e94' : (o.dynPtr ? o.dynPtr() : (o.ptr || '#e8eaff'));
  });
  return { col: o.bare ? skirt : col, skirt, ptr, label };
}
// slider-state adapter for a fixed relay id
const slAdapter = (id) => () => sliderState(id);
// slider-state adapter for the currently-selected tone's per-tone id
const selAdapter = (base) => () => sliderState(base + SFX[sel]);

// vertical slider (level fader / env). h = track height. cap 19x9, travel h-9.
function vslider(o) { // { adapter, h, label, full }
  const h = o.h;
  const box = mk('div', `display:flex;flex-direction:column;align-items:center;gap:3px;width:20px;flex:none`);
  const track = mk('div', `position:relative;width:9px;height:${h}px;background:#0c0e20;border-radius:2px;cursor:ns-resize;box-shadow:inset 0 1px 3px rgba(0,0,0,.8)`);
  const cap = mk('div', `position:absolute;left:-5px;width:19px;height:9px;border-radius:2px;background:#2a2d48;border-top:2px solid #e8eaff;box-shadow:0 2px 3px rgba(0,0,0,.6)`);
  track.appendChild(cap); box.appendChild(track);
  if (o.label != null) box.appendChild(mk('div', `font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8`, o.label));
  attachDrag(track, o.adapter, () => (o.full || o.label));
  refreshers.push(() => { cap.style.bottom = Math.round(clamp01(o.adapter().getNormalisedValue()) * (h - 9)) + 'px'; });
  return { box, track };
}

// small silkscreen stepper (< / >) with press feedback
function stepper(glyph, onClick, w, h, fs) {
  w = w || 20; h = h || 18; fs = fs || 10;
  const b = mk('div', `width:${w}px;height:${h}px;background:#181b34;border:1px solid #464e94;border-radius:2px;color:#c9cdf2;font:700 ${fs}px 'Barlow Semi Condensed';display:flex;align-items:center;justify-content:center;cursor:pointer;flex:none;user-select:none`, glyph);
  b.addEventListener('pointerdown', () => { b.style.background = '#2c3160'; });
  const rst = () => { b.style.background = '#181b34'; };
  b.addEventListener('pointerup', rst); b.addEventListener('pointerleave', rst);
  b.addEventListener('click', (e) => { e.stopPropagation(); onClick(); scheduleRefresh(); });
  return b;
}
// clickable LCD readout (select-type). extra: {center, padL, dyn}
function lcd(css, dynText, onClick) {
  const d = mk('div', `background:#07070a;border-radius:2px;box-shadow:inset 0 1px 4px #000;color:#ffd23f;font-family:'Doto';display:flex;align-items:center;white-space:nowrap;overflow:hidden;${css}${onClick ? ';cursor:pointer' : ''}`);
  if (onClick) d.addEventListener('click', (e) => { e.stopPropagation(); onClick(); });
  refreshers.push(() => { d.textContent = dynText(); });
  return d;
}
// text button (cycler / toggle face). css should set width/height/font.
function tbtn(css, dynText, onClick) {
  const d = mk('div', `background:#181b34;border:1px solid #464e94;border-radius:2px;color:#c9cdf2;display:flex;align-items:center;justify-content:center;cursor:pointer;box-shadow:0 1px 2px rgba(0,0,0,.4);user-select:none;${css}`);
  d.addEventListener('pointerdown', () => { d.style.background = '#2c3160'; });
  const rst = () => { d.style.background = '#181b34'; };
  d.addEventListener('pointerup', rst); d.addEventListener('pointerleave', rst);
  d.addEventListener('click', (e) => { e.stopPropagation(); onClick(); scheduleRefresh(); });
  if (dynText) refreshers.push(() => { d.textContent = dynText(); });
  return d;
}
// small round LED bound to a boolean getter
function led(size, getOn) {
  const d = mk('div', `width:${size}px;height:${size}px;border-radius:50%;flex:none`);
  refreshers.push(() => { d.style.cssText = `width:${size}px;height:${size}px;border-radius:50%;flex:none;${ledCss(getOn())}`; });
  return d;
}
// square push button (LED-less face) that flips a toggle relay
function pushToggle(css, id) {
  const d = mk('div', `background:#181b34;border:1px solid #464e94;border-radius:3px;cursor:pointer;box-shadow:0 2px 3px rgba(0,0,0,.5);${css}`);
  d.addEventListener('click', (e) => { e.stopPropagation(); const t = toggleState(id); t.setValue(!t.getValue()); scheduleRefresh(); });
  return d;
}
function labelTxt(css, text) { return mk('div', css, text); }

// combo helpers
const comboLen = { wave: WAVES.length, shape: SHAPES.length, tvf_type: TVFTYPES.length, aux_dest: AUXDESTS.length,
  lfo1_dest: LFODESTS.length, lfo2_dest: LFODESTS.length, lfo1_shape: LFOSHAPES.length, lfo2_shape: LFOSHAPES.length,
  voicing: VOICINGS.length, dreamy_spread: SPREADS.length, loop_mode: LOOPMODES.length, hit_play: PLAYMODES.length,
  vec_orbit_shape: ORBITSHAPES.length, lfo_shape: LFOSHAPES.length, mtx1_src: MSRC.length, mtx2_src: MSRC.length, mtx3_src: MSRC.length,
  mtx1_dst: MDST.length, mtx2_dst: MDST.length, mtx3_dst: MDST.length, flt_route: 2, flt1_type: FTYPES.length, flt2_type: FTYPES.length,
  modfx_type: MODFX.length, dly_mode: DLYMODES.length, rev_type: REVTYPES.length,
  modfx_pfocus: 4, dly_pfocus: 4, rev_pfocus: 4, fx_prepost: PREPOST.length, interp: 2, engine: 2 };
function cbCycle(id, dir) { const len = comboLen[id]; const i = cIdx(id, len); comboState(id).setChoiceIndex(((i + dir) % len + len) % len); scheduleRefresh(); }
// selected-tone combo helpers
function selCbIdx(base, len) { return cIdx(base + SFX[sel], len); }
function selCbSet(base, i) { comboState(base + SFX[sel]).setChoiceIndex(i); scheduleRefresh(); }
function selCbCycle(base, len, dir) { const i = selCbIdx(base, len); comboState(base + SFX[sel]).setChoiceIndex(((i + dir) % len + len) % len); scheduleRefresh(); }

//============================================================================
const frame = $('frame');

// ---- faceplate finish (seeded brushed metal) + corner screws --------------
function buildFaceplate() {
  const plate = mk('div', 'position:absolute;left:0;top:0;width:1140px;height:660px;pointer-events:none;overflow:hidden');
  // seeded streaks (seed 19971, LCG a=16807)
  let seed = 19971; const mr = () => (seed = seed * 16807 % 2147483647) / 2147483647;
  const svg = mkNS('svg'); setAttrs(svg, { viewBox: '0 0 1140 660' }); svg.style.cssText = 'position:absolute;inset:0;width:100%;height:100%';
  for (let y = 1; y < 660; y += 4 + Math.floor(mr() * 5)) {
    const light = mr() > .45, o = +(0.008 + mr() * .024).toFixed(3), w = +(0.5 + mr() * .7).toFixed(2);
    const r = mkNS('rect'); setAttrs(r, { x: 0, y, width: 1140, height: w, fill: light ? '#fff' : '#000', opacity: o }); svg.appendChild(r);
  }
  for (let i = 0; i < 18; i++) {
    const y = Math.floor(mr() * 660), x0 = Math.floor(mr() * 800), len = 120 + Math.floor(mr() * 560);
    const r = mkNS('rect'); setAttrs(r, { x: x0, y, width: len, height: 0.8, rx: 0.4, fill: '#fff', opacity: +(0.03 + mr() * .05).toFixed(3) }); svg.appendChild(r);
  }
  plate.appendChild(svg);
  plate.appendChild(mk('div', 'position:absolute;inset:0;background:linear-gradient(180deg, rgba(255,255,255,.10) 0%, rgba(255,255,255,.03) 25%, rgba(0,0,0,.05) 80%, rgba(0,0,0,.12) 100%)'));
  plate.appendChild(mk('div', 'position:absolute;inset:0;background:linear-gradient(122deg, rgba(255,255,255,0) 40%, rgba(255,255,255,.05) 48%, rgba(216,222,255,.07) 56%, rgba(255,255,255,.02) 64%, rgba(255,255,255,0) 72%)'));
  plate.appendChild(mk('div', 'position:absolute;inset:0;background:radial-gradient(ellipse 75% 75% at 50% 50%, rgba(0,0,0,0) 55%, rgba(0,0,0,.16) 100%)'));
  frame.appendChild(plate);
  // 4 corner screws, centres 22px from the faceplate corners (top y22, bottom y638)
  const screws = mk('div', 'position:absolute;inset:0;pointer-events:none;z-index:6');
  const scr = (left, top, rot) => {
    const s = mk('div', `position:absolute;left:${left}px;top:${top}px;width:16px;height:16px;border-radius:50%;background:radial-gradient(circle at 38% 38%, #6b708f, #292b42 70%, #0d0d1a);box-shadow:0 1px 2px rgba(0,0,0,.6)`);
    s.appendChild(mk('div', `position:absolute;left:3.5px;top:7.2px;width:9px;height:1.6px;border-radius:1px;background:#05050c;transform:rotate(${rot}deg)`));
    screws.appendChild(s);
  };
  scr(14, 14, 37); scr(1110, 14, 112); scr(14, 630, 74); scr(1110, 630, 155);
  frame.appendChild(screws);
}

// ---- header ---------------------------------------------------------------
function buildHeader() {
  const h = mk('div', 'position:relative;height:60px;background:#1b1f40;border-bottom:1px solid #464e94;display:flex;align-items:center;padding:0 16px;gap:16px');
  // logo
  const logo = mk('div', 'display:flex;flex-direction:column;gap:2px;align-items:center;margin-left:26px');
  logo.appendChild(mk('div', "font:400 19px 'Michroma';color:#c9cdf2;letter-spacing:.14em;white-space:nowrap", 'THE DREAMER'));
  logo.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.34em", '12-BIT ROM VECTOR SYNTHESIZER'));
  h.appendChild(logo);
  // centre cluster: preset steppers + main LCD
  const center = mk('div', 'flex:1;display:flex;justify-content:center');
  const psteps = mk('div', 'display:flex;flex-direction:column;gap:3px;margin-right:6px');
  psteps.appendChild(stepper('▲', () => selectPreset(preset - 1), 22, 19, 9));
  psteps.appendChild(stepper('▼', () => selectPreset(preset + 1), 22, 19, 9));
  center.appendChild(psteps);
  const lcdBox = mk('div', 'width:520px;height:44px;background:#07070a;border-radius:3px;box-shadow:inset 0 2px 8px rgba(0,0,0,.9), 0 1px 0 rgba(201,205,242,.15);padding:5px 10px;display:flex;align-items:center;gap:12px');
  const glyph = mk('div', 'display:flex;align-items:flex-end;gap:1px;height:30px');
  const bars = [];
  for (let i = 0; i < 12; i++) { const b = mk('div', 'width:3px;background:#ffd23f;opacity:.9;height:6px'); bars.push(b); glyph.appendChild(b); }
  lcdBox.appendChild(glyph);
  const lines = mk('div', 'flex:1;display:flex;flex-direction:column;gap:3px;overflow:hidden');
  const row1 = mk('div', "display:flex;justify-content:space-between;font:800 12px 'Doto';color:#ffd23f;letter-spacing:.04em;white-space:nowrap");
  const presetLbl = mk('div', 'cursor:pointer'); presetLbl.addEventListener('click', () => openPresetMenu());
  const toneLbl = mk('div', '');
  row1.appendChild(presetLbl); row1.appendChild(toneLbl);
  const row2 = mk('div', "display:flex;align-items:center;gap:8px;font:800 11px 'Doto';color:#ffd23f;white-space:nowrap");
  const touchedEl = mk('div', 'min-width:130px');
  const meterCells = mk('div', 'display:flex;gap:2px');
  const cells = [];
  for (let i = 0; i < 14; i++) { const c = mk('div', 'width:6px;height:9px;background:#1c1a10'); cells.push(c); meterCells.appendChild(c); }
  row2.appendChild(touchedEl); row2.appendChild(meterCells);
  lines.appendChild(row1); lines.appendChild(row2);
  lcdBox.appendChild(lines);
  center.appendChild(lcdBox);
  h.appendChild(center);
  // MIDI LEARN (feature deferred -- cosmetic LED toggle only)
  const midiCol = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:3px');
  const midiLed = mk('div', 'width:6px;height:6px;border-radius:50%');
  const midiBtn = tbtn("width:34px;height:16px;font:700 7px 'Barlow Semi Condensed';letter-spacing:.08em;border-radius:3px", null, () => { midiLearn = !midiLearn; setTouched(midiLearn ? 'MIDI LEARN ON' : 'MIDI LEARN OFF', midiLearn ? 1 : 0); });
  midiBtn.textContent = 'MIDI';
  midiCol.appendChild(midiLed); midiCol.appendChild(midiBtn);
  midiCol.appendChild(mk('div', "font:600 7px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.12em", 'LEARN'));
  h.appendChild(midiCol);
  // L/R output meters
  const meters = mk('div', 'display:flex;gap:3px;align-items:flex-end;height:40px');
  const meterFill = {};
  ['L', 'R'].forEach((ch) => {
    const colc = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:2px');
    const bar = mk('div', 'width:7px;height:34px;background:#07070a;border-radius:1px;box-shadow:inset 0 1px 3px #000;position:relative;overflow:hidden');
    const fill = mk('div', 'position:absolute;left:0;right:0;bottom:0;background:linear-gradient(180deg, #ff2b3e 0%, #ffd23f 20%, #ffd23f 100%);height:0%');
    meterFill[ch] = fill; bar.appendChild(fill); colc.appendChild(bar);
    colc.appendChild(mk('div', "font:600 6.5px 'Barlow Semi Condensed';color:#9aa1d8", ch));
    meters.appendChild(colc);
  });
  h.appendChild(meters);
  // MASTER knob
  h.appendChild(knob({ adapter: slAdapter('master'), label: 'MASTER', size: 40 }).col);
  // POWER
  const pw = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:4px;margin-right:26px');
  pw.appendChild(mk('div', 'width:8px;height:8px;border-radius:50%;background:#ff2b3e;box-shadow:0 0 8px rgba(255,43,62,.8)'));
  pw.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.2em", 'POWER'));
  h.appendChild(pw);
  frame.appendChild(h);
  // dynamic bits
  refreshers.push(() => {
    presetLbl.textContent = 'P' + String(preset + 1).padStart(3, '0') + ' ' + PRESETS[preset][1];
    toneLbl.textContent = 'TONE ' + TONELET[sel];
    const tv = touched.v;
    const val = touched.bip ? ((Math.round((tv - .5) * 126) >= 0 ? '+' : '') + Math.round((tv - .5) * 126)) : String(Math.round(tv * 127)).padStart(3, ' ');
    touchedEl.textContent = touched.label + ' ' + val;
    const fillN = Math.round(tv * 14);
    for (let i = 0; i < 14; i++) cells[i].style.background = i < fillN ? '#ffd23f' : '#1c1a10';
    midiLed.style.cssText = 'width:6px;height:6px;border-radius:50%;' + ledCss(midiLearn);
    midiBtn.style.background = midiLearn ? '#10122a' : '#181b34';
    // wave glyph bars (selected tone's wave, preview phase)
    const wi = selCbIdx('wave', WAVES.length), ph = previewPhase * 2 * Math.PI;
    for (let i = 0; i < 12; i++) {
      const hgt = Math.max(3, Math.round(4 + 24 * Math.abs(Math.sin((i / 11) * Math.PI * (1 + wi % 3 * .5) + ph * 2)) * (.75 + .25 * Math.sin(ph * 3 + i * .9))));
      bars[i].style.height = hgt + 'px';
    }
    meterFill.L.style.height = Math.round(Math.min(1, _mL) * 100) + '%';
    meterFill.R.style.height = Math.round(Math.min(1, _mR) * 100) + '%';
  });
}

// ---- row 1: TONES | TONE EDIT | FILTERS -----------------------------------
function groupPanel(css, title, titleExtra) {
  const g = mk('div', `border:1px solid #464e94;border-radius:4px;position:relative;${css}`);
  const t = mk('div', "position:absolute;top:-7px;left:8px;background:#232850;padding:0 5px;font:700 10px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.18em");
  t.appendChild(document.createTextNode(title));
  if (titleExtra) { t.appendChild(document.createTextNode('  ')); t.appendChild(titleExtra); }
  g.appendChild(t);
  return g;
}

function buildRow1() {
  const row = mk('div', 'display:grid;grid-template-columns:172px 1fr 200px;gap:10px;padding:14px 12px 0;height:352px;box-sizing:border-box');

  // ---- TONES mixer ----
  const gT = groupPanel('padding:16px 6px 8px', 'TONES');
  const cols = mk('div', 'display:flex;gap:4px;width:100%;justify-content:space-around');
  for (let i = 0; i < 4; i++) {
    const ti = i;
    const c = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:5px');
    c.appendChild(mk('div', "font:700 9px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.08em", TONELET[i]));
    c.appendChild(led(6, () => sel === ti));
    const selBtn = mk('div', 'width:20px;height:14px;background:#181b34;border:1px solid #464e94;border-radius:3px;cursor:pointer;box-shadow:0 2px 3px rgba(0,0,0,.5)');
    selBtn.addEventListener('click', () => { sel = ti; scheduleRefresh(); });
    c.appendChild(selBtn);
    // level fader 9x130
    const track = mk('div', 'position:relative;width:9px;height:130px;background:#0c0e20;border-radius:2px;cursor:ns-resize;box-shadow:inset 0 1px 3px rgba(0,0,0,.8)');
    const cap = mk('div', 'position:absolute;left:-5px;width:19px;height:9px;border-radius:2px;background:#2a2d48;border-top:2px solid #e8eaff;box-shadow:0 2px 3px rgba(0,0,0,.6)');
    track.appendChild(cap);
    attachDrag(track, slAdapter('level' + SFX[i]), () => 'LEVEL ' + TONELET[ti]);
    refreshers.push(() => { cap.style.bottom = Math.round(clamp01(sliderState('level' + SFX[ti]).getNormalisedValue()) * 121) + 'px'; });
    c.appendChild(track);
    // PAN mini-knob (bipolar, 24px)
    c.appendChild(knob({ adapter: slAdapter('pan' + SFX[i]), label: 'PAN', size: 24, w: 24, inset: 4, ptrTop: 1, labelSize: 7, bip: true }).col);
    c.appendChild(led(6, () => toggleState('on' + SFX[ti]).getValue()));
    c.appendChild(pushToggle('width:20px;height:14px', 'on' + SFX[i]));
    c.appendChild(mk('div', "font:600 7px 'Barlow Semi Condensed';color:#9aa1d8", 'ON'));
    // column dim when off
    refreshers.push(() => { c.style.opacity = toggleState('on' + SFX[ti]).getValue() ? '1' : '.55'; });
    cols.appendChild(c);
  }
  gT.appendChild(cols);
  row.appendChild(gT);

  // ---- TONE EDIT ----
  const editName = mk('span', 'color:#ff5b6e');
  const gE = groupPanel('padding:16px 12px 8px;display:flex;flex-direction:column;gap:9px', 'TONE EDIT', editName);
  refreshers.push(() => { editName.textContent = TONELET[sel]; });

  // Row: WAVE + SHAPE + LOOP/PLAY selector
  const r1 = mk('div', 'display:flex;align-items:center;gap:6px');
  r1.appendChild(labelTxt("font:600 8.5px 'Barlow Semi Condensed';letter-spacing:.1em;color:#9aa1d8", 'WAVE'));
  r1.appendChild(lcd("width:170px;height:20px;padding:0 6px;font:800 11px 'Doto'", () => { const w = WAVES[selCbIdx('wave', WAVES.length)]; return (w.cat + ' > ' + w.name).toUpperCase(); }, () => openWaveMenu()));
  r1.appendChild(stepper('<', () => selCbCycle('wave', WAVES.length, -1)));
  r1.appendChild(stepper('>', () => selCbCycle('wave', WAVES.length, +1)));
  r1.appendChild(mk('div', 'width:48px'));
  r1.appendChild(labelTxt("font:600 8.5px 'Barlow Semi Condensed';letter-spacing:.1em;color:#9aa1d8", 'SHAPE'));
  r1.appendChild(lcd("width:96px;height:20px;justify-content:center;font:800 11px 'Doto'", () => SHAPES[selCbIdx('shape', SHAPES.length)], () => openSelMenu('SHAPER — TONE ' + TONELET[sel], SHAPES, () => selCbIdx('shape', SHAPES.length), (i) => selCbSet('shape', i))));
  r1.appendChild(stepper('<', () => selCbCycle('shape', SHAPES.length, -1)));
  r1.appendChild(stepper('>', () => selCbCycle('shape', SHAPES.length, +1)));
  r1.appendChild(mk('div', 'width:24px'));
  // LOOP/PLAY selector: one fixed slot, swapped by wave bank tag
  const loopSel = mk('div', 'display:flex;align-items:center;gap:5px');
  const loopLed = mk('div', 'width:6px;height:6px;border-radius:50%;flex:none');
  const loopLbl = mk('div', "font:600 8.5px 'Barlow Semi Condensed';letter-spacing:.1em;color:#9aa1d8", 'LOOP');
  const loopBtn = mk('div', "width:66px;height:20px;background:#181b34;border:1px solid #464e94;border-radius:2px;color:#c9cdf2;font:700 8px 'Barlow Semi Condensed';display:flex;align-items:center;justify-content:center;flex:none", 'FWD');
  loopSel.appendChild(loopLed); loopSel.appendChild(loopLbl); loopSel.appendChild(loopBtn);
  loopBtn.addEventListener('click', () => {
    const tag = WAVES[selCbIdx('wave', WAVES.length)].tag;
    if (tag === 'ENS') { const st = comboState('loop_mode' + SFX[sel]); st.setChoiceIndex(1 - cIdx('loop_mode' + SFX[sel], 2)); scheduleRefresh(); }
    else if (tag === 'SHOT') { const st = comboState('hit_play' + SFX[sel]); st.setChoiceIndex(1 - cIdx('hit_play' + SFX[sel], 2)); scheduleRefresh(); }
  });
  r1.appendChild(loopSel);
  gE.appendChild(r1);
  refreshers.push(() => {
    const tag = WAVES[selCbIdx('wave', WAVES.length)].tag;
    if (tag === 'ENS') {
      const on = cIdx('loop_mode' + SFX[sel], 2) === 1;
      loopSel.style.opacity = '1'; loopSel.style.pointerEvents = 'auto'; loopBtn.style.cursor = 'pointer';
      loopLed.style.cssText = 'width:6px;height:6px;border-radius:50%;flex:none;' + ledCss(on);
      loopLbl.textContent = 'LOOP'; loopBtn.textContent = LOOPMODES[cIdx('loop_mode' + SFX[sel], 2)];
    } else if (tag === 'SHOT') {
      const on = cIdx('hit_play' + SFX[sel], 2) === 1;
      loopSel.style.opacity = '1'; loopSel.style.pointerEvents = 'auto'; loopBtn.style.cursor = 'pointer';
      loopLed.style.cssText = 'width:6px;height:6px;border-radius:50%;flex:none;' + ledCss(on);
      loopLbl.textContent = 'PLAY'; loopBtn.textContent = PLAYMODES[cIdx('hit_play' + SFX[sel], 2)];
    } else { // cycle -> greyed LOOP FWD placeholder
      loopSel.style.opacity = '.4'; loopSel.style.pointerEvents = 'none'; loopBtn.style.cursor = 'default';
      loopLed.style.cssText = 'width:6px;height:6px;border-radius:50%;flex:none;background:#4a1020';
      loopLbl.textContent = 'LOOP'; loopBtn.textContent = 'FWD';
    }
  });

  // Row: knob bank (288px L box + hit compartment + R knobs)
  const r2 = mk('div', 'display:flex;align-items:flex-start;gap:4px;margin-left:22px');
  const lbox = mk('div', 'display:flex;align-items:flex-start;gap:4px;width:288px;box-sizing:border-box');
  [['oct', 'OCTAVE'], ['fine', 'FINE'], ['start', 'START'], ['level', 'LEVEL'], ['velo', 'VELOCITY']].forEach(([k, l]) =>
    lbox.appendChild(knob({ adapter: selAdapter(k), label: l, size: 34, w: 46 }).col));
  // RND button col
  const rnd = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:3px;width:26px;padding-top:4px');
  rnd.appendChild(led(6, () => toggleState('start_random' + SFX[sel]).getValue()));
  const rndBtn = mk('div', 'width:22px;height:14px;background:#181b34;border:1px solid #464e94;border-radius:3px;cursor:pointer;box-shadow:0 2px 3px rgba(0,0,0,.5)');
  rndBtn.addEventListener('click', () => { const t = toggleState('start_random' + SFX[sel]); t.setValue(!t.getValue()); scheduleRefresh(); });
  rnd.appendChild(rndBtn);
  rnd.appendChild(mk('div', "font:600 7px 'Barlow Semi Condensed';color:#9aa1d8", 'RND'));
  lbox.appendChild(rnd);
  r2.appendChild(lbox);
  // hit compartment (STRETCH / P.TRIM) -- greyed in place unless HIT+STRETCH
  const hitBox = mk('div', 'display:flex;align-items:flex-start;gap:4px;margin-left:28px');
  hitBox.appendChild(knob({ adapter: selAdapter('hit_stretch'), label: 'STRETCH', size: 34, w: 46 }).col);
  hitBox.appendChild(knob({ adapter: selAdapter('hit_pitchtrim'), label: 'P.TRIM', size: 34, w: 46 }).col);
  r2.appendChild(hitBox);
  refreshers.push(() => {
    const tag = WAVES[selCbIdx('wave', WAVES.length)].tag;
    const active = tag === 'SHOT' && cIdx('hit_play' + SFX[sel], 2) === 1;
    hitBox.style.opacity = active ? '1' : '.35'; hitBox.style.pointerEvents = active ? 'auto' : 'none';
  });
  // right knobs
  [['shape_depth', 'SHAPE DEPTH', '#ffd23f'], ['noise', 'NOISE', null], ['noise_color', 'NOISE COLOR', null]].forEach(([k, l, p]) =>
    r2.appendChild(knob({ adapter: selAdapter(k), label: l, ptr: p, size: 34, w: 46 }).col));
  gE.appendChild(r2);

  // Row: TVF + env banks
  const r3 = mk('div', 'display:flex;align-items:flex-start;margin-left:22px');
  const tvfBox = mk('div', 'display:flex;align-items:center;gap:10px;width:288px;height:88px;box-sizing:border-box');
  tvfBox.appendChild(mk('div', "font:700 9px 'Barlow Semi Condensed';color:#ff5b6e;letter-spacing:.14em;writing-mode:vertical-rl;transform:rotate(180deg);display:flex;align-items:center", 'TVF'));
  const tvfType = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:3px;justify-content:center');
  tvfType.appendChild(lcd("width:36px;height:18px;justify-content:center;font:800 10px 'Doto'", () => TVFTYPES[selCbIdx('tvf_type', TVFTYPES.length)], () => openSelMenu('TVF TYPE — TONE ' + TONELET[sel], TVFTYPES, () => selCbIdx('tvf_type', TVFTYPES.length), (i) => selCbSet('tvf_type', i))));
  tvfType.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';letter-spacing:.08em;color:#9aa1d8", 'TYPE'));
  tvfBox.appendChild(tvfType);
  const tvfKnobs = mk('div', 'display:flex;gap:4px');
  [['tvf_cut', 'CUTOFF'], ['tvf_res', 'RESONANCE'], ['tvf_env', 'ENVELOPE'], ['tvf_kf', 'KEY FLW']].forEach(([k, l]) =>
    tvfKnobs.appendChild(knob({ adapter: selAdapter(k), label: l, ptr: '#ff5b6e', size: 34 }).col));
  tvfBox.appendChild(tvfKnobs);
  r3.appendChild(tvfBox);
  const divider = () => mk('div', 'width:1px;align-self:stretch;background:#464e94;opacity:.4');
  const envBank = (base, title, mlLeft) => {
    const b = mk('div', `display:flex;flex-direction:column;align-items:center;gap:3px;margin-left:${mlLeft}px`);
    const rowk = mk('div', 'display:flex;gap:8px');
    [['a', 'A'], ['d', 'D'], ['s', 'S'], ['r', 'R']].forEach(([k, l]) =>
      rowk.appendChild(vslider({ adapter: selAdapter(base + '_' + k), h: 62, label: l, full: title + ' ' + l }).box));
    b.appendChild(rowk);
    b.appendChild(mk('div', "font:700 8px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.14em", title));
    return b;
  };
  const d1 = divider(); d1.style.marginLeft = '24px'; r3.appendChild(d1);
  r3.appendChild(envBank('tvf', 'FILTER', 9));
  r3.appendChild((() => { const d = divider(); d.style.marginLeft = '9px'; return d; })());
  r3.appendChild(envBank('tva', 'AMPLITUDE', 9));
  r3.appendChild((() => { const d = divider(); d.style.marginLeft = '9px'; return d; })());
  r3.appendChild(envBank('aux', 'AUX', 9));
  gE.appendChild(r3);

  // LFO1 / LFO2 rows
  [1, 2].forEach((n) => {
    const lb = 'lfo' + n;
    const r = mk('div', 'display:flex;align-items:center;gap:8px');
    r.appendChild(mk('div', "width:32px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.14em;color:#c9cdf2", 'LFO' + n));
    // RATE knob: yellow + division label when synced
    r.appendChild(knob({ adapter: selAdapter(lb + '_rate'), size: 26, labelSize: 7.5,
      dynLabel: () => toggleState(lb + '_sync' + SFX[sel]).getValue() ? SYNCDIVS[Math.min(SYNCDIVS.length - 1, Math.floor(clamp01(sliderState(lb + '_rate' + SFX[sel]).getNormalisedValue()) * SYNCDIVS.length))] : 'RATE',
      dynPtr: () => toggleState(lb + '_sync' + SFX[sel]).getValue() ? '#ffd23f' : '#e8eaff' }).col);
    r.appendChild(knob({ adapter: selAdapter(lb + '_depth'), label: 'DEPTH', size: 26, labelSize: 7.5 }).col);
    r.appendChild(lcd("width:34px;height:16px;justify-content:center;font:800 9px 'Doto'", () => LFOSHAPES[selCbIdx(lb + '_shape', LFOSHAPES.length)], () => openSelMenu('LFO' + n + ' SHAPE — TONE ' + TONELET[sel], LFOSHAPES, () => selCbIdx(lb + '_shape', LFOSHAPES.length), (i) => selCbSet(lb + '_shape', i))));
    const syncWrap = mk('div', 'display:flex;align-items:center;gap:4px');
    syncWrap.appendChild(led(6, () => toggleState(lb + '_sync' + SFX[sel]).getValue()));
    const syncBtn = tbtn("width:32px;height:15px;font:700 7px 'Barlow Semi Condensed';letter-spacing:.1em;border-radius:3px", null, () => { const t = toggleState(lb + '_sync' + SFX[sel]); t.setValue(!t.getValue()); });
    syncBtn.textContent = 'SYNC'; syncWrap.appendChild(syncBtn); r.appendChild(syncWrap);
    r.appendChild(mk('div', "font:600 8.5px 'Barlow Semi Condensed';color:#9aa1d8", 'DEST'));
    r.appendChild(tbtn("width:56px;height:16px;font:700 8px 'Barlow Semi Condensed'", () => LFODESTS[selCbIdx(lb + '_dest', LFODESTS.length)], () => openSelMenu('LFO' + n + ' DEST — TONE ' + TONELET[sel], LFODESTS, () => selCbIdx(lb + '_dest', LFODESTS.length), (i) => selCbSet(lb + '_dest', i))));
    gE.appendChild(r);
  });

  // AUX ENV row: AMOUNT (bipolar) + DEST + VOICING + spread
  const aux = mk('div', 'display:flex;align-items:center;gap:8px');
  aux.appendChild(mk('div', "width:32px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.14em;color:#c9cdf2", 'AUX ENV'));
  aux.appendChild(knob({ adapter: selAdapter('aux_amt'), label: 'AMOUNT', ptr: '#ffd23f', size: 26, labelSize: 7.5, bip: true, dynLabel: () => 'AUX AMT' }).col);
  aux.appendChild(mk('div', "font:600 8.5px 'Barlow Semi Condensed';color:#9aa1d8", 'DEST'));
  aux.appendChild(tbtn("width:56px;height:16px;font:700 8px 'Barlow Semi Condensed'", () => AUXDESTS[selCbIdx('aux_dest', AUXDESTS.length)], () => openSelMenu('AUX ENV DEST — TONE ' + TONELET[sel], AUXDESTS, () => selCbIdx('aux_dest', AUXDESTS.length), (i) => selCbSet('aux_dest', i))));
  aux.appendChild(mk('div', 'width:1px;align-self:stretch;background:#464e94;opacity:.4;margin:0 6px'));
  aux.appendChild(mk('div', "font:700 8px 'Barlow Semi Condensed';letter-spacing:.1em;color:#c9cdf2", 'VOICE'));
  aux.appendChild(lcd("width:54px;height:16px;justify-content:center;font:800 9px 'Doto'", () => VOICINGS[selCbIdx('voicing', VOICINGS.length)], () => openSelMenu('VOICING — TONE ' + TONELET[sel], VOICINGS, () => selCbIdx('voicing', VOICINGS.length), (i) => selCbSet('voicing', i))));
  const spread = lcd("width:44px;height:16px;justify-content:center;font:800 9px 'Doto'", () => SPREADS[selCbIdx('dreamy_spread', SPREADS.length)], () => openSelMenu('DREAMY SPREAD — TONE ' + TONELET[sel], SPREADS, () => selCbIdx('dreamy_spread', SPREADS.length), (i) => selCbSet('dreamy_spread', i)));
  aux.appendChild(spread);
  aux.appendChild(mk('div', 'flex:1'));
  refreshers.push(() => { spread.style.display = (selCbIdx('voicing', VOICINGS.length) === 3) ? 'flex' : 'none'; });
  gE.appendChild(aux);
  row.appendChild(gE);

  // ---- FILTERS ----
  const gF = groupPanel('padding:16px 10px 8px;display:flex;flex-direction:column;gap:7px;align-items:center', 'FILTERS');
  const fstrip = (num, id) => {
    const s = mk('div', 'display:flex;align-items:center;gap:5px;width:100%');
    s.appendChild(mk('div', "font:700 9px 'Barlow Semi Condensed';color:#c9cdf2;width:10px", num));
    s.appendChild(stepper('<', () => cbCycle(id, -1)));
    s.appendChild(lcd("flex:1;height:20px;justify-content:center;font:800 10px 'Doto'", () => FTYPES[cIdx(id, FTYPES.length)], () => openSelMenu('FILTER ' + num + ' TYPE', FTYPES, () => cIdx(id, FTYPES.length), (i) => { comboState(id).setChoiceIndex(i); scheduleRefresh(); })));
    s.appendChild(stepper('>', () => cbCycle(id, +1)));
    return s;
  };
  const fknobs = (defs) => { const w = mk('div', 'display:flex;justify-content:space-around;width:100%'); defs.forEach(([id, l, p]) => w.appendChild(knob({ adapter: slAdapter(id), label: l, ptr: p, size: 36 }).col)); return w; };
  gF.appendChild(fstrip('1', 'flt1_type'));
  gF.appendChild(fknobs([['flt1_cut', 'CUTOFF', '#ff5b6e'], ['flt1_res', 'RESONANCE', '#ff5b6e'], ['flt1_env', 'ENVELOPE', '#ff5b6e']]));
  gF.appendChild(mk('div', 'width:100%;height:1px;background:#464e94;opacity:.55'));
  gF.appendChild(fstrip('2', 'flt2_type'));
  gF.appendChild(fknobs([['flt2_cut', 'CUTOFF', '#ff5b6e'], ['flt2_res', 'RESONANCE', '#ff5b6e'], ['flt2_morph', 'MORPH', '#ffd23f']]));
  // route row + BAL
  const route = mk('div', 'margin-top:auto;display:flex;align-items:center;gap:8px;padding-bottom:2px');
  const serWrap = mk('div', 'display:flex;align-items:center;gap:4px'); serWrap.appendChild(led(6, () => cIdx('flt_route', 2) === 0)); serWrap.appendChild(mk('div', "font:600 8px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.08em", 'SER'));
  route.appendChild(serWrap);
  const routeBtn = mk('div', 'width:34px;height:18px;background:#181b34;border:1px solid #464e94;border-radius:3px;cursor:pointer;box-shadow:0 2px 3px rgba(0,0,0,.5)');
  routeBtn.addEventListener('click', () => { comboState('flt_route').setChoiceIndex(1 - cIdx('flt_route', 2)); scheduleRefresh(); });
  refreshers.push(() => { routeBtn.style.background = cIdx('flt_route', 2) === 1 ? '#10122a' : '#181b34'; });
  route.appendChild(routeBtn);
  const parWrap = mk('div', 'display:flex;align-items:center;gap:4px'); parWrap.appendChild(led(6, () => cIdx('flt_route', 2) === 1)); parWrap.appendChild(mk('div', "font:600 8px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.08em", 'PAR'));
  route.appendChild(parWrap);
  route.appendChild(knob({ adapter: slAdapter('flt_bal'), label: 'BALANCE', ptr: '#ffd23f', size: 26, labelSize: 7.5, bip: true, dynLabel: () => 'FLT BAL' }).col);
  gF.appendChild(route);
  row.appendChild(gF);

  frame.appendChild(row);
}

// ---- gain law + constellation ---------------------------------------------
function toneGain(i, phi) {
  const dir = clamp01(sliderState('dir' + SFX[i]).getNormalisedValue());
  const vint = clamp01(sliderState('vint' + SFX[i]).getNormalisedValue());
  const a = dir * 2 * Math.PI, g = Math.max(0, Math.cos(phi - a));
  return (1 - vint) + vint * g * g;
}

// ---- row 2: DREAM VECTOR | MOD MATRIX | FX ---------------------------------
function buildRow2() {
  const row = mk('div', 'display:grid;grid-template-columns:420px 296px 1fr;gap:10px;padding:14px 12px 0;height:212px;box-sizing:border-box');

  // ---- DREAM VECTOR ----
  const gV = groupPanel('padding:16px 10px 8px;display:flex;gap:10px', 'DREAM VECTOR');
  // left: constellation
  const left = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:4px');
  const flowBox = mk('div', 'position:relative;width:150px;height:132px;background:#07070a;border-radius:3px;box-shadow:inset 0 2px 8px rgba(0,0,0,.9)');
  const svg = mkNS('svg'); setAttrs(svg, { viewBox: '0 0 150 132' }); svg.style.cssText = 'position:absolute;inset:0;width:100%;height:100%';
  const ys = [22, 52, 82, 112], x0 = 22, x1 = 117, y1 = 66;
  const paths = [], nodes = [], nlabels = [], dots = [];
  for (let i = 0; i < 4; i++) {
    const p = mkNS('path'); setAttrs(p, { d: `M ${x0 + 9} ${ys[i]} C ${x0 + 45} ${ys[i]}, ${x1 - 40} ${y1}, ${x1} ${y1}`, fill: 'none', stroke: '#ffd23f' }); svg.appendChild(p); paths.push(p);
  }
  for (let i = 0; i < 4; i++) {
    const c = mkNS('circle'); setAttrs(c, { cx: x0, cy: ys[i] }); svg.appendChild(c); nodes.push(c);
    const t = mkNS('text'); setAttrs(t, { x: x0, y: ys[i] + 3, 'text-anchor': 'middle', 'font-family': 'Doto', 'font-weight': 800, 'font-size': 8 }); t.textContent = TONELET[i]; svg.appendChild(t); nlabels.push(t);
    const d = mkNS('circle'); setAttrs(d, { fill: '#ff2b3e' }); svg.appendChild(d); dots.push(d);
  }
  const sumC = mkNS('circle'); setAttrs(sumC, { cx: 126, cy: 66, r: 9, fill: 'none', stroke: '#464e94', 'stroke-width': 1.5 }); svg.appendChild(sumC);
  const sumT = mkNS('text'); setAttrs(sumT, { x: 126, y: 69, 'text-anchor': 'middle', 'font-family': 'Doto', 'font-weight': 800, 'font-size': 9, fill: '#ffd23f' }); sumT.textContent = 'Σ'; svg.appendChild(sumT);
  const mixT = mkNS('text'); setAttrs(mixT, { x: 126, y: 88, 'text-anchor': 'middle', 'font-family': 'Doto', 'font-weight': 800, 'font-size': 7, fill: '#9aa1d8' }); mixT.textContent = 'MIX'; svg.appendChild(mixT);
  const phBg = mkNS('rect'); setAttrs(phBg, { x: 6, y: 122, width: 138, height: 3, fill: '#1c2040' }); svg.appendChild(phBg);
  const phBar = mkNS('rect'); setAttrs(phBar, { y: 122, width: 10, height: 3, fill: '#ff2b3e' }); svg.appendChild(phBar);
  flowBox.appendChild(svg); left.appendChild(flowBox);
  left.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.12em", 'SIGNAL FLOW · PHASE'));
  const shRow = mk('div', 'display:flex;align-items:center;gap:4px');
  shRow.appendChild(mk('div', "font:600 8px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.08em", 'SHAPE'));
  shRow.appendChild(stepper('<', () => cbCycle('vec_orbit_shape', -1), 14, 16, 9));
  shRow.appendChild(tbtn("width:36px;height:16px;font:700 8px 'Barlow Semi Condensed'", () => ORBITSHAPES[cIdx('vec_orbit_shape', ORBITSHAPES.length)], () => openSelMenu('ORBIT SHAPE', ORBITSHAPES, () => cIdx('vec_orbit_shape', ORBITSHAPES.length), (i) => { comboState('vec_orbit_shape').setChoiceIndex(i); scheduleRefresh(); })));
  shRow.appendChild(stepper('>', () => cbCycle('vec_orbit_shape', +1), 14, 16, 9));
  left.appendChild(shRow);
  gV.appendChild(left);
  // mid: PHASE/RATE + ORBIT/P-ENV/EDIT
  const mid = mk('div', 'display:flex;flex-direction:column;gap:6px;justify-content:center');
  mid.appendChild(knob({ adapter: slAdapter('vec_phase'), label: 'PHASE', ptr: '#ffd23f', size: 32, dynLabel: () => 'PHASE' }).col);
  mid.appendChild(knob({ adapter: slAdapter('vec_orbit_rate'), label: 'RATE', size: 32 }).col);
  const orbRow = mk('div', 'display:flex;align-items:center;gap:5px');
  orbRow.appendChild(led(6, () => toggleState('vec_orbit_on').getValue()));
  orbRow.appendChild(pushToggle('width:26px;height:14px', 'vec_orbit_on'));
  orbRow.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8", 'ORBIT'));
  mid.appendChild(orbRow);
  const penvRow = mk('div', 'display:flex;align-items:center;gap:5px');
  penvRow.appendChild(led(6, () => toggleState('vec_penv_on').getValue()));
  penvRow.appendChild(pushToggle('width:26px;height:14px', 'vec_penv_on'));
  penvRow.appendChild(mk('div', "font:600 7.5px 'Barlow Semi Condensed';color:#9aa1d8", 'P-ENV'));
  mid.appendChild(penvRow);
  mid.appendChild(tbtn("width:40px;height:14px;font:700 7px 'Barlow Semi Condensed';letter-spacing:.1em;border-radius:3px", () => 'EDIT', () => { penvOpen = true; syncOverlays(); }));
  gV.appendChild(mid);
  gV.appendChild(mk('div', 'width:1px;align-self:stretch;background:#464e94;opacity:.4'));
  // right: per-tone DIR/INT
  const rightC = mk('div', 'display:flex;flex-direction:column;gap:4px;justify-content:center');
  for (let i = 0; i < 4; i++) {
    const ti = i;
    const r = mk('div', 'display:flex;align-items:center;gap:6px');
    const let_ = mk('div', "width:10px;font:800 9px 'Doto'", TONELET[i]);
    refreshers.push(() => { let_.style.color = sel === ti ? '#ff5b6e' : '#9aa1d8'; });
    r.appendChild(let_);
    [['dir', 'DIR'], ['vint', 'INT']].forEach(([k, l]) =>
      r.appendChild(knob({ adapter: slAdapter(k + SFX[i]), label: l + ' ' + TONELET[i], ptr: '#ffd23f', size: 22, ptrTop: 1, labelSize: 7, row: true }).col));
    rightC.appendChild(r);
  }
  rightC.appendChild(mk('div', "font:600 7px 'Barlow Semi Condensed';color:#7d84c0;letter-spacing:.1em;text-align:center", 'DIR · INT'));
  gV.appendChild(rightC);
  row.appendChild(gV);
  // constellation updater
  refreshers.push(() => {
    const phi = previewPhase * 2 * Math.PI;
    for (let i = 0; i < 4; i++) {
      const on = toggleState('on' + SFX[i]).getValue();
      const gain = on ? toneGain(i, phi) : 0;
      setAttrs(paths[i], { 'stroke-width': +(0.5 + gain * 2.5).toFixed(1), opacity: +(0.08 + gain * 0.8).toFixed(2) });
      setAttrs(nodes[i], { r: +(6 + gain * 3).toFixed(2), fill: on ? '#ffd23f' : '#3a3520', opacity: on ? +(0.3 + gain * 0.7).toFixed(2) : 0.25 });
      nlabels[i].setAttribute('fill', sel === i ? '#ff5b6e' : '#07070a');
      if (gain > .05 && on) {
        const tt = flow[i], u = 1 - tt, y = ys[i];
        const bx = u * u * u * (x0 + 9) + 3 * u * u * tt * (x0 + 45) + 3 * u * tt * tt * (x1 - 40) + tt * tt * tt * x1;
        const by = u * u * u * y + 3 * u * u * tt * y + 3 * u * tt * tt * y1 + tt * tt * tt * y1;
        setAttrs(dots[i], { cx: +bx.toFixed(1), cy: +by.toFixed(1), r: +(1.5 + gain * 2).toFixed(1), opacity: +(0.3 + gain * 0.7).toFixed(2) }); dots[i].style.display = '';
      } else dots[i].style.display = 'none';
    }
    phBar.setAttribute('x', (6 + previewPhase * 128).toFixed(1));
  });

  // ---- MOD MATRIX ----
  const gM = groupPanel('padding:16px 10px 8px;display:flex;flex-direction:column;gap:5px', 'MOD MATRIX');
  for (let i = 0; i < 3; i++) {
    const n = i + 1, srcId = 'mtx' + n + '_src', dstId = 'mtx' + n + '_dst', amtId = 'mtx' + n + '_amt';
    const r = mk('div', 'display:flex;align-items:center;gap:5px');
    r.appendChild(mk('div', "font:700 8px 'Barlow Semi Condensed';color:#9aa1d8;width:8px", String(n)));
    r.appendChild(tbtn("width:52px;height:18px;font:700 8px 'Barlow Semi Condensed';flex:none", () => MSRC[cIdx(srcId, MSRC.length)], () => openSelMenu('MOD SOURCE — SLOT ' + n, MSRC, () => cIdx(srcId, MSRC.length), (j) => { comboState(srcId).setChoiceIndex(j); scheduleRefresh(); })));
    r.appendChild(mk('div', "font:800 10px 'Doto';color:#ffd23f", '>'));
    r.appendChild(tbtn("width:52px;height:18px;font:700 8px 'Barlow Semi Condensed';flex:none", () => MDST[cIdx(dstId, MDST.length)], () => openSelMenu('MOD DEST — SLOT ' + n, MDST, () => cIdx(dstId, MDST.length), (j) => { comboState(dstId).setChoiceIndex(j); scheduleRefresh(); })));
    r.appendChild(knob({ adapter: slAdapter(amtId), size: 26, inset: 4, ptrTop: 1, ptr: '#ffd23f', bip: true, bare: true, dynLabel: () => 'AMT ' + n }).col);
    const barWrap = mk('div', 'flex:1;height:8px;background:#0c0e20;border-radius:2px;position:relative;overflow:hidden');
    const bar = mk('div', 'position:absolute;left:0;top:0;bottom:0;opacity:.8;width:0%');
    barWrap.appendChild(bar); r.appendChild(barWrap);
    refreshers.push(() => { const a = clamp01(sliderState(amtId).getNormalisedValue()); bar.style.width = Math.round(Math.abs(a - .5) * 2 * 100) + '%'; bar.style.background = a < .5 ? '#ff2b3e' : '#ffd23f'; });
    gM.appendChild(r);
  }
  const glfo = mk('div', 'display:flex;align-items:center;gap:8px;margin-top:auto');
  glfo.appendChild(mk('div', "font:700 8px 'Barlow Semi Condensed';letter-spacing:.14em;color:#c9cdf2", 'GLOBAL LFO'));
  glfo.appendChild(knob({ adapter: slAdapter('lfo_rate'), label: 'RATE', size: 30 }).col);
  glfo.appendChild(stepper('<', () => cbCycle('lfo_shape', -1), 14, 18, 9));
  glfo.appendChild(tbtn("width:40px;height:18px;font:700 8px 'Barlow Semi Condensed'", () => LFOSHAPES[cIdx('lfo_shape', LFOSHAPES.length)], () => openSelMenu('GLOBAL LFO SHAPE', LFOSHAPES, () => cIdx('lfo_shape', LFOSHAPES.length), (i) => { comboState('lfo_shape').setChoiceIndex(i); scheduleRefresh(); })));
  glfo.appendChild(stepper('>', () => cbCycle('lfo_shape', +1), 14, 18, 9));
  gM.appendChild(glfo);
  row.appendChild(gM);

  // ---- FX ----
  const gX = groupPanel('padding:16px 12px 8px;display:flex;flex-direction:column;gap:6px', 'FX');
  const utilBtn = mk('div', "position:absolute;top:-9px;right:10px;background:#232850;padding:1px 7px;border:1px solid #464e94;border-radius:3px;font:700 7.5px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.12em;cursor:pointer;z-index:2", 'UTIL ▸');
  utilBtn.addEventListener('click', () => { utilOpen = true; syncOverlays(); });
  gX.appendChild(utilBtn);
  const fxRow = (name, modeId, modeList, primary, focusList, focusId, slot, onId, hasSync, divider) => {
    const wrap = mk('div', 'display:flex;flex-direction:column;gap:6px');
    const r = mk('div', 'display:flex;align-items:center;gap:4px');
    r.appendChild(mk('div', "width:28px;font:700 7.5px 'Barlow Semi Condensed';letter-spacing:.08em;color:#c9cdf2", name));
    r.appendChild(lcd("width:46px;height:18px;justify-content:center;flex:none;font:800 8px 'Doto'", () => modeList[cIdx(modeId, modeList.length)], () => openSelMenu(name + ' TYPE', modeList, () => cIdx(modeId, modeList.length), (i) => { comboState(modeId).setChoiceIndex(i); scheduleRefresh(); })));
    primary.forEach((def) => {
      const [id, l] = def, synced = def[2];
      r.appendChild(knob({ adapter: slAdapter(id), label: l, size: 26, w: 32,
        dynLabel: synced ? (() => toggleState('dly_sync').getValue() ? SYNCDIVS[Math.min(SYNCDIVS.length - 1, Math.floor(clamp01(sliderState(id).getNormalisedValue()) * SYNCDIVS.length))] : l) : null,
        dynPtr: synced ? (() => toggleState('dly_sync').getValue() ? '#ffd23f' : '#e8eaff') : null }).col);
    });
    const focusCol = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:2px');
    focusCol.appendChild(lcd("width:44px;height:15px;justify-content:center;font:800 7.5px 'Doto'", () => focusList[cIdx(focusId, focusList.length)], () => cbCycle(focusId, +1)));
    focusCol.appendChild(mk('div', "font:600 6.5px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.06em", 'FOCUS'));
    r.appendChild(focusCol);
    r.appendChild(knob({ adapter: () => sliderState(slot + '_p' + cIdx(focusId, focusList.length)), label: 'PARAMS', ptr: '#ffd23f', size: 26, w: 32, labelSize: 7.5, dynLabel: () => 'PARAMS' }).col);
    if (hasSync) {
      const sc = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:2px');
      sc.appendChild(led(5, () => toggleState('dly_sync').getValue()));
      const sb = tbtn("width:30px;height:13px;font:700 6.5px 'Barlow Semi Condensed';letter-spacing:.08em;border-radius:3px", null, () => { const t = toggleState('dly_sync'); t.setValue(!t.getValue()); }); sb.textContent = 'SYNC';
      sc.appendChild(sb); r.appendChild(sc);
    }
    const onCol = mk('div', 'margin-left:auto;display:flex;align-items:center;gap:5px');
    onCol.appendChild(led(6, () => toggleState(onId).getValue()));
    onCol.appendChild(pushToggle('width:26px;height:14px', onId));
    r.appendChild(onCol);
    wrap.appendChild(r);
    if (divider) wrap.appendChild(mk('div', 'width:100%;height:1px;background:#464e94;opacity:.4'));
    return wrap;
  };
  gX.appendChild(fxRow('MOD', 'modfx_type', MODFX, [['modfx_rate', 'RATE'], ['modfx_depth', 'DEPTH'], ['modfx_mix', 'MIX']], MODFXFOCUS, 'modfx_pfocus', 'modfx', 'modfx_on', false, true));
  gX.appendChild(fxRow('DELAY', 'dly_mode', DLYMODES, [['dly_time', 'TIME', true], ['dly_fb', 'FB'], ['dly_mix', 'MIX']], DLYFOCUS, 'dly_pfocus', 'dly', 'dly_on', true, true));
  gX.appendChild(fxRow('REVERB', 'rev_type', REVTYPES, [['rev_size', 'SIZE'], ['rev_damp', 'DAMP'], ['rev_mix', 'MIX']], REVFOCUS, 'rev_pfocus', 'rev', 'rev_on', false, false));
  row.appendChild(gX);

  frame.appendChild(row);
}

// ---- rubber band + KEYS fold + keyboard/wheel strip -----------------------
let keysFoldBtn, kbStrip;
function buildKeys() {
  // rubber-band separator (y640) with the KEYS fold pill centred on it
  const rb = mk('div', 'position:absolute;left:0;top:640px;width:1140px;height:18px;z-index:7;background:linear-gradient(180deg, #101018 0%, #1c1c26 30%, #14141c 70%, #08080e 100%);box-shadow:0 2px 4px rgba(0,0,0,.6), inset 0 1px 0 rgba(255,255,255,.06);display:flex;align-items:center;justify-content:center');
  keysFoldBtn = mk('div', "width:56px;height:12px;background:#181b34;border:1px solid #464e94;border-radius:6px;cursor:pointer;box-shadow:0 1px 2px rgba(0,0,0,.5);color:#c9cdf2;font:700 6.5px 'Barlow Semi Condensed';display:flex;align-items:center;justify-content:center;letter-spacing:.12em", '▲ KEYS');
  keysFoldBtn.addEventListener('click', toggleKeys);
  rb.appendChild(keysFoldBtn);
  frame.appendChild(rb);

  // key bed strip (y658, shown only when expanded)
  kbStrip = mk('div', 'position:absolute;left:0;top:658px;width:1140px;height:190px;overflow:hidden;border-radius:0 0 8px 8px;background:#1a1c38;box-shadow:inset 0 1px 1px rgba(255,255,255,.1), inset 0 4px 8px rgba(0,0,0,.6);display:none');
  kbStrip.appendChild(mk('div', 'position:absolute;left:8px;top:10px;width:128px;height:170px;border-radius:6px;background:linear-gradient(180deg, #0d0d1c 0%, #171a2e 100%);box-shadow:inset 0 3px 6px rgba(0,0,0,.7)'));
  // pitch + mod wheels
  const wheel = (x, slotX, ribCount, ptr) => {
    kbStrip.appendChild(mk('div', `position:absolute;left:${slotX}px;top:25px;width:40px;height:140px;border-radius:20px;background:#05050d;box-shadow:inset 0 2px 5px rgba(0,0,0,.9)`));
    const w = mk('div', `position:absolute;left:${x}px;top:36px;width:32px;height:118px;border-radius:16px;background:linear-gradient(180deg, #121426 0%, #383d61 35%, #454a73 50%, #383d61 65%, #121426 100%);box-shadow:0 2px 4px rgba(0,0,0,.6);cursor:ns-resize;overflow:hidden`);
    const ribs = [];
    for (let i = 0; i < ribCount; i++) { const r = mk('div', 'position:absolute;left:0;width:32px;height:2px;background:rgba(0,0,0,.55)'); w.appendChild(r); ribs.push(r); }
    let stripe = null;
    if (ptr) { stripe = mk('div', 'position:absolute;left:0;width:32px;height:3px;background:#ff2b3e;box-shadow:0 0 4px rgba(255,43,62,.7);border-radius:1px'); w.appendChild(stripe); }
    kbStrip.appendChild(w);
    return { w, ribs, stripe };
  };
  const pitch = wheel(28, 24, 19, true);
  const modw = wheel(80, 76, 18, false);
  kbStrip.appendChild(mk('div', "position:absolute;left:22px;top:12px;font:600 7px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.14em", 'PITCH'));
  kbStrip.appendChild(mk('div', "position:absolute;left:81px;top:12px;font:600 7px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.14em", 'MOD'));
  refreshers.push(() => {
    for (let i = 0; i < pitch.ribs.length; i++) pitch.ribs[i].style.top = (2 + i * 6.2 + pitchVal * 20) + 'px';
    if (pitch.stripe) pitch.stripe.style.top = (57.5 + pitchVal * 20) + 'px';
    for (let i = 0; i < modw.ribs.length; i++) modw.ribs[i].style.top = (2 + i * 6.5 - modVal * 40) + 'px';
  });
  // PITCH: bipolar over 59px, springs back to 0 on release
  pitch.w.addEventListener('pointerdown', (e) => {
    e.preventDefault(); try { pitch.w.setPointerCapture(e.pointerId); } catch (_) {}
    const sy = e.clientY;
    const mv = (ev) => { pitchVal = Math.max(-1, Math.min(1, (ev.clientY - sy) / 59)); if (nfPitchBend) nfPitchBend(pitchVal); setTouched('PITCH BEND', (pitchVal + 1) / 2, true); scheduleRefresh(); };
    const up = () => { pitch.w.removeEventListener('pointermove', mv); pitch.w.removeEventListener('pointerup', up); pitch.w.removeEventListener('pointercancel', up); pitchVal = 0; if (nfPitchBend) nfPitchBend(0); scheduleRefresh(); };
    pitch.w.addEventListener('pointermove', mv); pitch.w.addEventListener('pointerup', up); pitch.w.addEventListener('pointercancel', up);
  });
  // MOD: unipolar over 118px, holds on release
  modw.w.addEventListener('pointerdown', (e) => {
    e.preventDefault(); try { modw.w.setPointerCapture(e.pointerId); } catch (_) {}
    const sy = e.clientY, s0 = modVal;
    const mv = (ev) => { modVal = Math.max(0, Math.min(1, s0 + (sy - ev.clientY) / 118)); if (nfModWheel) nfModWheel(modVal); setTouched('MOD WHEEL', modVal); scheduleRefresh(); };
    const up = () => { modw.w.removeEventListener('pointermove', mv); modw.w.removeEventListener('pointerup', up); modw.w.removeEventListener('pointercancel', up); };
    modw.w.addEventListener('pointermove', mv); modw.w.addEventListener('pointerup', up); modw.w.addEventListener('pointercancel', up);
  });
  // key bed (30 white / 21 black), left of the wheel bay (x=144)
  const bed = mk('div', 'position:absolute;left:144px;top:0;right:0;bottom:0');
  const semis = [0, 2, 4, 5, 7, 9, 11];
  const whiteNote = (i) => 36 + Math.floor(i / 7) * 12 + semis[i % 7];
  const held = { el: null };
  const mkKey = (note, css, dnBg, upBg) => {
    const k = mk('div', css + `;background:${upBg}`);
    const dn = (e) => { e.preventDefault(); k.style.background = dnBg; setTouched('NOTE ON', clamp01((note - 36) / 60)); if (nfNoteOn) nfNoteOn(note, 0.8); scheduleRefresh(); };
    const upf = () => { k.style.background = upBg; if (nfNoteOff) nfNoteOff(note); };
    k.addEventListener('pointerdown', dn); k.addEventListener('pointerup', upf); k.addEventListener('pointerleave', upf); k.addEventListener('pointercancel', upf);
    return k;
  };
  for (let i = 0; i < 30; i++) {
    const css = `position:absolute;top:0;width:32.662px;height:170px;border-radius:0 0 3px 3px;box-shadow:1px 0 1px rgba(0,0,0,.35), inset 0 -3px 3px rgba(0,0,0,.25);cursor:pointer;left:${(i * 32.662).toFixed(2)}px`;
    bed.appendChild(mkKey(whiteNote(i), css, 'linear-gradient(180deg, #b2b5c7 0%, #c7c9d6 75%, #e0e3ed 94%, #a4a8bd 100%)', 'linear-gradient(180deg, #c7c9d6 0%, #e0e3ed 75%, #f7f7fc 94%, #b2b5c7 100%)'));
  }
  const blackIdx = [];
  for (let i = 0; i < 30 && blackIdx.length < 21; i++) if ([0, 1, 3, 4, 5].includes(i % 7) && i < 29) blackIdx.push(i);
  blackIdx.forEach((i) => {
    const css = `position:absolute;top:0;width:20px;height:104px;border-radius:0 0 3px 3px;box-shadow:0 3px 5px rgba(0,0,0,.5);cursor:pointer;left:${(i * 32.662 + 22.5).toFixed(2)}px`;
    bed.appendChild(mkKey(whiteNote(i) + 1, css, 'linear-gradient(180deg, #05050a 0%, #0a0a12 60%, #383b52 92%, #1a1a24 100%)', 'linear-gradient(180deg, #1a1a24 0%, #0a0a12 60%, #383b52 92%, #05050a 100%)'));
  });
  kbStrip.appendChild(bed);
  frame.appendChild(kbStrip);

  // VER label (getInfo overrides with the built version)
  const ver = mk('div', "position:absolute;right:34px;top:626px;z-index:6;font:600 7.5px 'Barlow Semi Condensed';color:#7d84c0;letter-spacing:.16em", 'VER 1.0');
  ver._isVer = true; frame.appendChild(ver); verEl = ver;
}
let verEl = null;

// ---- overlays (menu / util / P-ENV) ---------------------------------------
let menuOverlay, menuTitleEl, menuList, utilOverlay, penvOverlay;
function buildOverlays() {
  // menu overlay (dynamic list)
  menuOverlay = mk('div', 'position:absolute;inset:0;background:rgba(7,7,10,.82);display:none;align-items:center;justify-content:center;z-index:10');
  const box = mk('div', 'width:430px;max-height:520px;background:#07070a;border:1px solid #464e94;border-radius:4px;box-shadow:0 8px 40px #000;padding:12px;display:flex;flex-direction:column;gap:8px');
  box.addEventListener('click', (e) => e.stopPropagation());
  const head = mk('div', "display:flex;justify-content:space-between;font:800 12px 'Doto';color:#ffd23f");
  menuTitleEl = mk('span', ''); head.appendChild(menuTitleEl); head.appendChild(mk('span', '', 'ESC=EXIT'));
  menuList = mk('div', 'overflow-y:auto;overflow-x:hidden;display:flex;flex-direction:column;min-height:0'); menuList.className = 'lcd-scroll';
  box.appendChild(head); box.appendChild(menuList); menuOverlay.appendChild(box);
  menuOverlay.addEventListener('click', () => { menu = null; syncOverlays(); });
  frame.appendChild(menuOverlay);

  // UTIL overlay
  utilOverlay = mk('div', 'position:absolute;inset:0;background:rgba(7,7,10,.82);display:none;align-items:center;justify-content:center;z-index:10');
  const ubox = mk('div', 'background:#1b1f40;border:1px solid #464e94;border-radius:4px;box-shadow:0 8px 40px #000;padding:14px 18px;display:flex;flex-direction:column;gap:12px;min-width:400px');
  ubox.addEventListener('click', (e) => e.stopPropagation());
  const uhead = mk('div', "display:flex;justify-content:space-between;gap:40px;font:700 10px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.18em");
  uhead.appendChild(mk('span', '', 'UTILITY — LO-FI · WIDTH · TALK')); uhead.appendChild(mk('span', 'color:#9aa1d8', 'ESC=EXIT'));
  ubox.appendChild(uhead);
  // LO-FI row
  const lofiRow = mk('div', 'display:flex;align-items:center;gap:10px');
  lofiRow.appendChild(led(6, () => toggleState('lofi_on').getValue()));
  { const b = tbtn("width:52px;height:18px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.1em;border-radius:3px", null, () => { const t = toggleState('lofi_on'); t.setValue(!t.getValue()); }); b.textContent = 'LO-FI'; lofiRow.appendChild(b); }
  lofiRow.appendChild(lcd("width:62px;height:16px;justify-content:center;font:800 8px 'Doto'", () => LOFIFOCUS[lofiFocus], () => { lofiFocus = (lofiFocus + 1) % LOFIFOCUS.length; scheduleRefresh(); }));
  const lofiAdapter = () => {
    if (lofiFocus === 3) { const t = toggleState('lofi_alias'); return { getNormalisedValue: () => t.getValue() ? 1 : 0, setNormalisedValue: (v) => t.setValue(v >= 0.5), sliderDragStarted() {}, sliderDragEnded() {} }; }
    return sliderState(['lofi_bits', 'lofi_srate', 'lofi_compand'][lofiFocus]);
  };
  lofiRow.appendChild(knob({ adapter: lofiAdapter, label: 'PARAMS', ptr: '#ffd23f', size: 30, w: 38, labelSize: 7.5, dynLabel: () => 'LOFI ' + LOFIFOCUS[lofiFocus] }).col);
  ubox.appendChild(lofiRow);
  // WIDTH row
  const widthRow = mk('div', 'display:flex;align-items:center;gap:10px');
  widthRow.appendChild(led(6, () => toggleState('width_on').getValue()));
  { const b = tbtn("width:52px;height:18px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.1em;border-radius:3px", null, () => { const t = toggleState('width_on'); t.setValue(!t.getValue()); }); b.textContent = 'WIDTH'; widthRow.appendChild(b); }
  widthRow.appendChild(knob({ adapter: slAdapter('width'), label: 'WIDTH', size: 30, w: 38, labelSize: 7.5 }).col);
  widthRow.appendChild(knob({ adapter: slAdapter('width_haas'), label: 'HAAS', size: 30, w: 38, labelSize: 7.5 }).col);
  const bassWrap = mk('div', 'display:flex;align-items:center;gap:5px;margin-left:6px');
  bassWrap.appendChild(led(6, () => toggleState('width_bassmono').getValue()));
  { const b = tbtn("width:74px;height:18px;font:700 7.5px 'Barlow Semi Condensed';letter-spacing:.06em;border-radius:3px", null, () => { const t = toggleState('width_bassmono'); t.setValue(!t.getValue()); }); b.textContent = 'BASS MONO'; bassWrap.appendChild(b); }
  widthRow.appendChild(bassWrap);
  ubox.appendChild(widthRow);
  // TALK row
  const talkRow = mk('div', 'display:flex;align-items:center;gap:10px');
  talkRow.appendChild(led(6, () => toggleState('talk_on').getValue()));
  { const b = tbtn("width:52px;height:18px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.1em;border-radius:3px", null, () => { const t = toggleState('talk_on'); t.setValue(!t.getValue()); }); b.textContent = 'TALK'; talkRow.appendChild(b); }
  talkRow.appendChild(lcd("width:62px;height:16px;justify-content:center;font:800 8px 'Doto'", () => TALKFOCUS[talkFocus], () => { talkFocus = (talkFocus + 1) % TALKFOCUS.length; scheduleRefresh(); }));
  const talkAdapter = () => sliderState(['talk_va', 'talk_vb', 'talk_morph', 'talk_sens'][talkFocus]);
  talkRow.appendChild(knob({ adapter: talkAdapter, label: 'PARAMS', ptr: '#ffd23f', size: 30, w: 38, labelSize: 7.5, dynLabel: () => 'TALK ' + TALKFOCUS[talkFocus] }).col);
  ubox.appendChild(talkRow);
  // PRE/POST placement
  const ppRow = mk('div', 'display:flex;align-items:center;gap:10px;border-top:1px solid #464e94;padding-top:10px');
  ppRow.appendChild(mk('div', "font:600 8px 'Barlow Semi Condensed';color:#9aa1d8;letter-spacing:.06em", 'LO-FI / WIDTH PLACEMENT'));
  ppRow.appendChild(tbtn("width:88px;height:18px;font:700 8px 'Barlow Semi Condensed';letter-spacing:.08em;border-radius:3px", () => PREPOST[cIdx('fx_prepost', 2)] + ' FILTERS', () => { comboState('fx_prepost').setChoiceIndex(1 - cIdx('fx_prepost', 2)); scheduleRefresh(); }));
  ubox.appendChild(ppRow);
  utilOverlay.appendChild(ubox);
  utilOverlay.addEventListener('click', () => { utilOpen = false; syncOverlays(); });
  frame.appendChild(utilOverlay);

  // P-ENV overlay
  penvOverlay = mk('div', 'position:absolute;inset:0;background:rgba(7,7,10,.82);display:none;align-items:center;justify-content:center;z-index:10');
  const pbox = mk('div', 'background:#1b1f40;border:1px solid #464e94;border-radius:4px;box-shadow:0 8px 40px #000;padding:14px 16px;display:flex;flex-direction:column;gap:10px');
  pbox.addEventListener('click', (e) => e.stopPropagation());
  const phead = mk('div', "display:flex;justify-content:space-between;gap:30px;font:700 10px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.18em");
  phead.appendChild(mk('span', '', 'P-ENV — VECTOR PHASE ENVELOPE')); phead.appendChild(mk('span', 'color:#9aa1d8', 'ESC=EXIT'));
  pbox.appendChild(phead);
  const prow = mk('div', 'display:flex;align-items:center;gap:14px');
  prow.appendChild(knob({ adapter: slAdapter('vec_penv_start'), label: 'START', ptr: '#ffd23f', size: 38, dynLabel: () => 'PENV START' }).col);
  prow.appendChild(knob({ adapter: slAdapter('vec_penv_end'), label: 'END', ptr: '#ffd23f', size: 38, dynLabel: () => 'PENV END' }).col);
  prow.appendChild(knob({ adapter: slAdapter('vec_penv_time'), label: 'TIME', size: 38, dynLabel: () => 'PENV TIME' }).col);
  const loopCol = mk('div', 'display:flex;flex-direction:column;align-items:center;gap:4px');
  loopCol.appendChild(led(8, () => toggleState('vec_penv_loop').getValue()));
  loopCol.appendChild(pushToggle('width:34px;height:22px', 'vec_penv_loop'));
  loopCol.appendChild(mk('div', "font:600 8px 'Barlow Semi Condensed';color:#c9cdf2;letter-spacing:.12em", 'LOOP'));
  prow.appendChild(loopCol);
  pbox.appendChild(prow);
  penvOverlay.appendChild(pbox);
  penvOverlay.addEventListener('click', () => { penvOpen = false; syncOverlays(); });
  frame.appendChild(penvOverlay);
}

function syncOverlays() {
  utilOverlay.style.display = utilOpen ? 'flex' : 'none';
  penvOverlay.style.display = penvOpen ? 'flex' : 'none';
  menuOverlay.style.display = menu ? 'flex' : 'none';
  if (menu) {
    menuTitleEl.textContent = menu.title;
    menuList.innerHTML = '';
    const cur = menu.getCur();
    menu.rows.forEach((rw, i) => {
      const isCur = i === cur;
      const row = mk('div', `display:flex;gap:10px;padding:3px 6px;cursor:pointer;font:800 12px 'Doto';color:${isCur ? '#07070a' : '#ffd23f'};background:${isCur ? '#ffd23f' : 'transparent'}`);
      row.appendChild(mk('span', 'width:20px', rw.num));
      row.appendChild(mk('span', 'width:44px', rw.cat));
      row.appendChild(mk('span', '', (isCur ? '▸ ' : '') + rw.name));
      row.addEventListener('click', (e) => { e.stopPropagation(); menu.pick(i); menu = null; syncOverlays(); });
      menuList.appendChild(row);
    });
  }
  scheduleRefresh();
}
const pad2 = (n) => String(n).padStart(2, '0');
function openMenu(title, rows, getCur, pick) { if (midiLearn) return; menu = { title, rows, getCur, pick }; syncOverlays(); }
function openWaveMenu() { openMenu('WAVE SELECT — TONE ' + TONELET[sel], WAVES.map((w, i) => ({ num: pad2(i + 1), cat: w.cat.toUpperCase(), name: (w.name + (w.tag ? ' [' + w.tag + ']' : '')).toUpperCase() })), () => selCbIdx('wave', WAVES.length), (i) => selCbSet('wave', i)); }
function openSelMenu(title, list, getCur, pick) { openMenu(title, list.map((n, i) => ({ num: pad2(i + 1), cat: '', name: n })), getCur, pick); }
function openPresetMenu() { openMenu('PRESET SELECT', PRESETS.map(([c, n], i) => ({ num: pad2(i + 1), cat: c, name: n })), () => preset, (i) => selectPreset(i)); }

// ---- meters + animation ---------------------------------------------------
let _mL = 0, _mR = 0, _mtL = 0, _mtR = 0;
window.uiMeters = function (o) { if (o) { _mtL = clamp01(+o.l || 0); _mtR = clamp01(+o.r || 0); } };
function animTick() {
  const phi0 = previewPhase * 2 * Math.PI;
  if (toggleState('vec_orbit_on').getValue())
    previewPhase = (previewPhase + .003 + clamp01(sliderState('vec_orbit_rate').getNormalisedValue()) * .012) % 1;
  else
    previewPhase = clamp01(sliderState('vec_phase').getNormalisedValue());
  const phi = previewPhase * 2 * Math.PI;
  let act = 0;
  for (let i = 0; i < 4; i++) {
    const on = toggleState('on' + SFX[i]).getValue();
    const g = on ? toneGain(i, phi) : 0;
    flow[i] = (flow[i] + .01 + g * .05) % 1;
    if (on) act += g * clamp01(sliderState('level' + SFX[i]).getNormalisedValue());
  }
  act /= 4;
  if (MOCKMODE) { const m = clamp01(sliderState('master').getNormalisedValue()); _mtL = m * (.45 + act * .8 + Math.random() * .12); _mtR = m * (.45 + act * .8 + Math.random() * .12); }
  _mL = Math.max(_mL * .82, _mtL); _mR = Math.max(_mR * .82, _mtR);
  refresh();
}

// ---- native bridge --------------------------------------------------------
const nativeFn = (name) => { try { return Juce.getNativeFunction(name); } catch (_) { return null; } };
const nfNoteOn = nativeFn('noteOn'), nfNoteOff = nativeFn('noteOff');
const nfPitchBend = nativeFn('pitchBend'), nfModWheel = nativeFn('modWheel');
const nfKeyboardFold = nativeFn('keyboardFold');
// preset bank native bridge (processor-owned factory presets)
const nfLoadPreset = nativeFn('loadPreset'), nfGetPresetList = nativeFn('getPresetList');
// select preset i (wrapping): update the browser index + ask the processor to
// recall it -> the APVTS relays then refresh every panel control automatically.
function selectPreset(i) {
  const n = PRESETS.length; if (!n) return;
  preset = ((i % n) + n) % n;
  if (nfLoadPreset) nfLoadPreset(preset);
  scheduleRefresh();
}
// on boot, fetch the factory bank names/categories from the processor and use
// it as the preset browser's data source (replacing the hardcoded fallback).
function loadPresetList() {
  if (!nfGetPresetList) return;
  try {
    const p = nfGetPresetList();
    if (p && p.then) p.then((list) => {
      if (Array.isArray(list) && list.length) {
        PRESETS = list.map((e) => [(e && e.category) || '', (e && e.name) || '']);
        if (preset >= PRESETS.length) preset = 0;
        scheduleRefresh();
      }
    }).catch(() => {});
  } catch (_) {}
}
function applyVersion() {
  let p = null; try { p = Juce.getNativeFunction('getInfo')(); } catch (_) {}
  if (p && p.then) p.then((info) => { if (info && info.version && verEl) verEl.textContent = 'VER ' + info.version; }).catch(() => {});
}

// ---- window-derived resize (fit) ------------------------------------------
// The WINDOW size is the sole source of truth; content scale is DERIVED:
// scale = min(innerWidth/1140, innerHeight/currentBaseH). transform-origin is
// top-left; a centering translate positions the (possibly letter-boxed) frame.
// No in-page grip, no GUI-zoom -- scale is only ever set here.
function fit() {
  const baseH = kbdOpen ? 848 : 660;
  frame.style.height = baseH + 'px';
  const k = Math.min(window.innerWidth / 1140, window.innerHeight / baseH);
  const x = Math.max(0, (window.innerWidth - 1140 * k) / 2);
  const y = Math.max(0, (window.innerHeight - baseH * k) / 2);
  frame.style.transform = `translate(${x}px, ${y}px) scale(${k})`;
}
function toggleKeys() {
  kbdOpen = !kbdOpen;
  kbStrip.style.display = kbdOpen ? 'block' : 'none';
  keysFoldBtn.textContent = kbdOpen ? '▼ KEYS' : '▲ KEYS';
  if (nfKeyboardFold) nfKeyboardFold(kbdOpen);   // host resizes the editor window; its resize event re-fits
  fit();
}

// ---- boot -----------------------------------------------------------------
buildFaceplate();
buildHeader();
buildRow1();
buildRow2();
buildKeys();
buildOverlays();

// relay -> UI: refresh whenever any bound parameter changes (host automation,
// preset recall, other views). Menus/util/penv drive their own updates.
[...sCache.values()].forEach((s) => s.valueChangedEvent.addListener(scheduleRefresh));
[...tCache.values()].forEach((s) => s.valueChangedEvent.addListener(scheduleRefresh));
[...cCache.values()].forEach((s) => { s.valueChangedEvent.addListener(scheduleRefresh); if (s.propertiesChangedEvent) s.propertiesChangedEvent.addListener(scheduleRefresh); });

window.addEventListener('keydown', (e) => { if (e.key === 'Escape') { menu = null; utilOpen = false; penvOpen = false; syncOverlays(); } });
window.addEventListener('resize', fit);

applyVersion();
loadPresetList();   // pull the processor-owned factory bank into the browser
syncOverlays();
refresh();
fit();
setInterval(animTick, 40);   // constellation / flow dots / meters
