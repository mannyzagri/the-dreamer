/* ============================================================================
 * The Dreamer — production front-end (framework-free, JUCE WebView ready)
 * ----------------------------------------------------------------------------
 *  editor.html loads:  style.css  →  app.js (this file, type="module")
 *
 *  ARCHITECTURE
 *   1. BRIDGE  — talks to the JUCE juce_gui_extra WebView relays. Falls back to
 *                an in-memory MOCK so this file also runs in a plain browser
 *                (open editor.html) for design QA. See README_INTEGRATION.md.
 *   2. PARAMS  — the full APVTS parameter map (ids, ranges, choices, defaults).
 *   3. WIDGETS — Knob / Slider / Toggle / Stepper / LcdMenu / Led factories that
 *                bind a DOM node to a Param handle (read on change, write on drag).
 *   4. PANEL   — builders that assemble the locked 1140×660 layout. Geometry is
 *                FROZEN — mirror ../The Dreamer GUI.png; never reflow on redraw.
 *
 *  The layout is intentionally hand-placed with the master's pixel constants.
 * ========================================================================== */

/* ---------------------------------------------------------------- DOM helpers */
const NS = 'http://www.w3.org/2000/svg';
function h(tag, cls, attrs, ...kids) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (attrs) for (const k in attrs) {
    if (k === 'style') e.style.cssText = attrs[k];
    else if (k.startsWith('on')) e.addEventListener(k.slice(2).toLowerCase(), attrs[k]);
    else if (attrs[k] != null) e.setAttribute(k, attrs[k]);
  }
  for (const c of kids.flat()) if (c != null) e.append(c.nodeType ? c : document.createTextNode(c));
  return e;
}
function s(tag, attrs, ...kids) {
  const e = document.createElementNS(NS, tag);
  if (attrs) for (const k in attrs) if (attrs[k] != null) e.setAttribute(k, attrs[k]);
  for (const c of kids.flat()) if (c != null) e.append(c);
  return e;
}
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

/* ============================================================ 1. BRIDGE ==== */
/* Prefers the official juce-framework-frontend relays (window.__JUCE__ present
 * in the WebView). getState(id) returns a uniform handle regardless of backend.
 * Continuous params are normalised 0..1; choice = index; toggle = bool.        */
const Bridge = (() => {
  const inJuce = typeof window !== 'undefined' && !!window.__JUCE__;
  let juce = null;                      // populated from window.Juce if present
  if (inJuce && window.Juce) juce = window.Juce;

  const mock = new Map();               // id -> {v, subs:Set}
  function mockState(id, def) {
    if (!mock.has(id)) mock.set(id, { v: def, subs: new Set() });
    const m = mock.get(id);
    return {
      get: () => m.v,
      set: v => { m.v = v; m.subs.forEach(f => f(v)); },
      sub: f => { m.subs.add(f); return () => m.subs.delete(f); },
    };
  }

  // Wrap a JUCE relay state (slider/toggle/comboBox) into our handle.
  function wrapSlider(id, def) {
    const st = juce.getSliderState(id);
    return {
      get: () => st.getNormalisedValue(),
      set: v => st.setNormalisedValue(v),
      sub: f => { st.valueChangedEvent.addListener(() => f(st.getNormalisedValue())); return () => {}; },
    };
  }
  function wrapToggle(id, def) {
    const st = juce.getToggleState(id);
    return { get: () => st.getValue(), set: v => st.setValue(!!v),
      sub: f => { st.valueChangedEvent.addListener(() => f(st.getValue())); return () => {}; } };
  }
  function wrapChoice(id, def) {
    const st = juce.getComboBoxState(id);
    return { get: () => st.getChoiceIndex(), set: v => st.setChoiceIndex(v | 0),
      sub: f => { st.valueChangedEvent.addListener(() => f(st.getChoiceIndex())); return () => {}; } };
  }

  return {
    live: inJuce && !!juce,
    state(kind, id, def) {
      if (this.live) {
        if (kind === 'toggle') return wrapToggle(id, def);
        if (kind === 'choice') return wrapChoice(id, def);
        return wrapSlider(id, def);
      }
      return mockState(id, def);
    },
    // Native processor functions (D3 scope, panic, presets, midi learn, user bank).
    // In the WebView these come from Juce.getNativeFunction(name); mocked otherwise.
    fn(name) {
      if (this.live && juce.getNativeFunction) return juce.getNativeFunction(name);
      return async (...a) => MOCK_FN[name] ? MOCK_FN[name](...a) : undefined;
    },
  };
})();

/* mock implementations of the D-series native functions (browser QA only) */
const MOCK_FN = {
  getScopeData: () => Array.from({ length: 2048 }, (_, i) => Math.sin(i / 9) * 0.4 * Math.random()),
  getLimiterGR: () => (Math.random() > 0.85 ? Math.random() * 3 : 0),
  panic: () => {},
  getPresetList: () => PRESETS.map((p, i) => ({ index: i, category: p[0], name: p[1], bank: 'FACTORY' })),
  getUserPresetList: () => USER_PRESETS.slice(),
  saveUserPreset: n => { USER_PRESETS.push({ name: n, category: 'USER', bank: 'USER' }); },
  renameUserPreset: (o, n) => { const p = USER_PRESETS.find(x => x.name === o); if (p) p.name = n; },
  deleteUserPreset: n => { const i = USER_PRESETS.findIndex(x => x.name === n); if (i >= 0) USER_PRESETS.splice(i, 1); },
  loadUserPreset: () => {},
  getWaveList: () => WAVES.map((w, i) => ({ index: i, category: w[0], name: w[1], bank: w[2] || '' })),
};

/* ============================================================ 2. PARAMS ==== */
const TONES = ['A', 'B', 'C', 'D'];
const TONE_COLORS = ['#D4547D', '#1C9E75', '#388ADE', '#BA7517'];
const TONE_CORE = [['#FFD4FC','#D4547D','#4A1D2C'],['#9CFFF5','#1C9E75','#0A3729'],
                   ['#B8FFFF','#388ADE','#14304E'],['#FFF596','#BA7517','#412908']];
const TONE_ANGLE = [90, 0, 220, 300];

const FTYPES  = ['LP 24','LP 12','BP','HP','LIQUID','CLASSIC','LADDER','NOTCH','COMB +','COMB -','N+LP','FORMANT','ALLPASS','DREAMPLN'];
const TVFTYPES = ['LP24','LP12','BP','HP'];
const SHAPES   = ['OFF','SOFT FOLD','HARD FOLD','SINE FOLD','ASYM','DRIVE'];
const AUXDESTS = ['PITCH','START','SHAPE','PAN','NOISE'];
const LFODESTS = ['PITCH','CUTOFF','SHAPE','LEVEL'];
const WAVE_SHAPES = ['TRI','SIN','SAW','SQR','S+H'];
const VECSHAPES = ['SAW','TRI','SIN','SQR','S+H'];
const VOICINGS = ['SINGLE','OCT','POWER','DREAMY'];
const SPREADS  = ['ADD9','MIN7','SUS2'];
const LOOPMODES = ['FWD','PINGPONG'];
const PLAYMODES = ['NORMAL','STRETCH'];
const MODFX = ['CHORUS','FLANGER','PHASER','ENSEMBLE'];
const DLYMODES = ['DIGITAL','TAPE','PONG'];
const REVTYPES = ['ROOM','HALL','PLATE'];
const MSRC = ['G-LFO 1','VEC PHS','AUX','VELO','WHEEL','G-LFO 2','G-AUX'];
const MDST = ['PITCH','CUT 1','CUT 2','MORPH','SHAPE','VEC PHS','PAN','NOISE','FX PARAM','LOOP RATE'];
const MODFXFOCUS = ['DELAY','WIDTH','FEEDBK','TONE'];
const DLYFOCUS = ['WOW','FLUTTER','TONE','DUCK'];
const REVFOCUS = ['PREDLY','WIDTH','LO CUT','HI CUT'];
const LOFIFOCUS = ['BITS','SRATE','COMPAND','ALIAS'];
const TALKFOCUS = ['VOWEL-A','VOWEL-B','MORPH','SENS'];
const SYNCDIVS = ['4/1','2/1','1/1','1/2','1/2T','1/4','1/4T','1/8','1/8T','1/16','1/16T','1/32'];

/* Wave ROM: 218 = 78 cycle + 130 loop (ENS) + 10 hit (SHOT).
 * PRODUCTION: replace this with getWaveList() from the processor — do NOT hardcode. */
const WAVES = (() => {
  const cyc = {
    PAD:['SOFTSAW 1','SOFTSAW 2','HOLLOW 1','HOLLOW 3','TANNERIN','BREATH 1','GLASS ORG 1','SINHARM 1','SOFTSAW 3','HOLLOW 2','BREATH 2','GLASS ORG 2','SINHARM 2','SPECTRA','HAZE','NEBULA'],
    STR:['STRINGBOX 1','VIOLIN 1','CELLO 2','STRINGBOX 2','VIOLIN 2','CELLO 1','STRINGBOX 3','ARCO'],
    VOX:['VOICE 1','VOICE 4','VOICE BRT 1','VOICE 2','VOICE 3','VOICE BRT 2','GHOST VX','AAH','OOH','MMH'],
    BELL:['FM BELL 1','FM BELL 4','EP TINE 1','DIGIBELL 1','CHIME 2','FM BELL 2','FM BELL 3','EP TINE 2','DIGIBELL 2','CHIME 1','GLASS 1','GLASS 2','MUSICBOX','CELESTA'],
    MTL:['SPECTRUM 4','RAWMETAL 2','BITHASH 2','CLANG 1','SPECTRUM 1','SPECTRUM 2','RAWMETAL 1','BITHASH 1','SPECTRUM 3','RAWMETAL 3','BITHASH 3','SPECTRUM 5'],
    BAS:['SAW','SQUARE','TRIANGLE','SINE','PULSE 25','PULSE 50','RUBBER 1','RUBBER 2','ACID PULS','SUB SINE','DEEP HSE','PICK BS','FRETLESS','ORBIT BS','PHATT BS','303 SQR','MOOG BS','FM BS'],
  };
  const out = [];
  for (const cat in cyc) for (const nm of cyc[cat]) out.push([cat, nm]);
  ['SOLINA','OB STR','JUNO ENS','SUPERSAW','CHOIR ENS','BRASS ENS','TAPE STR','OCT STR','PWM ENS','VOX ENS','PAD ENS','SYN ENS','ANALOG ENS']
    .forEach(b => { for (let i = 1; i <= 10; i++) out.push(['ENS', b + ' ' + i, 'ENS']); });
  ['PIZZ','ANVIL','ICEHIT','ORBIT HIT','CLANG','TOM HIT','ZAP','GLASS HIT','METAL HIT','KICK']
    .forEach(nm => out.push(['HIT', nm, 'SHOT']));
  return out;
})();

const PRESETS = [
  ['PAD','ETHEREAL DAWN'],['PAD','SLOW MEMORY'],['PAD','GHOST HARBOR'],['PAD','VIOLET SLEEP'],
  ['PAD','TIDE GARDEN'],['PAD','HALF LIGHT'],['PAD','AURORA VEIL'],['PAD','DEEP FIELD'],
  ['SPLIT','GLASS RIVER'],['SPLIT','NIGHT DRIVE 84'],['SPLIT','BASS & BELLS'],['SPLIT','SPLIT HORIZON'],
  ['BELL','TINE CATHEDRAL'],['BELL','MUSICBOX MOON'],['BELL','COLD CHIMES'],['BELL','CRYSTAL RAIN'],
  ['BELL','DIGITAL CAROL'],['STR','ORBITAL STRINGS'],['STR','SOLINA FIELDS'],['STR','TAPE ENSEMBLE'],
  ['STR','CINEMA ARCO'],['VOX','CHOIR OF WIRES'],['VOX','BREATH MACHINE'],['VOX','GHOST CHOIR'],
  ['VOX','ANGEL AAH'],['BASS','RUBBER ORBIT'],['BASS','SUB DREAMS'],['BASS','ACID COMET'],
  ['BASS','PHATT SUB'],['BASS','303 REVIVAL'],['LEAD','SYNC COMET'],['LEAD','12-BIT STAR'],
  ['LEAD','SOLO WIRE'],['LEAD','NEON LINE'],['KEY','EPIANO HAZE'],['KEY','ORGAN NEBULA'],
  ['KEY','DIGI RHODES'],['KEY','GLASS KEYS'],['SFX','RE-ENTRY'],['SFX','STATIC BLOOM'],
  ['SFX','ORBIT DECAY'],['SFX','DREAM STATE'],['SFX','SIGNAL LOST'],['ARP','PULSE GRID'],
  ['ARP','STARFIELD SEQ'],['ARP','NIGHT MOTION'],['INIT','INIT PATCH'],
];
const USER_PRESETS = [{ name: 'MY ORBIT PAD', category: 'USER', bank: 'USER' },
                      { name: 'LIVE LEAD 1',  category: 'USER', bank: 'USER' }];

/* Per-tone param defaults (normalised). Ids are `<key>_<a|b|c|d>` in the APVTS. */
const TONE_DEFAULTS = {
  oct: .5, semi: .5, fine: .5, start: 0, level: .8, velo: .4, pan: .5, shape: 0, shapeDepth: 0,
  tvfType: 0, dir: .5, vint: .6, noise: 0, noiseCol: .5, startRnd: false, voicing: 0, spread: 0,
  detVoices: 0, detAmt: 0, loopMode: 0, hitPlay: 0, hitStretch: .5, hitTrim: .5,
  loopRate: .5, loopSync: false, loopBeats: 5, loopVari: false,
  l1r: .4, l1d: .15, l1sync: false, l1dest: 0, l1shape: 0,
  l2r: .25, l2d: 0, l2sync: false, l2dest: 1, l2shape: 2, auxAmt: .5, auxDest: 0,
  cut: .5, res: .25, env: .5, kf: .5,
  fa: .05, fd: .6, fs: .5, fr: .55, aa: .2, ad: .7, as: .8, ar: .65, xa: 0, xd: .4, xs: 0, xr: .3,
  ampOvr: false, filtOvr: false, auxOvr: false,
};
/* map UI key -> APVTS id stem (kept explicit so ids match DSP_BUILD/Params.h) */
const TONE_ID = {
  oct:'oct', semi:'semi', fine:'fine', start:'start', level:'level', velo:'velo', pan:'pan',
  shape:'shape', shapeDepth:'shape_depth', tvfType:'tvf_type', dir:'dir', vint:'vint',
  noise:'noise', noiseCol:'noise_color', startRnd:'start_random', voicing:'voicing', spread:'dreamy_spread',
  detVoices:'detune_voices', detAmt:'detune_amount', loopMode:'loop_mode', hitPlay:'hit_play',
  hitStretch:'hit_stretch', hitTrim:'hit_pitchtrim', loopRate:'loop_rate', loopSync:'loop_rate_sync',
  loopBeats:'loop_rate_beats', loopVari:'loop_varispeed',
  l1r:'lfo1_rate', l1d:'lfo1_depth', l1sync:'lfo1_sync', l1dest:'lfo1_dest', l1shape:'lfo1_shape',
  l2r:'lfo2_rate', l2d:'lfo2_depth', l2sync:'lfo2_sync', l2dest:'lfo2_dest', l2shape:'lfo2_shape',
  auxAmt:'aux_amt', auxDest:'aux_dest', cut:'tvf_cut', res:'tvf_res', env:'tvf_env', kf:'tvf_kf',
  fa:'tvf_a', fd:'tvf_d', fs:'tvf_s', fr:'tvf_r', aa:'tva_a', ad:'tva_d', as:'tva_s', ar:'tva_r',
  xa:'aux_a', xd:'aux_d', xs:'aux_s', xr:'aux_r', on:'on', wave:'wave',
  ampOvr:'amp_ovr', filtOvr:'flt_ovr', auxOvr:'aux_ovr',
};
const GLOBAL_DEFAULTS = {
  master: .78, f1Cut: .52, f1Res: .35, f1Env: .5, f2Cut: .7, f2Res: .2, f2Env: .5, f2Morph: .4,
  route: 0, fbal: .5, f1Type: 6, f2Type: 13, orbRate: 0, vphase: .12, vecShape: 2, orbit: true, venv: false,
  glfoRate: .4, glfoWave: 0, glfoSync: false, glfo2Rate: .25, glfo2Wave: 1, glfo2Sync: false,
  mfxType: 0, mfxRate: .3, mfxDepth: .5, mfxMix: .45, mfxParam: .5, mfxOn: true, mfxFocus: 0,
  dlyMode: 0, dTime: .5, dFb: .35, dMix: .3, dParam: .4, dlyOn: true, dlySync: false, dlyFocus: 0,
  revType: 1, rSize: .6, rDamp: .4, rMix: .35, rParam: .5, revOn: true, revFocus: 0,
  lofiOn: false, lofiParam: .5, lofiFocus: 0, widthOn: true, widthAmt: .55, haas: .3, bassMono: false,
  talkOn: false, talkParam: .5, talkFocus: 0, prePost: 0, limiter_on: true,
  gEnvA: .5, gEnvD: .5, gEnvS: .5, gEnvR: .5, gCutoff: .5, gRes: .5, gOctave: .5, globOffset: false,
  penvStart: 0, penvEnd: .5, penvTime: .4, penvLoop: false,
  lofiBits: .5, lofiSrate: .5, lofiCompand: .5, lofiAlias: .5, orbitVoice: 0,
  gAmpA: .10, gAmpD: .30, gAmpS: .80, gAmpR: .35,
  gFltA: .04, gFltD: .45, gFltS: .30, gFltR: .50,
  gAuxA: .20, gAuxD: .40, gAuxS: .60, gAuxR: .55,
};
const GLOBAL_ID = {
  master:'master', f1Cut:'flt1_cut', f1Res:'flt1_res', f1Env:'flt1_env', f2Cut:'flt2_cut',
  f2Res:'flt2_res', f2Env:'flt2_env', f2Morph:'flt2_morph', route:'flt_route', fbal:'flt_bal',
  f1Type:'flt1_type', f2Type:'flt2_type', orbRate:'vec_orbit_rate', vphase:'vec_phase',
  vecShape:'vec_orbit_shape', orbit:'vec_orbit_on', venv:'vec_penv_on', glfoRate:'lfo_rate',
  glfoWave:'lfo_shape', glfoSync:'lfo_sync', glfo2Rate:'lfo2_rate', glfo2Wave:'lfo2_shape', glfo2Sync:'lfo2_sync', mfxType:'modfx_type', mfxRate:'modfx_rate',
  mfxDepth:'modfx_depth', mfxMix:'modfx_mix', mfxParam:'modfx_param', mfxOn:'modfx_on', mfxFocus:'modfx_pfocus',
  dlyMode:'dly_mode', dTime:'dly_time', dFb:'dly_fb', dMix:'dly_mix', dParam:'dly_param', dlyOn:'dly_on',
  dlySync:'dly_sync', dlyFocus:'dly_pfocus', revType:'rev_type', rSize:'rev_size', rDamp:'rev_damp',
  rMix:'rev_mix', rParam:'rev_param', revOn:'rev_on', revFocus:'rev_pfocus', lofiOn:'lofi_on',
  lofiParam:'lofi_param', lofiFocus:'lofi_pfocus', widthOn:'width_on', widthAmt:'width',
  haas:'width_haas', bassMono:'width_bassmono', talkOn:'talk_on', talkParam:'talk_param',
  talkFocus:'talk_pfocus', prePost:'fx_prepost', limiter_on:'limiter_on',
  gEnvA:'g_env_a', gEnvD:'g_env_d', gEnvS:'g_env_s', gEnvR:'g_env_r', gCutoff:'g_cutoff', gRes:'g_res',
  gOctave:'g_octave', globOffset:'ui_global_offset',
  penvStart:'vec_penv_start', penvEnd:'vec_penv_end', penvTime:'vec_penv_time', penvLoop:'vec_penv_loop',
  lofiBits:'lofi_bits', lofiSrate:'lofi_srate', lofiCompand:'lofi_compand', lofiAlias:'lofi_alias', orbitVoice:'vec_orbit_voice',
  gAmpA:'gamp_env_a', gAmpD:'gamp_env_d', gAmpS:'gamp_env_s', gAmpR:'gamp_env_r',
  gFltA:'gflt_env_a', gFltD:'gflt_env_d', gFltS:'gflt_env_s', gFltR:'gflt_env_r',
  gAuxA:'gaux_env_a', gAuxD:'gaux_env_d', gAuxS:'gaux_env_s', gAuxR:'gaux_env_r',
};
const KIND = {  // relay kind per param key (default = slider/continuous)
  route:'toggle', orbit:'toggle', venv:'toggle', glfoSync:'toggle', mfxOn:'toggle', dlyOn:'toggle',
  dlySync:'toggle', revOn:'toggle', lofiOn:'toggle', widthOn:'toggle', bassMono:'toggle', talkOn:'toggle',
  prePost:'toggle', limiter_on:'toggle', globOffset:'toggle', penvLoop:'toggle', orbitVoice:'toggle', glfo2Sync:'toggle',
  f1Type:'choice', f2Type:'choice', vecShape:'choice', glfoWave:'choice', glfo2Wave:'choice', mfxType:'choice',
  dlyMode:'choice', revType:'choice', mfxFocus:'choice', dlyFocus:'choice', revFocus:'choice',
  lofiFocus:'choice', talkFocus:'choice',
};
const TONE_KIND = {
  startRnd:'toggle', l1sync:'toggle', l2sync:'toggle', loopSync:'toggle', loopVari:'toggle', on:'toggle',
  shape:'choice', tvfType:'choice', voicing:'choice', spread:'choice', l1dest:'choice', l2dest:'choice',
  l1shape:'choice', l2shape:'choice', auxDest:'choice', loopMode:'choice', hitPlay:'choice',
  loopBeats:'choice', wave:'choice', detVoices:'choice',
  ampOvr:'toggle', filtOvr:'toggle', auxOvr:'toggle',
};

/* Param handle cache. glob(key) / tone(key, toneIdx). */
const _cache = new Map();
function glob(key) {
  const id = GLOBAL_ID[key];
  if (!_cache.has(id)) _cache.set(id, Bridge.state(KIND[key] || 'slider', id, GLOBAL_DEFAULTS[key]));
  return _cache.get(id);
}
const WAVE_DEFAULTS = [3, 19, 27, 37];   // A/B/C/D initial wave index
function toneDefault(key, ti) {
  if (key === 'wave') return WAVE_DEFAULTS[ti];
  if (key === 'on') return true;
  return TONE_DEFAULTS[key];
}
function tone(key, ti) {
  const id = TONE_ID[key] + '_' + TONES[ti].toLowerCase();
  if (!_cache.has(id)) _cache.set(id, Bridge.state(TONE_KIND[key] || 'slider', id, toneDefault(key, ti)));
  return _cache.get(id);
}
/* MOD MATRIX slots are real APVTS params: mtx{1..3}_src / _dst / _amt. */
const MTX_SRC_DEF = [0, 1, 2], MTX_DST_DEF = [3, 4, 0], MTX_AMT_DEF = [.8, .35, .5];
function mtx(n, field) {
  const id = 'mtx' + (n + 1) + '_' + field;
  if (!_cache.has(id)) {
    const kind = field === 'amt' ? 'slider' : 'choice';
    const def = field === 'src' ? MTX_SRC_DEF[n] : field === 'dst' ? MTX_DST_DEF[n] : MTX_AMT_DEF[n];
    _cache.set(id, Bridge.state(kind, id, def));
  }
  return _cache.get(id);
}

/* UI-only state (not APVTS): selected tone, open overlay, matrix rows */
const UI = { sel: 0, overlay: null, touched: { label: 'MORPH', disp: null, v: .52, bip: false },
  matrix: [{ s: 0, d: 3, a: .8 }, { s: 1, d: 4, a: .35 }, { s: 2, d: 0, a: .5 }],
  penv: { start: 0, end: .5, time: .4, loop: false }, presetSel: { bank: 'FACTORY', i: 0 },
  renameBuf: '', preset: 0, midiLearn: false, kbdOpen: false, scale: 1, envDest: 'AMP', envAll: false };
const listeners = new Set();               // components re-read UI-derived text
const notify = () => listeners.forEach(f => f());
function touch(label, v, bip, disp) { UI.touched = { label, v, bip, disp }; notify(); }

/* ============================================================ 3. WIDGETS === */
function fmtTime(v) { const ms = Math.round(Math.pow(10, v * 4.3)); return ms >= 1000 ? (+(ms / 1000).toFixed(ms < 10000 ? 2 : 1)) + ' s' : ms + ' ms'; }
const fmtPct = v => Math.round(v * 100) + '%';
const fmtBip = v => (v >= .5 ? '+' : '') + Math.round((v - .5) * 126);

/* Knob bound to a continuous Param handle. opts: {size,color,def,bip,label,fmt} */
function Knob(handle, label, opts = {}) {
  const size = opts.size || 34;
  const rot = document.createElement('div'); rot.className = 'knob-rot';
  const ptr = h('div', 'knob-ptr', { style: `--pc:${opts.color || 'var(--ptr-white)'}` });
  rot.append(ptr);
  const cap = h('div', 'knob-cap', null, h('div', 'knob-face'), rot);
  const knob = h('div', 'knob', { style: `--s:${size}px` }, cap);
  const paint = v => { rot.style.setProperty('--rot', Math.round(-135 + v * 270) + 'deg'); };
  paint(handle.get()); handle.sub(paint);
  dragBehaviour(knob, handle, { label, def: opts.def, bip: opts.bip, fmt: opts.fmt });
  const cell = h('div', 'knob-cell', { style: `width:${size + 12}px` }, knob,
    label != null ? h('div', 'lbl', null, label) : null);
  cell._knob = knob; cell._setLabel = t => { if (cell.lastChild) cell.lastChild.textContent = t; };
  return cell;
}

function Slider(handle, label, opts = {}) {
  const H = opts.h || 62;
  const cap = h('div', 'slider-cap');
  const track = h('div', 'slider', { style: `--h:${H}px` }, cap);
  const paint = v => cap.style.setProperty('--bot', Math.round(v * (H - 9)) + 'px');
  paint(handle.get()); handle.sub(paint);
  dragBehaviour(track, handle, { label, def: opts.def, fmt: opts.fmt });
  return h('div', 'slider-cell', null, track, label != null ? h('div', 'lbl-sm', null, label) : null);
}

/* vertical-drag / alt-reset / double-click-reset / bipolar-detent, + MIDI learn */
function dragBehaviour(node, handle, o) {
  node.addEventListener('pointerdown', e => {
    if (UI.midiLearn) { e.preventDefault(); touch('LEARN ' + o.label, 1); return; }
    e.preventDefault();
    if (e.altKey && o.def != null) { handle.set(o.def); touch(o.label, o.def, o.bip, o.fmt && o.fmt(o.def)); return; }
    const sy = e.clientY, sv = handle.get(); let moved = false;
    const mv = ev => {
      if (Math.abs(ev.clientY - sy) > 2) moved = true;
      let v = clamp(sv + (sy - ev.clientY) / 140, 0, 1);
      if (o.bip && Math.abs(v - .5) < .035) v = .5;
      handle.set(v); touch(o.label, v, o.bip, o.fmt && o.fmt(v));
    };
    const up = () => {
      removeEventListener('pointermove', mv); removeEventListener('pointerup', up);
      if (!moved && o.def != null) {
        const now = Date.now();
        if (node._lc && now - node._lc < 350) { handle.set(o.def); touch(o.label, o.def, o.bip, o.fmt && o.fmt(o.def)); node._lc = 0; }
        else node._lc = now;
      }
    };
    addEventListener('pointermove', mv); addEventListener('pointerup', up);
  });
}

function Led(handle) {
  const led = h('div', 'led');
  const paint = v => led.classList.toggle('on', !!v);
  paint(handle.get()); handle.sub(paint);
  return led;
}
/* Toggle: LED + button. handle is a 'toggle' (bool) or choice-of-2 (loopMode/hitPlay). */
function Toggle(handle, text, opts = {}) {
  const btn = h('div', 'btn', { style: opts.style || 'width:32px;height:15px',
    onclick: () => { const v = handle.get(); const nv = typeof v === 'boolean' ? !v : (1 - v); handle.set(nv); touch(opts.label || text, nv ? 1 : 0); } });
  const paint = v => { btn.textContent = opts.choices ? opts.choices[+v] : text; btn.classList.toggle('on', !!v); };
  paint(handle.get()); handle.sub(paint);
  return btn;
}
/* Stepper for a choice param, with optional LCD readout between arrows. */
function Stepper(handle, arr, opts = {}) {
  const read = h('div', opts.lcd ? 'lcd click' : 'btn', { style: opts.readStyle || 'width:40px;height:18px',
    onclick: opts.onLcd });
  const dec = h('div', 'step', { style: opts.arrowStyle, onclick: () => step(-1) }, '\u2039');
  const inc = h('div', 'step', { style: opts.arrowStyle, onclick: () => step(1) }, '\u203a');
  function step(d) { let v = handle.get() + d; if (opts.clamp) v = clamp(v, opts.clamp[0], opts.clamp[1]); else v = (v + arr.length) % arr.length; handle.set(v); touch(opts.label || 'STEP', v / arr.length); }
  const paint = v => read.textContent = opts.map ? opts.map(v) : arr[v];
  paint(handle.get()); handle.sub(paint);
  return h('div', 'row', { style: 'gap:3px' }, dec, read, inc);
}
/* LCD that opens a menu overlay (wave/type select). */
function LcdMenu(handle, arr, title, style, tagged) {
  const lcd = h('div', 'lcd click', { style, onclick: () => openMenu(title, arr, handle, tagged) });
  const paint = v => lcd.textContent = Array.isArray(arr[v]) ? (arr[v][0] + ' > ' + arr[v][1]) : arr[v];
  paint(handle.get()); handle.sub(paint);
  return lcd;
}

/* generic scrolling menu overlay */
function openMenu(title, arr, handle, tagged) {
  const cur = handle.get();
  const rows = arr.map((it, i) => {
    const cat = Array.isArray(it) ? it[0] : '';
    const nm = (Array.isArray(it) ? it[1] : it) + (tagged && Array.isArray(it) && it[2] ? ' [' + it[2] + ']' : '');
    return h('div', 'menu-row' + (i === cur ? ' cur' : ''), { onclick: () => { handle.set(i); closeOverlay(); } },
      h('span', null, { style: 'width:20px' }, String(i + 1).padStart(2, '0')),
      h('span', null, { style: 'width:44px' }, cat),
      h('span', null, null, (i === cur ? '\u25B8 ' : '') + nm));
  });
  const box = h('div', 'menu', { onclick: e => e.stopPropagation() },
    h('div', 'menu-head', null, h('span', null, null, title), h('span', null, null, 'ESC=EXIT')),
    h('div', 'menu-list', null, ...rows));
  showOverlay(box);
}
let overlayEl = null;
function showOverlay(child, cls) {
  closeOverlay();
  overlayEl = h('div', 'overlay' + (cls ? ' ' + cls : ''), { onclick: closeOverlay }, child);
  document.querySelector('.panel').append(overlayEl);
}
function closeOverlay() { if (overlayEl) { overlayEl.remove(); overlayEl = null; } }
addEventListener('keydown', e => { if (e.key === 'Escape') closeOverlay(); });

/* small label helpers */
const L  = (t, c) => h('div', 'lbl', c ? { style: `color:${c}` } : null, t);
const LH = t => h('div', 'lbl-hi', null, t);

export { h, s };   /* (allow unit importing if desired) */

/* ============================================================ 4. PANEL ===== */
/* built in build() at the bottom, after the DOM is ready. */

function ledToggle(handle, label, boxW = 26, boxH = 14) {
  return h('div', 'row', { style: 'gap:5px' }, Led(handle),
    h('div', 'btn', { style: `width:${boxW}px;height:${boxH}px`,
      onclick: () => { const v = !handle.get(); handle.set(v); touch(label, v ? 1 : 0); } }));
}

function buildHeader() {
  const logo = h('div', 'col', { style: 'gap:2px;align-items:center;margin-left:26px' },
    h('div', null, { style: "font:400 19px var(--f-logo);color:var(--silk-hi);letter-spacing:.14em;white-space:nowrap" }, 'THE DREAMER'),
    h('div', null, { style: "font:600 7.5px var(--f-silk);color:var(--silk);letter-spacing:.34em" }, '12-BIT ROM VECTOR SYNTHESIZER'));

  // preset LCD with live spectrum
  const spec = h('div', 'row', { style: 'align-items:flex-end;gap:1px;height:30px' });
  const specBars = Array.from({ length: 22 }, () => { const b = h('div', null, { style: 'width:2px;background:var(--lcd-ink);opacity:.9;height:3px' }); spec.append(b); return b; });
  const presetName = h('span', null, { style: 'cursor:pointer', onclick: openPresetBrowser });
  const toneName = h('span');
  const touchedLine = h('span', null, { style: 'min-width:130px' });
  const meter = h('span', null, { style: 'display:flex;gap:2px' });
  const meterCells = Array.from({ length: 14 }, () => { const c = h('span', null, { style: 'width:6px;height:9px;background:#1c1a10' }); meter.append(c); return c; });
  const lcd = h('div', null, { style: 'width:520px;height:44px;background:var(--lcd-bg);border-radius:3px;box-shadow:inset 0 2px 8px rgba(0,0,0,.9),0 1px 0 rgba(201,205,242,.15);padding:5px 10px;display:flex;align-items:center;gap:12px' },
    spec,
    h('div', 'grow col', { style: 'gap:3px;overflow:hidden' },
      h('div', 'row', { style: 'justify-content:space-between;font:800 12px var(--f-lcd);color:var(--lcd-ink);letter-spacing:.04em;white-space:nowrap' }, presetName, toneName),
      h('div', 'row', { style: 'gap:8px;font:800 11px var(--f-lcd);color:var(--lcd-ink);white-space:nowrap' }, touchedLine, meter)));

  const steppers = h('div', 'col', { style: 'gap:3px;margin-right:6px' },
    h('div', 'step', { style: 'width:22px;height:19px', onclick: () => { UI.preset = (UI.preset + PRESETS.length - 1) % PRESETS.length; UI.presetSel = { bank: 'FACTORY', i: UI.preset }; Bridge.fn('loadPreset')(UI.preset); refreshHeader(); } }, '\u25B2'),
    h('div', 'step', { style: 'width:22px;height:19px', onclick: () => { UI.preset = (UI.preset + 1) % PRESETS.length; UI.presetSel = { bank: 'FACTORY', i: UI.preset }; Bridge.fn('loadPreset')(UI.preset); refreshHeader(); } }, '\u25BC'));

  // right cluster: MIDI learn, meters, master, LIM/panic, power
  const midiLed = h('div', 'led');
  const midiBtn = h('div', 'btn', { style: 'width:34px;height:16px', onclick: () => { UI.midiLearn = !UI.midiLearn; midiLed.classList.toggle('on', UI.midiLearn); document.querySelector('.panel').classList.toggle('learn', UI.midiLearn); touch(UI.midiLearn ? 'MIDI LEARN ON' : 'MIDI LEARN OFF', UI.midiLearn ? 1 : 0); } }, 'MIDI');
  const midi = h('div', 'col', { style: 'align-items:center;gap:3px' }, midiLed, midiBtn, h('div', 'lbl-sm', null, 'LEARN'));

  const mL = h('div', null, { style: 'position:absolute;left:0;right:0;bottom:0;background:linear-gradient(180deg,#ff2b3e 0%,#ffd23f 20%,#ffd23f 100%);height:0%' });
  const mR = h('div', null, { style: 'position:absolute;left:0;right:0;bottom:0;background:linear-gradient(180deg,#ff2b3e 0%,#ffd23f 20%,#ffd23f 100%);height:0%' });
  const meterBar = fill => h('div', null, { style: 'width:7px;height:34px;background:var(--lcd-bg);border-radius:1px;box-shadow:inset 0 1px 3px #000;position:relative;overflow:hidden' }, fill);
  const meters = h('div', 'row', { style: 'gap:3px;align-items:flex-end;height:40px' },
    h('div', 'col', { style: 'align-items:center;gap:2px' }, meterBar(mL), h('div', 'lbl-sm', null, 'L')),
    h('div', 'col', { style: 'align-items:center;gap:2px' }, meterBar(mR), h('div', 'lbl-sm', null, 'R')));

  const master = Knob(glob('master'), 'MASTER', { size: 40, def: GLOBAL_DEFAULTS.master });

  const limLed = h('div', 'led');
  const panic = h('div', null, { style: 'width:44px;height:26px;background:var(--btn);border:1px solid #7a2130;border-radius:3px;cursor:pointer;color:#ff8a97;font:700 7px var(--f-silk);display:flex;flex-direction:column;align-items:center;justify-content:center;line-height:1.05',
    onpointerdown: () => { Bridge.fn('panic')(); touch('SOUND OFF', 1); } }, h('span', null, null, 'SOUND'), h('span', null, null, 'OFF'));
  const lim = h('div', 'col', { style: 'align-items:center;gap:3px' }, limLed, h('div', 'lbl-sm', null, 'LIM'), panic);

  const power = h('div', 'col', { style: 'align-items:center;gap:4px;margin-right:26px' },
    h('div', null, { style: 'width:8px;height:8px;border-radius:50%;background:#ff2b3e;box-shadow:0 0 8px rgba(255,43,62,.8)' }),
    h('div', 'lbl-sm', null, 'POWER'));

  refreshHeader = () => {
    presetName.textContent = 'P' + String(UI.preset + 1).padStart(3, '0') + ' ' + PRESETS[UI.preset][1];
    toneName.textContent = 'TONE ' + TONES[UI.sel];
    const t = UI.touched;
    touchedLine.textContent = t.label + ' ' + (t.disp != null ? t.disp : (t.bip ? fmtBip(t.v) : String(Math.round(t.v * 127)).padStart(3, ' ')));
    const f = Math.round(t.v * 14); meterCells.forEach((c, i) => c.style.background = i < f ? '#ffd23f' : '#1c1a10');
  };
  refreshHeader(); listeners.add(refreshHeader);
  HEADER_ANIM = { specBars, mL, mR, limLed };

  return h('div', null, { style: 'position:relative;height:60px;background:var(--panel-header);border-bottom:1px solid var(--frame);display:flex;align-items:center;padding:0 16px;gap:16px' },
    logo, h('div', 'grow row', { style: 'justify-content:center' }, steppers, lcd), midi, meters, master, lim, power);
}
let refreshHeader = () => {}, HEADER_ANIM = null;

/* ---- TONES mixer ---------------------------------------------------------- */
function buildTones() {
  const cols = TONES.map((name, i) => {
    const col = h('div', 'col', { style: 'align-items:center;gap:5px' });
    const selLed = h('div', 'led');
    const onLed = h('div', 'led');
    const setSelDim = () => col.style.opacity = tone('on', i).get() ? 1 : .55;
    tone('on', i).sub(v => { onLed.classList.toggle('on', !!v); setSelDim(); });
    const paintSel = () => selLed.classList.toggle('on', UI.sel === i);
    listeners.add(paintSel);
    onLed.classList.toggle('on', !!tone('on', i).get()); setSelDim(); paintSel();
    col.append(
      h('div', null, { style: `font:700 11px var(--f-silk);color:${TONE_COLORS[i]};letter-spacing:.08em` }, name),
      selLed,
      h('div', 'btn', { style: 'width:20px;height:14px', onclick: () => { UI.sel = i; rebuildToneEdit(); notify(); } }),
      Slider(tone('level', i), null, { h: 130, def: .8 }),
      (() => { const k = Knob(tone('pan', i), 'PAN', { size: 24, bip: true, def: .5 }); return k; })(),
      onLed,
      h('div', 'btn', { style: 'width:20px;height:14px', onclick: () => { const v = !tone('on', i).get(); tone('on', i).set(v); touch('TONE ' + name, v ? 1 : 0); } }),
      h('div', 'lbl-sm', null, 'ON'));
    // insert PAN label
    col.insertBefore(h('div', 'lbl-sm', null, 'PAN'), onLed);
    return col;
  });
  return grp('TONES', 'padding:16px 6px 8px',
    h('div', 'row', { style: 'gap:4px;width:100%;justify-content:space-around' }, ...cols));
}

/* ---- generic group frame -------------------------------------------------- */
function grp(title, pad, ...kids) {
  return h('div', 'group', { style: (pad || 'padding:16px 12px 8px') }, h('div', 'group-title', null, title), ...kids);
}

/* the tone-edit block is rebuilt when the selected tone changes */
let toneEditHost = null;
function rebuildToneEdit() {
  const fresh = buildToneEdit();
  if (toneEditHost && toneEditHost.parentNode) toneEditHost.replaceWith(fresh);
  toneEditHost = fresh;
}
function buildToneEdit() {
  const i = UI.sel, col = TONE_COLORS[i];
  const P = k => tone(k, i);
  const w = () => WAVES[P('wave').get()];
  const isENS = () => w()[2] === 'ENS', isSHOT = () => w()[2] === 'SHOT';

  // ---- WAVE / SHAPE / PLAY / LOOP row
  const waveLcd = LcdMenu(P('wave'), WAVES, 'WAVE SELECT \u2014 TONE ' + TONES[i], 'width:170px;height:20px', true);
  const waveDec = h('div', 'step', { onclick: () => cyc(P('wave'), WAVES.length, -1) }, '\u2039');
  const waveInc = h('div', 'step', { onclick: () => cyc(P('wave'), WAVES.length, 1) }, '\u203a');
  const shapeLcd = LcdMenu(P('shape'), SHAPES, 'SHAPER \u2014 TONE ' + TONES[i], 'width:96px;height:20px');
  const shapeDec = h('div', 'step', { onclick: () => cyc(P('shape'), SHAPES.length, -1) }, '\u2039');
  const shapeInc = h('div', 'step', { onclick: () => cyc(P('shape'), SHAPES.length, 1) }, '\u203a');

  const playLed = Led(P('hitPlay'));
  const playBtn = h('div', 'btn', { style: 'width:66px;height:20px', onclick: () => { const v = 1 - P('hitPlay').get(); P('hitPlay').set(v); touch('PLAY MODE', v); rebuildToneEdit(); } });
  const paintPlay = () => playBtn.textContent = PLAYMODES[P('hitPlay').get()]; paintPlay(); P('hitPlay').sub(paintPlay);
  const playGrp = h('div', 'row', { style: 'gap:5px' }, playLed, L('PLAY'), playBtn);

  const loopGrp = h('div', 'row', { style: 'gap:5px' });
  if (isENS()) {
    const lmLed = Led(P('loopMode'));
    const lmBtn = h('div', 'btn', { style: 'width:66px;height:20px', onclick: () => { const v = 1 - P('loopMode').get(); P('loopMode').set(v); touch('LOOP MODE', v); } });
    const paint = () => lmBtn.textContent = LOOPMODES[P('loopMode').get()]; paint(); P('loopMode').sub(paint);
    loopGrp.append(lmLed, L('LOOP'), lmBtn);
  }
  const waveRow = h('div', 'row', { style: 'gap:6px' },
    h('div', 'lbl', { style: `color:${col};width:42px` }, 'WAVE'), waveLcd, waveDec, waveInc, h('div', null, { style: 'width:48px' }),
    L('SHAPE', col), shapeLcd, shapeDec, shapeInc, h('div', null, { style: 'width:24px' }), playGrp, loopGrp);

  // ---- knob row: OCTAVE SEMI FINE START VELOCITY [RND] | stretch/loop compartment | SHAPE DEPTH NOISE NOISE COLOR
  const semiFmt = v => { const n = Math.round((v - .5) * 24); return (n >= 0 ? '+' : '') + n + ' st'; };
  const leftKnobs = h('div', 'row', { style: 'gap:4px;width:288px' },
    Knob(P('oct'), 'OCTAVE', { def: .5 }), Knob(P('semi'), 'SEMI', { bip: true, def: .5, fmt: semiFmt }),
    Knob(P('fine'), 'FINE', { bip: true, def: .5 }), Knob(P('start'), 'START', { def: 0 }),
    Knob(P('velo'), 'VELOCITY', { def: .4 }),
    (() => { const rnd = P('startRnd'); const led = Led(rnd);
      const b = h('div', 'btn', { style: 'width:22px;height:14px', onclick: () => { const v = !rnd.get(); rnd.set(v); touch('START RANDOM', v ? 1 : 0); } });
      return h('div', 'col', { style: 'align-items:center;gap:3px;width:26px;padding-top:4px' }, led, b, h('div', 'lbl-sm', null, 'RND')); })());

  // middle compartment
  let comp;
  if (isSHOT() && P('hitPlay').get() === 1)
    comp = h('div', 'row', { style: 'gap:4px;margin-left:28px' }, Knob(P('hitStretch'), 'STRETCH'), Knob(P('hitTrim'), 'P.TRIM'));
  else if (isENS() && P('hitPlay').get() === 1) {
    const synced = P('loopSync').get();
    const rate = Knob(P('loopRate'), synced ? SYNCDIVS[P('loopBeats').get()] : 'LOOP RATE', { color: 'var(--ptr-yellow)', def: .5, fmt: v => Math.pow(4, (v - .5) * 2).toFixed(2) + '\u00d7' });
    const syncLed = Led(P('loopSync'));
    const syncBtn = h('div', 'btn', { style: 'width:30px;height:14px', onclick: () => { const v = !P('loopSync').get(); P('loopSync').set(v); touch('LOOP SYNC', v ? 1 : 0); rebuildToneEdit(); } }, 'SYNC');
    const beats = Stepper(P('loopBeats'), SYNCDIVS, { lcd: true, readStyle: 'width:34px;height:14px', arrowStyle: 'width:13px;height:14px', label: 'LOOP BEATS' });
    const variLed = Led(P('loopVari'));
    const variBtn = h('div', 'btn', { style: 'width:76px;height:14px', onclick: () => { const v = !P('loopVari').get(); P('loopVari').set(v); touch('VARISPEED', v ? 1 : 0); } }, 'VARISPEED');
    comp = h('div', 'row', { style: 'gap:8px;margin-left:28px' }, rate,
      h('div', 'col', { style: 'gap:4px' },
        h('div', 'row', { style: 'gap:3px' }, syncLed, syncBtn, beats),
        h('div', 'row', { style: 'gap:3px' }, variLed, variBtn)));
  } else {
    comp = h('div', 'row', { style: 'gap:4px;margin-left:28px;opacity:.35;pointer-events:none' }, Knob(P('hitStretch'), 'STRETCH'), Knob(P('hitTrim'), 'P.TRIM'));
  }

  const rightKnobs = h('div', 'row', { style: 'gap:4px' },
    Knob(P('shapeDepth'), 'SHAPE DEPTH', { color: 'var(--ptr-yellow)' }), Knob(P('noise'), 'NOISE'), Knob(P('noiseCol'), 'NOISE COLOR'));
  const knobRow = h('div', 'row', { style: 'align-items:flex-start;margin-left:22px;gap:4px' }, leftKnobs, comp, rightKnobs);

  // ---- TVF + ADSR banks row
  const off = glob('globOffset').get();
  // ---- TVF header (mirrors WAVE row: colored label + LCD + < > arrows) + knob row below
  const tvfHeaderRow = h('div', 'row', { style: 'gap:6px;align-items:center' },
    h('div', 'lbl', { style: `color:${col};width:42px` }, 'TVF'),
    LcdMenu(P('tvfType'), TVFTYPES, 'TVF TYPE \u2014 TONE ' + TONES[i], 'width:170px;height:20px'),
    h('div', 'step', { onclick: () => cyc(P('tvfType'), TVFTYPES.length, -1) }, '\u2039'),
    h('div', 'step', { onclick: () => cyc(P('tvfType'), TVFTYPES.length, 1) }, '\u203a'));
  const tvfKnobs = h('div', 'row', { style: 'gap:4px;margin-left:22px;width:288px' },
    off ? Knob(glob('gCutoff'), 'CUT \u00b1', { color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip }) : Knob(P('cut'), 'CUTOFF', { color: 'var(--ptr-red)', def: .5 }),
    off ? Knob(glob('gRes'), 'RES \u00b1', { color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip }) : Knob(P('res'), 'RESONANCE', { color: 'var(--ptr-red)', def: .25 }),
    Knob(P('env'), 'ENVELOPE', { color: 'var(--ptr-red)', def: .5 }),
    Knob(P('kf'), 'KEY FLW', { color: 'var(--ptr-red)', bip: true, def: .5, fmt: fmtBip }));
  const tvfSection = h('div', 'col', { style: 'gap:9px' }, tvfHeaderRow, tvfKnobs);
  // ---- shared ENVELOPE editor (v17): TONE (1-4 / ALL) x ROUTE (AMP/FILT/AUX)
  const ENV_ADSR = { AMP:['aa','ad','as','ar'], FILT:['fa','fd','fs','fr'], AUX:['xa','xd','xs','xr'] };
  const ENV_GLOB = { AMP:['gAmpA','gAmpD','gAmpS','gAmpR'], FILT:['gFltA','gFltD','gFltS','gFltR'], AUX:['gAuxA','gAuxD','gAuxS','gAuxR'] };
  const ENV_OVR  = { AMP:'ampOvr', FILT:'filtOvr', AUX:'auxOvr' };
  const ENV_ALLC = '#B48CFF';
  const ENV_ROUTE_TXT = { AMP:'to voice amplitude', FILT:'to filter ENV AMT', AUX:'assignable MOD MATRIX source' };
  const envRoute = h('div', 'col', { style: 'gap:5px;justify-content:center' });
  const envTone  = h('div', 'row', { style: 'gap:5px;align-items:center' });
  const envCurve = s('svg', { viewBox: '0 0 300 56', preserveAspectRatio: 'none', style: 'position:absolute;inset:0;width:100%;height:100%' });
  const envTitle = h('div', null, { style: 'position:absolute;top:4px;left:50%;font:700 7px var(--f-silk);letter-spacing:.1em' });
  const envSub   = h('div', null, { style: 'position:absolute;bottom:3px;left:50%;font:600 6px var(--f-silk);color:#6d7a2a;letter-spacing:.05em' });
  const envLcd   = h('div', null, { style: 'position:relative;width:188px;height:56px;background:#0b0d05;border:1px solid #3a3410;border-radius:3px;overflow:hidden;box-shadow:inset 0 1px 6px rgba(0,0,0,.7)' }, envCurve, envTitle, envSub);
  const envSliders = h('div', 'row', { style: 'gap:14px;align-items:flex-end' });
  const envBlock = h('div', 'row', { style: 'gap:10px;align-items:stretch;margin-left:12px;flex:1' },
    envRoute,
    h('div', 'col', { style: 'gap:6px;flex:1' }, envTone, h('div', 'row', { style: 'gap:10px;align-items:flex-end;justify-content:space-between' }, envLcd, envSliders)));
  const envVal = (dest, all, idx) => (all || !P(ENV_OVR[dest]).get()) ? glob(ENV_GLOB[dest][idx]).get() : P(ENV_ADSR[dest][idx]).get();
  function commitEnv(idx, v) {
    const dest = UI.envDest, all = UI.envAll, ov = ENV_OVR[dest], gk = ENV_GLOB[dest], pk = ENV_ADSR[dest];
    if (all) glob(gk[idx]).set(v);
    else { if (!P(ov).get()) { pk.forEach((k, j) => P(k).set(glob(gk[j]).get())); P(ov).set(true); } P(pk[idx]).set(v); }
    touch(dest + ' ' + ['A','D','S','R'][idx], v);
  }
  function paintCurve() {
    const dest = UI.envDest, all = UI.envAll, color = all ? ENV_ALLC : TONE_COLORS[i];
    const a = envVal(dest, all, 0), d = envVal(dest, all, 1), su = envVal(dest, all, 2), r = envVal(dest, all, 3);
    const top = 8, bot = 48, x0 = 6; let x = x0; const p = [[x, bot]];
    x += 8 + a * 78; p.push([x, top]); x += 8 + d * 74; const sy = bot - (bot - top) * su; p.push([x, sy]);
    x += 66; p.push([x, sy]); x += 8 + r * 80; p.push([x, bot]);
    const line = p.map(q => q[0].toFixed(1) + ',' + q[1].toFixed(1)).join(' ');
    envCurve.innerHTML = '';
    [18, 37].forEach(y => envCurve.append(s('line', { x1: 0, y1: y, x2: 300, y2: y, stroke: '#1c2109', 'stroke-width': 1 })));
    envCurve.append(s('polygon', { points: x0 + ',' + bot + ' ' + line, fill: color, opacity: .14 }));
    envCurve.append(s('polyline', { points: line, fill: 'none', stroke: color, 'stroke-width': 2.5, 'stroke-linejoin': 'round' }));
    const xA = p[1][0], xS = p[2][0], syC = p[2][1], xR = p[4][0];
    const mkHandle = (cx, cy, onMove) => {
      const hit = s('circle', { cx, cy, r: 7, fill: 'transparent', style: 'cursor:grab' });
      hit.addEventListener('pointerdown', e => {
        e.preventDefault(); e.stopPropagation();
        const rect = envCurve.getBoundingClientRect();
        const mv = ev => onMove((ev.clientX - rect.left) / rect.width * 300, (ev.clientY - rect.top) / rect.height * 56);
        const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); };
        addEventListener('pointermove', mv); addEventListener('pointerup', up);
      });
      envCurve.append(hit, s('circle', { cx, cy, r: 4, fill: color, stroke: '#0b0d05', 'stroke-width': 1.5, style: 'pointer-events:none' }));
    };
    mkHandle(xA, top, vx => { commitEnv(0, clamp((vx - x0 - 8) / 78, 0, 1)); renderEnv(); });
    mkHandle(xS, syC, (vx, vy) => { commitEnv(1, clamp((vx - xA - 8) / 74, 0, 1)); commitEnv(2, clamp((bot - vy) / (bot - top), 0, 1)); renderEnv(); });
    mkHandle(xR, bot, vx => { commitEnv(3, clamp((vx - xS - 66 - 8) / 80, 0, 1)); renderEnv(); });
  }
  function renderEnv() {
    const dest = UI.envDest, all = UI.envAll, color = all ? ENV_ALLC : TONE_COLORS[i];
    const ov = ENV_OVR[dest], gk = ENV_GLOB[dest], pk = ENV_ADSR[dest];
    envRoute.innerHTML = '';
    ['AMP', 'FILT', 'AUX'].forEach(key => { const on = dest === key;
      envRoute.append(h('div', null, { style: `width:38px;height:18px;display:flex;align-items:center;justify-content:center;border-radius:3px;cursor:pointer;font:700 8px var(--f-silk);letter-spacing:.06em;background:${on ? color : '#181b34'};border:1px solid ${on ? color : '#464e94'};color:${on ? '#07070a' : '#c9cdf2'}`,
        onclick: () => { UI.envDest = key; renderEnv(); } }, key)); });
    envTone.innerHTML = '';
    envTone.append(h('div', null, { style: 'width:30px;font:700 7px var(--f-silk);color:#9aa1d8;letter-spacing:.14em' }, 'TONE'));
    [['0','1'],['1','2'],['2','3'],['3','4'],['ALL','ALL']].forEach(([m, lab]) => { const isAll = m === 'ALL', on = isAll ? all : (!all && i === +m);
      const btn = h('div', null, { style: `position:relative;min-width:16px;height:16px;padding:0 5px;display:flex;align-items:center;justify-content:center;border-radius:3px;cursor:pointer;font:700 8px var(--f-silk);background:${on ? color : '#181b34'};border:1px solid ${on ? color : '#464e94'};color:${on ? '#07070a' : '#c9cdf2'}`,
        onclick: () => { if (isAll) { UI.envAll = true; renderEnv(); } else if (i !== +m) { UI.sel = +m; UI.envAll = false; rebuildToneEdit(); notify(); } else { UI.envAll = false; renderEnv(); } } }, lab);
      if (!isAll && tone(ov, +m).get()) btn.append(h('div', null, { style: 'position:absolute;top:1px;right:1px;width:3px;height:3px;border-radius:50%;background:#ffd23f;box-shadow:0 0 3px #ffd23f' }));
      envTone.append(btn); });
    envTitle.style.color = color;
    envTitle.textContent = dest + ' \u00b7 ' + (all ? 'ALL (GLOBAL)' : 'TONE ' + TONES[i]);
    const subTxt = () => all ? ('GLOBAL \u2014 ' + ENV_ROUTE_TXT[dest]) : (P(ov).get() ? 'PER-TONE OVERRIDE' : 'FOLLOWS GLOBAL');
    envSub.textContent = subTxt();
    envSliders.innerHTML = '';
    ['A','D','S','R'].forEach((lab, idx) => {
      const subs = [];
      const handle = { get: () => envVal(dest, all, idx), sub: f => subs.push(f),
        set: v => { commitEnv(idx, v); subs.forEach(f => f(v)); paintCurve(); envSub.textContent = subTxt(); } };
      envSliders.append(Slider(handle, lab, { h: 48 }));
    });
    paintCurve();
  }
  renderEnv();

  const tvfRow = h('div', 'row', { style: 'align-items:flex-start' },
    tvfSection,
    h('div', 'divider-v', { style: 'margin-left:24px' }),
    envBlock);

  // ---- LFO rows
  const lfoRow = (n, rk, dk, sk, dk2, shk) => {
    const synced = P(sk).get();
    const rate = Knob(P(rk), synced ? SYNCDIVS[Math.min(11, Math.floor(P(rk).get() * 12))] : 'RATE', { size: 26, color: synced ? 'var(--ptr-yellow)' : null });
    const syncLed = Led(P(sk));
    const syncBtn = h('div', 'btn', { style: 'width:32px;height:15px', onclick: () => { const v = !P(sk).get(); P(sk).set(v); touch(n + ' SYNC', v ? 1 : 0); rebuildToneEdit(); } }, 'SYNC');
    const shapeStep = Stepper(P(shk), WAVE_SHAPES, { lcd: true, readStyle: 'width:34px;height:16px', arrowStyle: 'width:14px;height:16px', label: n + ' SHAPE', onLcd: () => openMenu(n + ' SHAPE \u2014 TONE ' + TONES[i], WAVE_SHAPES, P(shk)) });
    const dest = h('div', 'btn', { style: 'width:56px;height:16px', onclick: () => openMenu(n + ' DEST \u2014 TONE ' + TONES[i], LFODESTS, P(dk2)) });
    const paintDest = () => dest.textContent = LFODESTS[P(dk2).get()]; paintDest(); P(dk2).sub(paintDest);
    return h('div', 'row', { style: 'gap:8px' }, h('div', 'lbl-hi', { style: 'width:32px' }, n),
      rate, Knob(P(dk), 'DEPTH', { size: 26 }), shapeStep,
      h('div', 'row', { style: 'gap:4px' }, syncLed, syncBtn), L('DEST'), dest);
  };

  // ---- AUX ENV + VOICING row
  const auxAmt = Knob(P('auxAmt'), 'AMOUNT', { size: 26, color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip });
  const auxDest = h('div', 'btn', { style: 'width:56px;height:16px', onclick: () => openMenu('AUX ENV DEST \u2014 TONE ' + TONES[i], AUXDESTS, P('auxDest')) });
  const paintAux = () => auxDest.textContent = AUXDESTS[P('auxDest').get()]; paintAux(); P('auxDest').sub(paintAux);
  const voicing = h('div', 'lcd click', { style: 'width:54px;height:16px', onclick: () => openMenu('VOICING \u2014 TONE ' + TONES[i], VOICINGS, P('voicing')) });
  const spread = h('div', 'lcd click', { style: 'width:44px;height:16px', onclick: () => openMenu('DREAMY SPREAD \u2014 TONE ' + TONES[i], SPREADS, P('spread')) });
  const paintVoi = () => { voicing.textContent = VOICINGS[P('voicing').get()]; spread.style.display = P('voicing').get() === 3 ? 'flex' : 'none'; };
  paintVoi(); P('voicing').sub(paintVoi);
  const paintSpread = () => spread.textContent = SPREADS[P('spread').get()]; paintSpread(); P('spread').sub(paintSpread);
  const auxRow = h('div', 'row', { style: 'gap:8px' }, h('div', 'lbl-hi', { style: 'width:32px' }, 'AUX ENV'),
    auxAmt, L('DEST'), auxDest, h('div', 'divider-v', { style: 'margin:0 6px' }), LH('VOICE'), voicing, spread, h('div', 'grow'));

  // ---- GLOBAL sub-block (absolute, bottom-right; nothing else moves)
  const detVoices = Stepper(P('detVoices'), ['1','2','3','4'], { lcd: true, readStyle: 'width:20px;height:16px', arrowStyle: 'width:13px;height:16px', clamp: [0, 3], label: 'DETUNE VOICES' });
  const detKnob = Knob(P('detAmt'), 'DETUNE', { size: 30, color: 'var(--ptr-yellow)', def: 0, fmt: v => '\u00b1' + Math.round(v * 25) + ' c' });
  const dimDet = () => detKnob.classList.toggle('dim', P('detVoices').get() === 0);
  dimDet(); P('detVoices').sub(dimDet);
  const gOctStep = h('div', 'col', { style: 'align-items:center;gap:3px' },
    (() => { const read = h('div', 'lcd', { style: 'width:24px;height:16px' });
      const paint = () => { const n = Math.round((glob('gOctave').get() - .5) * 4); read.textContent = (n > 0 ? '+' : '') + n; };
      paint(); glob('gOctave').sub(paint);
      const dec = h('div', 'step', { style: 'width:13px;height:16px', onclick: () => { glob('gOctave').set(clamp(glob('gOctave').get() - .25, 0, 1)); touch('GLOBAL OCTAVE', glob('gOctave').get()); } }, '\u2039');
      const inc = h('div', 'step', { style: 'width:13px;height:16px', onclick: () => { glob('gOctave').set(clamp(glob('gOctave').get() + .25, 0, 1)); touch('GLOBAL OCTAVE', glob('gOctave').get()); } }, '\u203a');
      return h('div', 'row', { style: 'gap:2px' }, dec, read, inc); })(),
    h('div', 'lbl-sm', null, 'OCTAVE'));
  const offLed = Led(glob('globOffset'));
  const offBtn = h('div', 'btn', { style: 'width:86px;height:16px', onclick: () => { const v = !glob('globOffset').get(); glob('globOffset').set(v); touch(v ? 'GLOBAL OFFSET ON' : 'GLOBAL OFFSET OFF', v ? 1 : 0); rebuildToneEdit(); } }, 'OFFSET MODE');
  const globalBlock = h('div', 'group', { style: 'position:absolute;right:12px;bottom:10px;width:307px;padding:13px 10px 8px;display:flex;flex-direction:column;gap:9px;background:rgba(16,18,40,.45)' },
    h('div', 'group-title', { style: 'background:#2a2f56;font-size:9px' }, 'GLOBAL'),
    h('div', 'row', { style: 'gap:9px' },
      h('div', 'col', { style: 'align-items:center;gap:3px' }, detVoices, h('div', 'lbl-sm', null, 'VOICES')),
      detKnob, h('div', 'divider-v'), gOctStep),
    h('div', 'row', { style: 'gap:7px' }, offLed, offBtn,
      h('div', null, { style: 'font:600 6.5px var(--f-silk);color:var(--silk-dim);line-height:1.15' }, 'AMP ADSR + CUT/RES \u2192 GLOBAL \u00b1')));

  const block = grp('TONE EDIT', 'padding:16px 12px 8px;display:flex;flex-direction:column;gap:9px',
    waveRow, knobRow, tvfRow,
    lfoRow('LFO1', 'l1r', 'l1d', 'l1sync', 'l1dest', 'l1shape'),
    lfoRow('LFO2', 'l2r', 'l2d', 'l2sync', 'l2dest', 'l2shape'),
    auxRow, globalBlock);
  // colour the TONE EDIT title letter
  block.querySelector('.group-title').append(' ', h('span', null, { style: `color:${col}` }, TONES[i]));
  return block;
}
function cyc(handle, len, d) { handle.set((handle.get() + d + len) % len); }

/* ---- FILTERS -------------------------------------------------------------- */
function buildFilters() {
  const strip = (n, typeKey, cutKey, resKey, thirdKey, thirdLabel, thirdColor, thirdBip) => {
    const typeLcd = h('div', 'lcd click grow', { style: 'height:20px', onclick: () => openMenu('FILTER ' + n + ' TYPE', FTYPES, glob(typeKey)) });
    const paint = () => typeLcd.textContent = FTYPES[glob(typeKey).get()]; paint(); glob(typeKey).sub(paint);
    const typeRow = h('div', 'row', { style: 'gap:5px;width:100%' }, h('div', null, { style: 'font:700 9px var(--f-silk);color:var(--silk-hi);width:10px' }, String(n)),
      h('div', 'step', { onclick: () => cyc(glob(typeKey), FTYPES.length, -1) }, '\u2039'), typeLcd, h('div', 'step', { onclick: () => cyc(glob(typeKey), FTYPES.length, 1) }, '\u203a'));
    const knobs = h('div', 'row', { style: 'justify-content:space-between;width:100%' },
      Knob(glob(cutKey), 'CUTOFF', { size: 36, color: 'var(--ptr-red)', def: GLOBAL_DEFAULTS[cutKey] }),
      Knob(glob(resKey), 'RESONANCE', { size: 36, color: 'var(--ptr-red)', def: GLOBAL_DEFAULTS[resKey] }),
      Knob(glob(thirdKey), thirdLabel, { size: 36, color: thirdColor, bip: thirdBip, def: GLOBAL_DEFAULTS[thirdKey], fmt: thirdBip ? fmtBip : null }));
    return { typeRow, knobs };
  };
  const f1 = strip(1, 'f1Type', 'f1Cut', 'f1Res', 'f1Env', 'ENVELOPE', 'var(--ptr-red)', false);
  const f2 = strip(2, 'f2Type', 'f2Cut', 'f2Res', 'f2Morph', 'MORPH', 'var(--ptr-yellow)', true);

  const serLed = h('div', 'led'), parLed = h('div', 'led');
  const paintRoute = () => { const r = glob('route').get(); serLed.classList.toggle('on', !r); parLed.classList.toggle('on', !!r); };
  paintRoute(); glob('route').sub(paintRoute);
  const routeRow = h('div', 'row', { style: 'margin-top:auto;gap:14px;padding-bottom:2px;align-items:center;justify-content:center' },
    h('div', 'col', { style: 'align-items:center;gap:2px' },
      h('div', 'row', { style: 'gap:3px' }, serLed, L('SER')),
      h('div', 'btn', { style: 'width:34px;height:18px', onclick: () => { glob('route').set(!glob('route').get()); touch('FILTER ROUTE', glob('route').get() ? 1 : 0); } }),
      h('div', 'row', { style: 'gap:3px' }, parLed, L('PAR'))),
    h('div', 'row', { style: 'gap:4px;align-items:center' }, L('F1'),
      Knob(glob('fbal'), 'BALANCE', { size: 26, color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip }),
      L('F2')));

  return grp('FILTERS', 'padding:16px 10px 8px;display:flex;flex-direction:column;gap:7px;align-items:center',
    f1.typeRow, f1.knobs, h('div', 'divider-h'), f2.typeRow, f2.knobs, routeRow);
}

/* ---- DREAM VECTOR (radar) ------------------------------------------------- */
let RADAR = null;
function buildVector() {
  const box = h('div', null, { style: 'position:relative;width:150px;height:132px;background:linear-gradient(180deg,#333857 0%,#242640 50%,#484D70 100%);border:1px solid #4D547A;border-radius:6px;box-shadow:inset 0 1px 1px rgba(255,255,255,.12),inset 0 -2px 4px rgba(0,0,0,.4)' });
  const svg = s('svg', { viewBox: '0 0 150 132', style: 'position:absolute;inset:0;width:100%;height:100%' });
  box.append(svg);
  // corner screws around orb
  [[6,6],[135,6],[6,117],[135,117]].forEach(([x,y]) => box.append(h('div', null, { style: `position:absolute;left:${x}px;top:${y}px;width:9px;height:9px;border-radius:50%;background:radial-gradient(circle at 40% 40%,#8087A8,#141724);box-shadow:0 1px 1px rgba(0,0,0,.6)` })));
  RADAR = svg;

  const shape = Stepper(glob('vecShape'), VECSHAPES, { readStyle: 'width:36px;height:16px', arrowStyle: 'width:14px;height:16px', label: 'ORBIT SHAPE', onLcd: () => openMenu('ORBIT SHAPE', VECSHAPES, glob('vecShape')) });
  const left = h('div', 'col', { style: 'align-items:center;gap:4px' }, box,
    h('div', 'lbl-sm', null, 'VECTOR RADAR \u00b7 PHASE'), h('div', 'row', { style: 'gap:4px;align-items:center' }, L('SHAPE'), shape));

  const knobs = h('div', 'col', { style: 'gap:6px;justify-content:space-around;padding:2px 0' },
    Knob(glob('vphase'), 'PHASE', { size: 32, color: 'var(--ptr-yellow)' }), Knob(glob('orbRate'), 'RATE', { size: 32, fmt: v => { const hz = 0.02 * Math.pow(1000, v); return (hz < 1 ? hz.toFixed(2) : hz.toFixed(1)) + ' Hz'; } }),
    ledToggle(glob('orbit'), 'ORBIT'), ledToggle(glob('venv'), 'P-ENV'),
    h('div', 'btn', { style: 'width:40px;height:14px', onclick: openPenv }, 'EDIT'));
  // relabel toggles
  knobs.children[2].append(h('div', 'lbl-sm', null, 'ORBIT'));
  knobs.children[3].append(h('div', 'lbl-sm', { style: 'white-space:nowrap' }, 'P-ENV'));

  const dirInt = h('div', 'col', { style: 'gap:5px;justify-content:space-around;padding:2px 0' },
    h('div', 'row', { style: 'align-items:flex-end;gap:8px;padding-left:18px' },
      h('div', null, { style: 'width:44px;text-align:center;font:700 7.5px var(--f-silk);color:var(--silk-hi)' }, 'DIRECTION'),
      h('div', null, { style: 'width:44px;text-align:center;font:700 7.5px var(--f-silk);color:var(--silk-hi)' }, 'INTENSITY')),
    ...TONES.map((n, i) => h('div', 'row', { style: 'align-items:center;gap:8px' },
      h('div', null, { style: `width:10px;font:800 11px var(--f-lcd);color:${TONE_COLORS[i]}` }, n),
      h('div', null, { style: 'width:44px;display:flex;justify-content:center' }, Knob(tone('dir', i), null, { size: 22, color: 'var(--ptr-yellow)' })),
      h('div', null, { style: 'width:44px;display:flex;justify-content:center' }, Knob(tone('vint', i), null, { size: 22, color: 'var(--ptr-yellow)' })))));

  return grp('DREAM VECTOR', 'padding:16px 10px 8px;display:flex;gap:10px', left, knobs, h('div', 'divider-v'), dirInt);
}

/* ---- MOD MATRIX + GLOBAL LFO --------------------------------------------- */
function buildMatrix() {
  const rows = [0, 1, 2].map(i => {
    const S = mtx(i, 'src'), D = mtx(i, 'dst'), A = mtx(i, 'amt');
    const src = h('div', 'btn', { style: 'width:52px;height:18px;flex:none', onclick: () => openMenu('MOD SOURCE \u2014 SLOT ' + (i + 1), MSRC, S) });
    const dst = h('div', 'btn', { style: 'width:52px;height:18px;flex:none', onclick: () => openMenu('MOD DEST \u2014 SLOT ' + (i + 1), MDST, D) });
    const rot = h('div', 'knob-rot');
    const amt = h('div', 'knob', { style: '--s:26px;flex:none' }, h('div', 'knob-cap', null, h('div', 'knob-face'), rot));
    rot.append(h('div', 'knob-ptr', { style: '--pc:var(--ptr-yellow)' }));
    const fill = h('div', null, { style: 'position:absolute;left:0;top:0;bottom:0;opacity:.8;width:0%;background:var(--ptr-yellow)' });
    dragBehaviour(amt, A, { label: 'AMT ' + (i + 1), bip: true, def: .5, fmt: fmtBip });
    const paintSrc = () => src.textContent = MSRC[S.get()]; paintSrc(); S.sub(paintSrc);
    const paintDst = () => dst.textContent = MDST[D.get()]; paintDst(); D.sub(paintDst);
    const paintAmt = v => { rot.style.setProperty('--rot', Math.round(-135 + v * 270) + 'deg'); fill.style.width = Math.round(Math.abs(v - .5) * 200) + '%'; fill.style.background = v < .5 ? 'var(--led-on)' : 'var(--ptr-yellow)'; };
    paintAmt(A.get()); A.sub(paintAmt);
    return h('div', 'row', { style: 'gap:5px' }, h('div', null, { style: 'font:700 8px var(--f-silk);color:var(--silk);width:8px' }, String(i + 1)),
      src, h('div', null, { style: 'font:800 10px var(--f-lcd);color:var(--lcd-ink)' }, '>'), dst, amt,
      h('div', 'grow', { style: 'height:8px;background:var(--track);border-radius:2px;position:relative;overflow:hidden' }, fill));
  });
  const glfoRowUI = (label, rKey, sKey, wKey) => {
    const rate = Knob(glob(rKey), 'RATE', { size: 26 });
    const syncLed = Led(glob(sKey));
    const syncBtn = h('div', 'btn', { style: 'width:30px;height:14px', onclick: () => { glob(sKey).set(!glob(sKey).get()); touch(label + ' SYNC', glob(sKey).get() ? 1 : 0); } }, 'SYNC');
    const wave = Stepper(glob(wKey), WAVE_SHAPES, { readStyle: 'width:38px;height:16px', arrowStyle: 'width:13px;height:16px', label: label + ' SHAPE', onLcd: () => openMenu(label + ' SHAPE', WAVE_SHAPES, glob(wKey)) });
    return h('div', 'row', { style: 'gap:6px;align-items:center' }, h('div', 'lbl-hi', { style: 'width:44px' }, label), rate, h('div', 'row', { style: 'gap:4px' }, syncLed, syncBtn), wave);
  };
  const glfo = h('div', 'col', { style: 'gap:4px;margin-top:auto' },
    glfoRowUI('G-LFO 1', 'glfoRate', 'glfoSync', 'glfoWave'),
    glfoRowUI('G-LFO 2', 'glfo2Rate', 'glfo2Sync', 'glfo2Wave'));
  return grp('MOD MATRIX', 'padding:16px 10px 8px;display:flex;flex-direction:column;gap:5px', ...rows, glfo);
}
/* menu overlay bound to a plain getter/setter (for UI.matrix objects) */
function openMenuObj(title, arr, get, set) {
  const cur = get();
  const rows = arr.map((it, i) => h('div', 'menu-row' + (i === cur ? ' cur' : ''), { onclick: () => { set(i); closeOverlay(); } },
    h('span', null, { style: 'width:20px' }, String(i + 1).padStart(2, '0')), h('span', null, null, (i === cur ? '\u25B8 ' : '') + it)));
  showOverlay(h('div', 'menu', { onclick: e => e.stopPropagation() },
    h('div', 'menu-head', null, h('span', null, null, title), h('span', null, null, 'ESC=EXIT')),
    h('div', 'menu-list', null, ...rows)));
}

/* ---- FX ------------------------------------------------------------------- */
function buildFX() {
  const fxRow = (name, typeArr, typeKey, knobDefs, onKey, focusArr, focusKey, paramKey, syncKey, divider) => {
    const typeLcd = h('div', 'lcd click', { style: 'width:52px;height:18px', onclick: () => openMenu(name + ' TYPE', typeArr, glob(typeKey)) });
    const paintT = () => typeLcd.textContent = typeArr[glob(typeKey).get()]; paintT(); glob(typeKey).sub(paintT);
    const knobs = h('div', 'grow row', { style: 'justify-content:space-evenly;align-items:flex-start' },
      ...knobDefs.map(([k, l]) => {
        const synced = syncKey && k === knobDefs[1][0] && glob(syncKey).get();
        return Knob(glob(k), synced ? SYNCDIVS[Math.min(11, Math.floor(glob(k).get() * 12))] : l, { size: 26, color: synced ? 'var(--ptr-yellow)' : null });
      }));
    const focus = h('div', 'lcd click', { style: 'width:52px;height:18px', onclick: () => cyc(glob(focusKey), focusArr.length, 1) });
    const paintF = () => focus.textContent = focusArr[glob(focusKey).get()]; paintF(); glob(focusKey).sub(paintF);
    const paramK = Knob(glob(paramKey), 'PARAMS', { size: 26, color: 'var(--ptr-yellow)' });
    const mid = h('div', 'row', { style: 'flex:none;gap:8px' }, h('div', 'col', { style: 'align-items:center;gap:2px' }, h('div', 'lbl-sm', null, 'FOCUS'), focus), paramK);
    const tail = h('div', 'row', { style: 'flex:none;width:82px;justify-content:flex-end;gap:6px' });
    if (syncKey) { const sLed = Led(glob(syncKey)); tail.append(h('div', 'col', { style: 'align-items:center;gap:2px' }, sLed, h('div', 'btn', { style: 'width:30px;height:13px', onclick: () => { glob(syncKey).set(!glob(syncKey).get()); touch(name + ' SYNC', glob(syncKey).get() ? 1 : 0); rebuildFX(); } }, 'SYNC'))); }
    const onLed = Led(glob(onKey));
    tail.append(h('div', 'row', { style: 'gap:5px' }, onLed, h('div', 'btn', { style: 'width:26px;height:14px', onclick: () => { glob(onKey).set(!glob(onKey).get()); touch(name + ' ON', glob(onKey).get() ? 1 : 0); } })));
    const row = h('div', 'row', { style: 'gap:6px' },
      h('div', 'lbl-hi', { style: 'width:30px;flex:none' }, name),
      h('div', 'col', { style: 'align-items:center;gap:2px;flex:none' }, h('div', 'lbl-sm', null, 'TYPE'), typeLcd),
      knobs, mid, tail);
    return h('div', 'col', { style: 'gap:6px' }, row, divider ? h('div', 'divider-h', { style: 'opacity:.4' }) : null);
  };
  const util = h('div', 'btn', { style: 'position:absolute;top:-9px;right:10px;padding:1px 7px;font-size:7.5px;z-index:2', onclick: openUtil }, 'UTIL \u25b8');
  return grp('FX', 'padding:16px 12px 10px;display:flex;flex-direction:column;justify-content:space-between;gap:6px',
    util,
    fxRow('MOD', MODFX, 'mfxType', [['mfxMix','MIX'],['mfxRate','RATE'],['mfxDepth','DEPTH']], 'mfxOn', MODFXFOCUS, 'mfxFocus', 'mfxParam', null, true),
    fxRow('DELAY', DLYMODES, 'dlyMode', [['dMix','MIX'],['dTime','TIME'],['dFb','FB']], 'dlyOn', DLYFOCUS, 'dlyFocus', 'dParam', 'dlySync', true),
    fxRow('REVERB', REVTYPES, 'revType', [['rMix','MIX'],['rSize','SIZE'],['rDamp','DAMP']], 'revOn', REVFOCUS, 'revFocus', 'rParam', null, false));
}
let fxHost = null;
function rebuildFX() { const f = buildFX(); if (fxHost && fxHost.parentNode) fxHost.replaceWith(f); fxHost = f; }

/* ---- UTILITY + P-ENV modals ---------------------------------------------- */
function openUtil() {
  const knob = (k, l, c) => Knob(glob(k), l, { size: 30, color: c });
  const lofiFocus = h('div', 'lcd click', { style: 'width:62px;height:16px', onclick: () => { cyc(glob('lofiFocus'), LOFIFOCUS.length, 1); openUtil(); } });
  const talkFocus = h('div', 'lcd click', { style: 'width:62px;height:16px', onclick: () => cyc(glob('talkFocus'), TALKFOCUS.length, 1) });
  const bind = (lcd, arr, key) => { const p = () => lcd.textContent = arr[glob(key).get()]; p(); glob(key).sub(p); };
  bind(lofiFocus, LOFIFOCUS, 'lofiFocus'); bind(talkFocus, TALKFOCUS, 'talkFocus');
  // LO-FI PARAMS knob is focus-aware: BITS/SRATE/COMPAND/ALIAS each drive their own APVTS param
  const LOFI_IDS = ['lofiBits', 'lofiSrate', 'lofiCompand', 'lofiAlias'];
  const lofiParamKnob = Knob(glob(LOFI_IDS[glob('lofiFocus').get()]), 'PARAMS', { size: 30, color: 'var(--ptr-yellow)' });
  const prePost = h('div', 'btn', { style: 'width:88px;height:18px', onclick: () => { glob('prePost').set(!glob('prePost').get()); pp(); } });
  const pp = () => prePost.textContent = (glob('prePost').get() ? 'POST' : 'PRE') + ' FILTERS'; pp();
  const row = (...k) => h('div', 'row', { style: 'gap:10px;align-items:center' }, ...k);
  const modal = h('div', 'modal', { onclick: e => e.stopPropagation() },
    h('div', 'menu-head', { style: "font:700 10px var(--f-silk);letter-spacing:.18em;color:var(--silk-hi)" }, h('span', null, null, 'UTILITY \u2014 LO-FI \u00b7 WIDTH \u00b7 TALK'), h('span', { style: 'color:var(--silk)' }, null, 'ESC=EXIT')),
    row(Led(glob('lofiOn')), tglBtn('lofiOn', 'LO-FI', 52), lofiFocus, lofiParamKnob),
    row(Led(glob('widthOn')), tglBtn('widthOn', 'WIDTH', 52), knob('widthAmt', 'WIDTH'), knob('haas', 'HAAS'), Led(glob('bassMono')), tglBtn('bassMono', 'BASS MONO', 74)),
    row(Led(glob('talkOn')), tglBtn('talkOn', 'TALK', 52), talkFocus, knob('talkParam', 'PARAMS', 'var(--ptr-yellow)')),
    h('div', 'row', { style: 'gap:10px;border-top:1px solid var(--frame);padding-top:10px' }, Led(glob('limiter_on')), tglBtn('limiter_on', 'LIMITER', 64), h('div', 'lbl-sm', null, 'OUTPUT BRICKWALL \u00b7 DEFAULT ON')),
    h('div', 'row', { style: 'gap:10px;border-top:1px solid var(--frame);padding-top:10px' }, h('div', 'lbl', null, 'LO-FI / WIDTH PLACEMENT'), prePost));
  showOverlay(modal);
}
function tglBtn(key, text, w) {
  const b = h('div', 'btn', { style: `width:${w}px;height:18px`, onclick: () => { glob(key).set(!glob(key).get()); touch(text, glob(key).get() ? 1 : 0); } }, text);
  const p = () => b.classList.toggle('on', !!glob(key).get()); p(); glob(key).sub(p); return b;
}
function openPenv() {
  const k = (key, l, c) => Knob(glob(key), l, { size: 38, color: c, def: GLOBAL_DEFAULTS[key] });
  const loopLed = Led(glob('penvLoop'));
  const modal = h('div', 'modal', { onclick: e => e.stopPropagation() },
    h('div', 'menu-head', { style: "font:700 10px var(--f-silk);letter-spacing:.18em;color:var(--silk-hi)" }, h('span', null, null, 'P-ENV \u2014 VECTOR PHASE ENVELOPE'), h('span', { style: 'color:var(--silk)' }, null, 'ESC=EXIT')),
    h('div', 'row', { style: 'gap:14px;align-items:center' }, k('penvStart', 'START', 'var(--ptr-yellow)'), k('penvEnd', 'END', 'var(--ptr-yellow)'), k('penvTime', 'TIME'),
      h('div', 'col', { style: 'align-items:center;gap:4px' }, loopLed, h('div', 'btn', { style: 'width:34px;height:22px', onclick: () => { const v = !glob('penvLoop').get(); glob('penvLoop').set(v); touch('P-ENV LOOP', v ? 1 : 0); } }), h('div', 'lbl-hi', null, 'LOOP'))));
  showOverlay(modal);
}

/* ---- PRESET BROWSER (factory + user bank) -------------------------------- */
function openPresetBrowser() {
  const factory = h('div', 'menu-list', { style: 'max-height:430px' });
  PRESETS.forEach((p, i) => {
    const row = h('div', 'menu-row' + (UI.presetSel.bank === 'FACTORY' && UI.presetSel.i === i ? ' cur' : ''), { onclick: () => { UI.presetSel = { bank: 'FACTORY', i }; UI.preset = i; Bridge.fn('loadPreset')(i); refreshHeader(); reopenPreset(); } },
      h('span', null, { style: 'width:34px' }, 'P' + String(i + 1).padStart(3, '0')), h('span', null, { style: 'width:42px' }, p[0]), h('span', null, null, p[1]));
    factory.append(row);
  });
  const user = h('div', 'menu-list', { style: 'flex:1;min-height:64px;max-height:250px' });
  if (!USER_PRESETS.length) user.append(h('div', null, { style: 'padding:6px;font:800 9px var(--f-lcd);color:#3a6d52' }, '(NO USER PRESETS \u2014 SAVE ONE)'));
  USER_PRESETS.forEach((p, i) => {
    const seld = UI.presetSel.bank === 'USER' && UI.presetSel.i === i;
    const row = h('div', 'menu-row', { style: seld ? 'color:#07070a;background:#7dffc0' : 'color:#7dffc0', onclick: () => { UI.presetSel = { bank: 'USER', i }; UI.renameBuf = p.name; Bridge.fn('loadUserPreset')(p.name); reopenPreset(); } },
      h('span', null, { style: 'width:28px' }, 'U' + String(i + 1).padStart(2, '0')), h('span', null, null, p.name));
    user.append(row);
  });
  const field = h('input', 'field', { placeholder: 'RENAME SELECTED USER\u2026', value: UI.renameBuf, oninput: e => UI.renameBuf = e.target.value.toUpperCase() });
  const btn = (t, cls, fn) => h('div', 'btn', { style: 'flex:1;height:22px' + (cls || ''), onclick: fn }, t);
  const actions = h('div', 'row', { style: 'gap:5px' },
    btn('SAVE', '', () => { Bridge.fn('saveUserPreset')('USER ' + String(USER_PRESETS.length + 1).padStart(2, '0')); UI.presetSel = { bank: 'USER', i: USER_PRESETS.length - 1 }; UI.renameBuf = USER_PRESETS[UI.presetSel.i].name; touch('SAVE PRESET', 1); reopenPreset(); }),
    btn('RENAME', '', () => { if (UI.presetSel.bank === 'USER' && USER_PRESETS[UI.presetSel.i]) { Bridge.fn('renameUserPreset')(USER_PRESETS[UI.presetSel.i].name, UI.renameBuf); reopenPreset(); } }),
    h('div', 'btn', { style: 'flex:1;height:22px;border-color:#7a2130;color:#ff8a97', onclick: () => { if (UI.presetSel.bank === 'USER' && USER_PRESETS[UI.presetSel.i]) { Bridge.fn('deleteUserPreset')(USER_PRESETS[UI.presetSel.i].name); UI.presetSel = { bank: 'FACTORY', i: 0 }; reopenPreset(); } } }, 'DELETE'));
  const load = h('div', 'btn', { style: 'height:24px;background:#1e2547;color:var(--lcd-ink);font-size:9px;letter-spacing:.12em', onclick: () => { if (UI.presetSel.bank === 'USER') { const p = USER_PRESETS[UI.presetSel.i]; if (p) Bridge.fn('loadUserPreset')(p.name); } else { UI.preset = UI.presetSel.i; Bridge.fn('loadPreset')(UI.presetSel.i); refreshHeader(); } closeOverlay(); } }, 'LOAD SELECTED');
  const box = h('div', null, { style: 'width:600px;max-height:540px;background:var(--lcd-bg);border:1px solid var(--frame);border-radius:4px;box-shadow:0 8px 40px #000;padding:12px;display:flex;flex-direction:column;gap:10px', onclick: e => e.stopPropagation() },
    h('div', 'menu-head', null, h('span', null, null, 'PRESET BROWSER'), h('span', null, null, 'ESC=EXIT')),
    h('div', 'row', { style: 'gap:12px;min-height:0' },
      h('div', 'grow col', { style: 'gap:4px;min-height:0' }, h('div', 'lbl-hi', { style: 'color:var(--silk-hi)' }, 'FACTORY \u00b7 47'), factory),
      h('div', null, { style: 'width:1px;background:var(--frame);opacity:.5' }),
      h('div', 'col', { style: 'width:232px;gap:5px;min-height:0' }, h('div', 'lbl-hi', { style: 'color:#7dffc0' }, 'USER BANK'), user, field, actions, load,
        h('div', null, { style: 'font:600 7px var(--f-silk);color:var(--silk-dim)' }, 'FACTORY READ-ONLY \u00b7 SAVE = USER COPY'))));
  showOverlay(box, 'z11');
}
function reopenPreset() { openPresetBrowser(); }

/* ============================================================ ANIMATION ==== */
function drawRadar() {
  if (!RADAR) return;
  const CX = 75, CY = 63, R = 62, BR = '#339E61', FT = '#268052';
  const phi = glob('vphase').get() * 2 * Math.PI;
  const kids = [];
  const grad = (id, stops, attrs = {}) => { const g = s('radialGradient', Object.assign({ id, cx: '50%', cy: '50%', r: '50%' }, attrs)); stops.forEach(([o, c, op]) => g.append(s('stop', { offset: o, 'stop-color': c, 'stop-opacity': op }))); return g; };
  const defs = s('defs');
  defs.append(grad('rf', [['0%','#0A0F0D'],['80%','#05080A'],['100%','#000503']]));
  defs.append(grad('rp', [['0%','#144D2E',.3],['60%','#0F3824',.16],['100%','#0A2417',0]]));
  defs.append(grad('rs', [['0%','#4DE699',.45],['100%','#4DE699',0]]));
  defs.append(grad('rh', [['0%','#4DE699'],['60%','#1A6647'],['100%','#0A2417']]));
  TONE_CORE.forEach((c, i) => { defs.append(grad('rc' + i, [['0%',c[0]],['45%',c[1]],['100%',c[2]]])); defs.append(grad('rg' + i, [['0%',TONE_COLORS[i],.55],['100%',TONE_COLORS[i],0]])); });
  const bez = s('linearGradient', { id: 'rb', x1: CX, y1: CY - R, x2: CX, y2: CY + R, gradientUnits: 'userSpaceOnUse' });
  [['0%','#6B7399'],['50%','#1A1C2E'],['100%','#474D70']].forEach(([o, c]) => bez.append(s('stop', { offset: o, 'stop-color': c })));
  defs.append(bez);
  const clip = s('clipPath', { id: 'rt' }); clip.append(s('circle', { cx: CX, cy: CY, r: R })); defs.append(clip);
  kids.push(defs);
  kids.push(s('circle', { cx: CX, cy: CY, r: R + 2, fill: 'none', stroke: 'url(#rb)', 'stroke-width': 3.9 }));
  kids.push(s('circle', { cx: CX, cy: CY, r: R, fill: 'url(#rf)' }));
  const tube = s('g', { 'clip-path': 'url(#rt)' });
  tube.append(s('circle', { cx: CX, cy: CY, r: R, fill: 'url(#rp)' }));
  [[.225,.3],[.452,.34],[.68,.4],[.906,.55]].forEach(([f, op]) => tube.append(s('circle', { cx: CX, cy: CY, r: (R*f).toFixed(1), fill: 'none', stroke: BR, 'stroke-width': 1, opacity: op })));
  [[.134,.1],[.407,.13],[.68,.16],[.952,.22]].forEach(([f, op]) => tube.append(s('circle', { cx: CX, cy: CY, r: (R*f).toFixed(1), fill: 'none', stroke: FT, 'stroke-width': 1, opacity: op })));
  const sa = -beamAngle;   // radar sweep spins CLOCKWISE, decoupled from tone RATE (#1/#2)
  tube.append(s('path', { d: `M ${CX} ${CY} L ${(CX+R*Math.cos(sa)).toFixed(1)} ${(CY-R*Math.sin(sa)).toFixed(1)} A ${R} ${R} 0 0 0 ${(CX+R*Math.cos(sa-.55)).toFixed(1)} ${(CY-R*Math.sin(sa-.55)).toFixed(1)} Z`, fill: 'url(#rs)' }));
  tube.append(s('line', { x1: CX, y1: CY, x2: (CX+R*Math.cos(sa)).toFixed(1), y2: (CY-R*Math.sin(sa)).toFixed(1), stroke: '#4DE699', 'stroke-width': 1.2, opacity: .5 }));
  TONES.forEach((n, i) => {
    const on = tone('on', i).get();
    const gain = on ? toneGain(i, phi) : 0;
    const a = (TONE_ANGLE[i] + (tone('dir', i).get() - .5) * 40) * Math.PI / 180;
    const rr = (((a - sa) % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
    const fade = Math.pow(1 - rr / (2 * Math.PI), 1.8);   // bright when beam just swept it, fades until next pass
    const op = +(0.12 + 0.88 * fade).toFixed(3);
    const r = R * (.28 + gain * .64), bx = +(CX + r * Math.cos(a)).toFixed(1), by = +(CY - r * Math.sin(a)).toFixed(1);
    if (!on) { tube.append(s('circle', { cx: bx, cy: by, r: 3, fill: TONE_COLORS[i], opacity: +(.04 + .14 * fade).toFixed(3) })); return; }
    const cr = 3 + gain * 3;
    tube.append(s('circle', { cx: bx, cy: by, r: cr * 2.4, fill: `url(#rg${i})`, opacity: op }));
    if (i === UI.sel) tube.append(s('circle', { cx: bx, cy: by, r: cr + 3.5, fill: 'none', stroke: TONE_COLORS[i], 'stroke-width': 1, opacity: +(.15 + .65 * fade).toFixed(3) }));
    tube.append(s('circle', { cx: bx, cy: by, r: cr, fill: `url(#rc${i})`, opacity: op }));
    const txt = s('text', { x: bx, y: by - cr - 3, 'text-anchor': 'middle', 'font-family': 'Doto', 'font-weight': 800, 'font-size': 9, fill: TONE_COLORS[i], opacity: op }); txt.textContent = n; tube.append(txt);
  });
  for (let yy = 0; yy < 128; yy += 3) tube.append(s('rect', { x: 0, y: yy, width: 150, height: 1, fill: '#000', 'fill-opacity': .1 }));
  kids.push(tube);
  kids.push(s('circle', { cx: CX, cy: CY, r: R * .225, fill: 'none', stroke: BR, 'stroke-width': 1, opacity: .3 }));
  kids.push(s('circle', { cx: CX, cy: CY, r: 4, fill: 'url(#rh)' }));
  RADAR.replaceChildren(...kids);
}
function toneGain(i, phi) { const a = tone('dir', i).get() * 2 * Math.PI; const g = Math.max(0, Math.cos(phi - a)); const vint = tone('vint', i).get(); return (1 - vint) + vint * g * g; }

let mL = 0, mR = 0, beamAngle = 0;
function tick() {
  if (glob('orbit').get() && !overlayEl) { const hz = 0.02 * Math.pow(1000, glob('orbRate').get()); glob('vphase').set((glob('vphase').get() + hz / 60) % 1); }
  beamAngle = (beamAngle + 0.0175) % (2 * Math.PI);   // steady clockwise radar sweep (velocity reduced 65%)
  const phi = glob('vphase').get() * 2 * Math.PI;
  // meters (the header spectrum is driven separately by pollScope() → getScopeData FFT)
  if (HEADER_ANIM) {
    const act = TONES.reduce((s2, n, i) => s2 + (tone('on', i).get() ? toneGain(i, phi) * tone('level', i).get() : 0), 0) / 4;
    const m = glob('master').get();
    mL = Math.max(mL * .82, m * (.45 + act * .8 + Math.random() * .12));
    mR = Math.max(mR * .82, m * (.45 + act * .8 + Math.random() * .12));
    HEADER_ANIM.mL.style.height = Math.round(Math.min(1, mL) * 100) + '%';
    HEADER_ANIM.mR.style.height = Math.round(Math.min(1, mR) * 100) + '%';
    HEADER_ANIM.limLed.classList.toggle('on', glob('limiter_on').get() && (mL > .9 || mR > .9));
  }
  drawRadar();
  requestAnimationFrame(tick);
}

/* ============================================================ FACEPLATE ==== */
function buildFaceplate() {
  const fp = h('div', 'faceplate');
  fp.append(h('div', null, { style: 'position:absolute;inset:0;background:linear-gradient(180deg,rgba(255,255,255,.10) 0%,rgba(255,255,255,.03) 25%,rgba(0,0,0,.05) 80%,rgba(0,0,0,.12) 100%)' }));
  fp.append(h('div', null, { style: 'position:absolute;inset:0;background:radial-gradient(ellipse 75% 75% at 50% 50%,rgba(0,0,0,0) 55%,rgba(0,0,0,.16) 100%)' }));
  // brushed grain via canvas
  const cv = document.createElement('canvas'); cv.width = 1140; cv.height = 660; cv.style.cssText = 'position:absolute;inset:0;width:100%;height:100%';
  const ctx = cv.getContext('2d'); let seed = 19971; const mr = () => (seed = seed * 16807 % 2147483647) / 2147483647;
  for (let y = 1; y < 660; y += 4 + Math.floor(mr() * 5)) { ctx.fillStyle = (mr() > .45 ? '255,255,255' : '0,0,0').replace(/^/, 'rgba(') ; ctx.globalAlpha = 0.008 + mr() * .024; ctx.fillStyle = (mr() > .45 ? '#fff' : '#000'); ctx.fillRect(0, y, 1140, 0.5 + mr()); }
  ctx.globalAlpha = 1; fp.append(cv);
  const screws = h('div', 'screws');
  [[22,22,37],[1118,22,112],[22,638,74],[1118,638,155]].forEach(([x, y, rot]) => {
    const sc = h('div', null, { style: `position:absolute;left:${x-8}px;top:${y-8}px;width:16px;height:16px;border-radius:50%;background:radial-gradient(circle at 38% 38%,#6b708f,#292b42 70%,#0d0d1a);box-shadow:0 1px 2px rgba(0,0,0,.6)` },
      h('div', null, { style: `position:absolute;left:3.5px;top:7.2px;width:9px;height:1.6px;border-radius:1px;background:#05050c;transform:rotate(${rot}deg)` }));
    screws.append(sc);
  });
  return [fp, screws];
}

/* header spectrum = FFT of processor output (getScopeData); flat when silent. */
function startScope() {
  const getScope = Bridge.fn('getScopeData');
  const NB = 22;
  setInterval(async () => {
    if (!HEADER_ANIM) return;
    let buf; try { buf = await getScope(1024); } catch (e) { buf = null; }
    const bars = HEADER_ANIM.specBars;
    if (!buf || !buf.length) { bars.forEach(b => b.style.height = '2px'); return; }
    const N = buf.length, step = Math.max(1, (N / 512) | 0);
    for (let k = 0; k < NB; k++) {
      const f0 = 20 * Math.pow(1000, k / (NB - 1));          // ~20 Hz .. 20 kHz, log-spaced
      const w = 2 * Math.PI * f0 / 44100;
      let re = 0, im = 0, cnt = 0;
      for (let n = 0; n < N; n += step) { re += buf[n] * Math.cos(w * n); im += buf[n] * Math.sin(w * n); cnt++; }
      const mag = Math.sqrt(re * re + im * im) / cnt;
      const db = Math.min(1, Math.max(0, (20 * Math.log10(mag + 1e-6) + 60) / 60));
      bars[k].style.height = Math.max(2, Math.round(3 + db * 27)) + 'px';
    }
  }, 50);
}

/* processor-owned data is FETCHED on boot, never hardcoded (contract §2) */
function hydrate() {
  Bridge.fn('getPresetList')().then(l => { if (l && l.length) { PRESETS.length = 0; l.forEach(p => PRESETS.push([p.category, p.name])); refreshHeader(); } }).catch(() => {});
  Bridge.fn('getUserPresetList')().then(l => { if (Array.isArray(l)) { USER_PRESETS.length = 0; l.forEach(p => USER_PRESETS.push({ name: p.name, category: p.category || 'USER', bank: 'USER' })); } }).catch(() => {});
  Bridge.fn('getWaveList')().then(l => { if (l && l.length) { WAVES.length = 0; l.forEach(w => WAVES.push([w.category, w.name, w.bank || ''])); rebuildToneEdit(); } }).catch(() => {});
}

/* scale the whole panel to fill the host window (fixes “frame grows but layout doesn’t”, #6).
 * The plugin editor only changes the window size; the panel is one fixed-geometry unit that
 * scales uniformly — exactly the JUCE AffineTransform::scale rule, done in CSS here. */
function fitToWindow() {
  const panel = document.querySelector('.panel'); if (!panel) return;
  /* Faceplate is a fixed 660px unit; the keybed extends BELOW it and must not
   * rescale or move the face. Always fit the 660 faceplate — opening the keybed
   * only reveals the bed under the blue KEYS band. */
  UI.scale = 0.8 * Math.min(window.innerWidth / 1140, window.innerHeight / 660);
  panel.style.transform = `scale(${UI.scale})`;
}
addEventListener('resize', fitToWindow);

/* keyboard bed (folded by default) — pitch/mod wheels + 30-key bed under the panel */
function buildKeyboard() {
  const frag = document.createDocumentFragment();
  const tabTxt = () => UI.kbdOpen ? '\u25BC KEYS' : '\u25B2 KEYS';
  const tabBtn = h('div', null, { style: 'width:56px;height:12px;background:#181b34;border:1px solid #464e94;border-radius:6px;cursor:pointer;box-shadow:0 1px 2px rgba(0,0,0,.5);color:#c9cdf2;font:700 6.5px var(--f-silk);display:flex;align-items:center;justify-content:center;letter-spacing:.12em' }, tabTxt());
  const tab = h('div', 'rubber', { style: 'position:absolute;left:0;top:646px;width:1140px;height:14px;z-index:7;background:linear-gradient(180deg,#414a92 0%,#333b78 50%,#232a5c 100%);border-top:1px solid rgba(150,160,220,.28);border-bottom:1px solid #0a0c1c;box-shadow:inset 0 1px 0 rgba(180,190,240,.22),inset 0 -1px 2px rgba(0,0,0,.5),0 2px 5px rgba(0,0,0,.55);display:flex;align-items:center;justify-content:center' }, tabBtn);
  const bed = h('div', 'keybed', { style: 'position:absolute;left:0;top:660px;width:1140px;height:190px;overflow:hidden;border-radius:0 0 8px 8px;background:#1a1c38;box-shadow:inset 0 1px 1px rgba(255,255,255,.1),inset 0 4px 8px rgba(0,0,0,.6)' });
  bed.append(h('div', null, { style: 'position:absolute;left:8px;top:10px;width:128px;height:170px;border-radius:6px;background:linear-gradient(180deg,#0d0d1c 0%,#171a2e 100%);box-shadow:inset 0 3px 6px rgba(0,0,0,.7)' }));
  const wheel = (leftWell, leftW, isPitch) => {
    bed.append(h('div', null, { style: `position:absolute;left:${leftWell}px;top:25px;width:40px;height:140px;border-radius:20px;background:#05050d;box-shadow:inset 0 2px 5px rgba(0,0,0,.9)` }));
    const w = h('div', null, { style: `position:absolute;left:${leftW}px;top:36px;width:32px;height:118px;border-radius:16px;background:linear-gradient(180deg,#121426 0%,#383d61 35%,#454a73 50%,#383d61 65%,#121426 100%);box-shadow:0 2px 4px rgba(0,0,0,.6);cursor:ns-resize;overflow:hidden` });
    let val = 0;
    const paint = () => {
      w.innerHTML = '';
      const ribs = isPitch ? 19 : 18;
      for (let k = 0; k < ribs; k++) { const top = isPitch ? (2 + k * 6.2 + val * 20) : (2 + k * 6.5 - val * 40); w.append(h('div', null, { style: `position:absolute;left:0;width:32px;height:2px;background:rgba(0,0,0,.55);top:${top}px` })); }
      if (isPitch) w.append(h('div', null, { style: `position:absolute;left:0;width:32px;height:3px;background:#ff2b3e;box-shadow:0 0 4px rgba(255,43,62,.7);border-radius:1px;top:${57.5 + val * 20}px` }));
    };
    paint();
    w.addEventListener('pointerdown', e => { e.preventDefault(); const sy = e.clientY, s0 = val;
      const mv = ev => { val = clamp(s0 + (sy - ev.clientY) / 118, isPitch ? -1 : 0, 1); touch(isPitch ? 'PITCH BEND' : 'MOD WHEEL', isPitch ? (val + 1) / 2 : val); paint(); };
      const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); if (isPitch) { val = 0; paint(); } };
      addEventListener('pointermove', mv); addEventListener('pointerup', up); });
    bed.append(w);
  };
  wheel(24, 28, true); wheel(76, 80, false);
  bed.append(h('div', null, { style: 'position:absolute;left:22px;top:12px;font:600 7px var(--f-silk);color:#9aa1d8;letter-spacing:.14em' }, 'PITCH'));
  bed.append(h('div', null, { style: 'position:absolute;left:81px;top:12px;font:600 7px var(--f-silk);color:#9aa1d8;letter-spacing:.14em' }, 'MOD'));
  const keys = h('div', null, { style: 'position:absolute;left:144px;top:0;right:0;bottom:0' });
  const press = el => e => { e.preventDefault(); const orig = el.style.background; el.style.background = 'linear-gradient(180deg,#ffe9a0,#ffd23f)'; touch('NOTE ON', .5); setTimeout(() => el.style.background = orig, 200); };
  for (let k = 0; k < 30; k++) { const wk = h('div', null, { style: `position:absolute;top:0;width:32.662px;height:170px;border-radius:0 0 3px 3px;box-shadow:1px 0 1px rgba(0,0,0,.35),inset 0 -3px 3px rgba(0,0,0,.25);cursor:pointer;left:${(k * 32.662).toFixed(2)}px;background:linear-gradient(180deg,#f0f1fb 0%,#c7cae0 100%)` }); wk.addEventListener('pointerdown', press(wk)); keys.append(wk); }
  Array.from({ length: 30 }, (_, k) => k).filter(k => [0, 1, 3, 4, 5].includes(k % 7) && k < 29).slice(0, 21).forEach(k => { const bk = h('div', null, { style: `position:absolute;top:0;width:20px;height:104px;border-radius:0 0 3px 3px;box-shadow:0 3px 5px rgba(0,0,0,.5);cursor:pointer;z-index:2;left:${(k * 32.662 + 22.5).toFixed(2)}px;background:linear-gradient(180deg,#26283e 0%,#0c0d18 100%)` }); bk.addEventListener('pointerdown', press(bk)); keys.append(bk); });
  bed.append(keys);
  bed.style.display = UI.kbdOpen ? 'block' : 'none';
  tabBtn.addEventListener('click', () => { UI.kbdOpen = !UI.kbdOpen; const panel = document.querySelector('.panel'); if (panel) panel.classList.toggle('kbd-open', UI.kbdOpen); bed.style.display = UI.kbdOpen ? 'block' : 'none'; tabBtn.textContent = tabTxt(); fitToWindow(); });
  frag.append(tab, bed);
  return frag;
}

/* ============================================================ BUILD ======== */
function build() {
  const panel = h('div', 'panel');
  const [fp, screws] = buildFaceplate();
  panel.append(fp, screws);
  panel.append(buildHeader());
  const row1 = h('div', null, { style: 'display:grid;grid-template-columns:172px 1fr 200px;gap:10px;padding:14px 12px 0;height:352px' });
  toneEditHost = buildToneEdit();
  row1.append(buildTones(), toneEditHost, buildFilters());
  panel.append(row1);
  const row2 = h('div', null, { style: 'display:grid;grid-template-columns:376px 276px 1fr;gap:10px;padding:14px 12px 0;height:212px' });
  fxHost = buildFX();
  row2.append(buildVector(), buildMatrix(), fxHost);
  panel.append(row2);
  panel.append(buildKeyboard());
  panel.append(h('div', 'grip', { onpointerdown: gripResize, ondblclick: () => { UI.scale = 1; panel.style.transform = 'scale(1)'; } }));
  panel.append(h('div', null, { style: "position:absolute;right:34px;top:626px;font:600 7.5px var(--f-silk);color:var(--silk-dim);letter-spacing:.16em;z-index:6" }, 'VER 1.0'));
  document.getElementById('root').append(panel);
  hydrate(); startScope(); fitToWindow(); drawRadar();
  requestAnimationFrame(tick);
}
function gripResize(e) {
  e.preventDefault(); const sy = e.clientY, s0 = UI.scale, panel = document.querySelector('.panel');
  const mv = ev => { UI.scale = clamp(s0 + (ev.clientY - sy) / 660, .5, 2); panel.style.transform = `scale(${UI.scale})`; };
  const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); };
  addEventListener('pointermove', mv); addEventListener('pointerup', up);
}

if (document.readyState !== 'loading') build(); else addEventListener('DOMContentLoaded', build);
