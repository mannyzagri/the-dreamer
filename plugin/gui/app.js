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
    /* UI-local state for ids with NO backing APVTS param (ui_global_offset).
     * Wrapping these in a relay when live binds them to a dead endpoint that
     * never receives values — route them to the local store instead so they
     * behave as plain UI state. ⚠ GUI-Claude fold upstream. */
    local(id, def) { return mockState(id, def); },
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
const AUXDESTS = ['PITCH','START','SHAPE','PAN','NOISE','BALANCE'];   /* +BALANCE (choice-6 aux_dest) — USER-STATED v18 engine add, ⚠ GUI-Claude fold upstream */
const LFODESTS = ['PITCH','CUTOFF','SHAPE','LEVEL','BALANCE'];        /* +BALANCE (choice-5 lfo*_dest) — USER-STATED v18 engine add, ⚠ GUI-Claude fold upstream */
const WAVE_SHAPES = ['TRI','SIN','SAW','SQR','S+H'];
const VOICINGS = ['SINGLE','OCT','POWER','DREAMY'];
const SPREADS  = ['ADD9','MIN7','SUS2'];
const LOOPMODES = ['FWD','PINGPONG'];
const PLAYMODES = ['NORMAL','STRETCH'];
const MODFX = ['CHORUS','FLANGER','PHASER','ENSEMBLE','DIMENSION','ROTARY','BARBERPOLE'];   /* choice-7 parity with modfx_type — ⚠ GUI-Claude fold upstream */
const DLYMODES = ['DIGITAL','TAPE','PONG'];
const REVTYPES = ['ROOM','HALL','PLATE'];
const MSRC = ['G-LFO 1','AUX','VELO','WHEEL','G-LFO 2','G-AUX'];
const MDST = ['PITCH','CUT 1','CUT 2','MORPH','SHAPE','PAN','NOISE','FX PARAM','LOOP RATE'];
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
  tvfType: 0, noise: 0, noiseCol: .5, startRnd: false, voicing: 0, spread: 0,
  oct2: .5, semi2: .5, fine2: .5, start2: 0, velo2: .4, startRnd2: false,
  voicing2: 0, spread2: 0, wbal: 0,
  detVoices: 0, detAmt: 0, loopMode: 0, hitPlay: 0, hitStretch: .5, hitTrim: .5,
  loopRate: .5, loopSync: false, loopBeats: 5, loopVari: false,
  l1r: .4, l1d: .15, l1sync: false, l1dest: 0, l1shape: 0,
  l2r: .25, l2d: 0, l2sync: false, l2dest: 1, l2shape: 2, auxAmt: .5, auxDest: 0,
  cut: 1, res: .25, env: .5, kf: .5,
  fa: .05, fd: .6, fs: .5, fr: .55, aa: .2, ad: .7, as: .8, ar: .65, xa: 0, xd: .4, xs: 0, xr: .3,
  ampOvr: false, filtOvr: false, auxOvr: false,
};
/* map UI key -> APVTS id stem (kept explicit so ids match DSP_BUILD/Params.h) */
const TONE_ID = {
  oct:'oct', semi:'semi', fine:'fine', start:'start', level:'level', velo:'velo', pan:'pan',
  shape:'shape', shapeDepth:'shape_depth', tvfType:'tvf_type',
  noise:'noise', noiseCol:'noise_color', startRnd:'start_random', voicing:'voicing', spread:'dreamy_spread',
  wave2:'wave2', oct2:'oct2', semi2:'semi2', fine2:'fine2', start2:'start2', velo2:'velo2',
  startRnd2:'start2_random', voicing2:'voicing2', spread2:'dreamy_spread2', wbal:'wave_balance',
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
  master: .78, f1Cut: 1, f1Res: .35, f1Env: .5, f2Cut: 1, f2Res: .2, f2Env: .5, f2Morph: .4,
  route: 0, fbal: .5, f1Type: 0, f2Type: 2,
  glfoRate: .4, glfoWave: 0, glfoSync: false, glfo2Rate: .25, glfo2Wave: 1, glfo2Sync: false,
  mfxType: 0, mfxRate: .3, mfxDepth: .5, mfxMix: .45, mfxParam: .5, mfxOn: true, mfxFocus: 0,
  dlyMode: 0, dTime: .5, dFb: .35, dMix: .3, dParam: .4, dlyOn: true, dlySync: false, dlyFocus: 0,
  revType: 1, rSize: .6, rDamp: .4, rMix: .35, rParam: .5, revOn: true, revFocus: 0,
  lofiOn: false, lofiParam: .5, lofiFocus: 0, widthOn: true, widthAmt: .55, haas: .3, bassMono: false,
  talkOn: false, talkParam: .5, talkFocus: 0, prePost: 0, limiter_on: true,
  gEnvA: .5, gEnvD: .5, gEnvS: .5, gEnvR: .5, gCutoff: .5, gRes: .5, gOctave: .5, globOffset: false,
  lofiBits: .5, lofiSrate: .5, lofiCompand: .5, lofiAlias: .5,
  gAmpA: .10, gAmpD: .30, gAmpS: .80, gAmpR: .35,
  gFltA: .04, gFltD: .45, gFltS: .30, gFltR: .50,
  gAuxA: .20, gAuxD: .40, gAuxS: .60, gAuxR: .55,
};
const GLOBAL_ID = {
  master:'master', f1Cut:'flt1_cut', f1Res:'flt1_res', f1Env:'flt1_env', f2Cut:'flt2_cut',
  f2Res:'flt2_res', f2Env:'flt2_env', f2Morph:'flt2_morph', route:'flt_route', fbal:'flt_bal',
  f1Type:'flt1_type', f2Type:'flt2_type', glfoRate:'lfo_rate',
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
  lofiBits:'lofi_bits', lofiSrate:'lofi_srate', lofiCompand:'lofi_compand', lofiAlias:'lofi_alias',
  gAmpA:'gamp_env_a', gAmpD:'gamp_env_d', gAmpS:'gamp_env_s', gAmpR:'gamp_env_r',
  gFltA:'gflt_env_a', gFltD:'gflt_env_d', gFltS:'gflt_env_s', gFltR:'gflt_env_r',
  gAuxA:'gaux_env_a', gAuxD:'gaux_env_d', gAuxS:'gaux_env_s', gAuxR:'gaux_env_r',
};
const KIND = {  // relay kind per param key (default = slider/continuous)
  route:'toggle', glfoSync:'toggle', mfxOn:'toggle', dlyOn:'toggle',
  dlySync:'toggle', revOn:'toggle', lofiOn:'toggle', widthOn:'toggle', bassMono:'toggle', talkOn:'toggle',
  prePost:'toggle', limiter_on:'toggle', globOffset:'toggle', glfo2Sync:'toggle',
  f1Type:'choice', f2Type:'choice', glfoWave:'choice', glfo2Wave:'choice', mfxType:'choice',
  dlyMode:'choice', revType:'choice', mfxFocus:'choice', dlyFocus:'choice', revFocus:'choice',
  lofiFocus:'choice', talkFocus:'choice',
};
const TONE_KIND = {
  startRnd:'toggle', startRnd2:'toggle', l1sync:'toggle', l2sync:'toggle', loopSync:'toggle', loopVari:'toggle', on:'toggle',
  shape:'choice', tvfType:'choice', voicing:'choice', spread:'choice', voicing2:'choice', spread2:'choice', l1dest:'choice', l2dest:'choice',
  l1shape:'choice', l2shape:'choice', auxDest:'choice', loopMode:'choice', hitPlay:'choice',
  loopBeats:'choice', wave:'choice', wave2:'choice', detVoices:'choice',
  ampOvr:'toggle', filtOvr:'toggle', auxOvr:'toggle',
};

/* Param handle cache. glob(key) / tone(key, toneIdx). */
const _cache = new Map();
/* globOffset: ui_global_offset has NO backing APVTS param (B1) — as a live
 * relay it bound a dead endpoint (LED never lit, state lost). Session-local
 * via Bridge.local. NOTE v18: the global-env tier (gamp/gflt/gaux_env_*) and
 * the *_ovr flags ARE real APVTS params now — they bind as normal relays.
 * ⚠ GUI-Claude fold upstream. */
function glob(key) {
  const id = GLOBAL_ID[key];
  if (!_cache.has(id)) _cache.set(id, key === 'globOffset'
    ? Bridge.local(id, GLOBAL_DEFAULTS[key])
    : Bridge.state(KIND[key] || 'slider', id, GLOBAL_DEFAULTS[key]));
  return _cache.get(id);
}
const WAVE_DEFAULTS = [3, 19, 27, 37];   // A/B/C/D initial wave index
function toneDefault(key, ti) {
  if (key === 'wave' || key === 'wave2') return WAVE_DEFAULTS[ti];
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
  presetSel: { bank: 'FACTORY', i: 0 },
  renameBuf: '', preset: 0, midiLearn: false, kbdOpen: false, scale: 1, envDest: 'AMP', envAll: false,
  headerName: null };   /* headerName: override for the header preset display (INIT / user preset / host push) — ⚠ GUI-Claude fold upstream */
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

  /* TD-011: stepper cycles FACTORY then the LIVE user bank (USER_PRESETS read at click, wrap both ways); position derives from UI.presetSel so browser-initiated loads and refreshUserBank() mutations carry through. \u26A0 GUI-Claude fold upstream. */
  const stepPreset = d => {
    const nf = PRESETS.length, total = nf + USER_PRESETS.length;
    let cur = UI.presetSel.bank === 'USER' ? nf + UI.presetSel.i : UI.presetSel.i;
    if (cur < 0 || cur >= total) cur = Math.min(Math.max(UI.preset, 0), nf - 1);
    const n = (cur + d + total) % total;
    if (n < nf) {
      UI.preset = n; UI.presetSel = { bank: 'FACTORY', i: n }; UI.headerName = null;
      Bridge.fn('loadPreset')(n); refreshHeader();
    } else {
      const p = USER_PRESETS[n - nf];
      UI.presetSel = { bank: 'USER', i: n - nf }; UI.renameBuf = p.name;
      Bridge.fn('loadUserPreset')(p.name).then(() => { UI.headerName = p.name; refreshHeader(); });
    }
  };
  const steppers = h('div', 'col', { style: 'gap:3px;margin-right:6px' },
    h('div', 'step', { style: 'width:22px;height:19px', onclick: () => stepPreset(-1) }, '\u25B2'),
    h('div', 'step', { style: 'width:22px;height:19px', onclick: () => stepPreset(1) }, '\u25BC'));

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
    /* TD-010/B9: headerName override (user preset / INIT / host push) — ⚠ GUI-Claude fold upstream */
    presetName.textContent = UI.headerName != null ? UI.headerName
      : 'P' + String(UI.preset + 1).padStart(3, '0') + ' ' + (PRESETS[UI.preset] ? PRESETS[UI.preset][1] : '');
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
  /* C1: the wave-class compartment (ENS loop cluster / SHOT stretch) is chosen
   * at build — an EXTERNAL wave/hit_play change (preset load, host automation)
   * left the old class on screen. Rebuild only when the built class/hitPlay
   * actually changed and this tone is still shown (no rebuild loops: read-only
   * guards, rebuild sets nothing). ⚠ GUI-Claude fold upstream. */
  const builtClass = (w() && w()[2]) || '', builtHit = P('hitPlay').get();
  P('wave').sub(() => { if (UI.sel === i && (((w() && w()[2]) || '') !== builtClass)) rebuildToneEdit(); });
  P('hitPlay').sub(v => { if (UI.sel === i && v !== builtHit) rebuildToneEdit(); });
  const semiFmt = v => { const n = Math.round((v - .5) * 24); return (n >= 0 ? '+' : '') + n + ' st'; };

  // ---- WAVE 1 | WAVE 2 twin columns (v18)
  const waveCol = (label, waveKey, keys, rndKey) => {
    const lcd = LcdMenu(P(waveKey), WAVES, label + ' SELECT \u2014 TONE ' + TONES[i], 'width:170px;height:20px', true);
    const row = h('div', 'row', { style: 'gap:6px' },
      h('div', 'lbl', { style: 'color:' + col + ';width:42px' }, label), lcd,
      h('div', 'step', { onclick: () => cyc(P(waveKey), WAVES.length, -1) }, '\u2039'),
      h('div', 'step', { onclick: () => cyc(P(waveKey), WAVES.length, 1) }, '\u203a'));
    const rnd = P(rndKey);
    const knobs = h('div', 'row', { style: 'gap:4px;width:288px;margin-left:22px;align-items:flex-start' },
      Knob(P(keys[0]), 'OCTAVE', { def: .5 }),
      Knob(P(keys[1]), 'SEMI', { bip: true, def: .5, fmt: semiFmt }),
      Knob(P(keys[2]), 'FINE', { bip: true, def: .5 }),
      Knob(P(keys[3]), 'START', { def: 0 }),
      Knob(P(keys[4]), 'VELOCITY', { def: .4 }),
      h('div', 'col', { style: 'align-items:center;gap:3px;width:26px;padding-top:4px' }, Led(rnd),
        h('div', 'btn', { style: 'width:22px;height:14px', onclick: () => { const v = !rnd.get(); rnd.set(v); touch('START RANDOM', v ? 1 : 0); } }),
        h('div', 'lbl-sm', null, 'RND')));
    return h('div', 'col', { style: 'gap:9px' }, row, knobs);
  };
  const wave2Col = waveCol('WAVE 2', 'wave2', ['oct2','semi2','fine2','start2','velo2'], 'startRnd2');
  wave2Col.style.marginLeft = '12px';
  const waveWrap = h('div', 'row', { style: 'align-items:flex-start' },
    waveCol('WAVE 1', 'wave', ['oct','semi','fine','start','velo'], 'startRnd'),
    h('div', 'divider-v', { style: 'margin-left:24px' }), wave2Col);

  // ---- FILTER (per-tone, full 14-type list) | SHAPE columns
  const off = glob('globOffset').get();
  const filterHeader = h('div', 'row', { style: 'gap:6px;align-items:center' },
    h('div', 'lbl', { style: 'color:' + col + ';width:42px' }, 'FILTER'),
    LcdMenu(P('tvfType'), FTYPES, 'TVF TYPE \u2014 TONE ' + TONES[i], 'width:170px;height:20px'),
    h('div', 'step', { onclick: () => cyc(P('tvfType'), FTYPES.length, -1) }, '\u2039'),
    h('div', 'step', { onclick: () => cyc(P('tvfType'), FTYPES.length, 1) }, '\u203a'));
  const filterKnobs = h('div', 'row', { style: 'gap:4px;margin-left:22px;width:288px' },
    off ? Knob(glob('gCutoff'), 'CUT \u00b1', { color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip }) : Knob(P('cut'), 'CUTOFF', { color: 'var(--ptr-red)', def: 1 }),
    off ? Knob(glob('gRes'), 'RES \u00b1', { color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip }) : Knob(P('res'), 'RESONANCE', { color: 'var(--ptr-red)', def: .25 }),
    Knob(P('env'), 'ENVELOPE', { color: 'var(--ptr-red)', def: .5 }),
    Knob(P('kf'), 'KEY FLW', { color: 'var(--ptr-red)', bip: true, def: .5, fmt: fmtBip }));
  const filterSection = h('div', 'col', { style: 'gap:9px' }, filterHeader, filterKnobs);

  const playBtn = h('div', 'btn', { style: 'width:66px;height:20px', onclick: () => { const v = 1 - P('hitPlay').get(); P('hitPlay').set(v); touch('PLAY MODE', v); rebuildToneEdit(); } });
  const paintPlay = () => playBtn.textContent = PLAYMODES[P('hitPlay').get()]; paintPlay(); P('hitPlay').sub(paintPlay);
  const shapeHeader = h('div', 'row', { style: 'gap:6px;align-items:center' },
    h('div', 'lbl', { style: 'color:' + col + ';width:42px' }, 'SHAPE'),
    LcdMenu(P('shape'), SHAPES, 'SHAPER \u2014 TONE ' + TONES[i], 'width:96px;height:20px'),
    h('div', 'step', { onclick: () => cyc(P('shape'), SHAPES.length, -1) }, '\u2039'),
    h('div', 'step', { onclick: () => cyc(P('shape'), SHAPES.length, 1) }, '\u203a'),
    h('div', null, { style: 'width:24px' }),
    h('div', 'row', { style: 'gap:5px' }, Led(P('hitPlay')), L('PLAY'), playBtn));

  let comp = null, loopComp = null, loopGrp = null;
  if (isENS() && P('hitPlay').get() === 1) {
    const synced = P('loopSync').get();
    const rate = Knob(P('loopRate'), synced ? SYNCDIVS[P('loopBeats').get()] : 'LOOP RATE', { color: 'var(--ptr-yellow)', def: .5, fmt: v => Math.pow(4, (v - .5) * 2).toFixed(2) + '\u00d7' });
    /* C2: label snapshot \u2014 track EXTERNAL sync/beats changes (GUI sync click
     * already rebuilds). \u26a0 GUI-Claude fold upstream. */
    P('loopSync').sub(v => { if (UI.sel === i && !!v !== !!synced) rebuildToneEdit(); });
    P('loopBeats').sub(b => { if (P('loopSync').get()) rate._setLabel(SYNCDIVS[b]); });
    const syncBtn = h('div', 'btn', { style: 'width:30px;height:14px', onclick: () => { const v = !P('loopSync').get(); P('loopSync').set(v); touch('LOOP SYNC', v ? 1 : 0); rebuildToneEdit(); } }, 'SYNC');
    const beats = Stepper(P('loopBeats'), SYNCDIVS, { lcd: true, readStyle: 'width:34px;height:14px', arrowStyle: 'width:13px;height:14px', label: 'LOOP BEATS' });
    const variBtn = h('div', 'btn', { style: 'width:76px;height:14px', onclick: () => { const v = !P('loopVari').get(); P('loopVari').set(v); touch('VARISPEED', v ? 1 : 0); } }, 'VARISPEED');
    loopComp = h('div', 'row', { style: 'gap:8px;margin-left:20px' }, rate,
      h('div', 'col', { style: 'gap:4px' },
        h('div', 'row', { style: 'gap:3px' }, Led(P('loopSync')), syncBtn, beats),
        h('div', 'row', { style: 'gap:3px' }, Led(P('loopVari')), variBtn)));
  } else {
    /* TD-009: CYCLE+STRETCH is real varispeed (same hit_stretch/hit_pitchtrim) —
     * live STRETCH/P.TRIM for any non-ENS wave in STRETCH mode (USER-ORDERED,
     * overrides the spec's HIT-only grey rule); NORMAL stays greyed.
     * ⚠ GUI-Claude fold upstream. */
    const active = !isENS() && P('hitPlay').get() === 1;
    comp = h('div', 'row', { style: 'gap:4px;margin-left:8px' + (active ? '' : ';opacity:.35;pointer-events:none') },
      Knob(P('hitStretch'), 'STRETCH'), Knob(P('hitTrim'), 'P.TRIM'));
  }
  if (isENS()) {
    const lmBtn = h('div', 'btn', { style: 'width:66px;height:20px', onclick: () => { const v = 1 - P('loopMode').get(); P('loopMode').set(v); touch('LOOP MODE', v); } });
    const paintLm = () => lmBtn.textContent = LOOPMODES[P('loopMode').get()]; paintLm(); P('loopMode').sub(paintLm);
    loopGrp = h('div', 'row', { style: 'gap:5px;margin-left:20px;align-self:flex-start;padding-top:7px' }, Led(P('loopMode')), L('LOOP'), lmBtn);
  }
  const shapeKnobs = h('div', 'row', { style: 'gap:4px;margin-left:22px;align-items:flex-start' },
    Knob(P('shapeDepth'), 'DEPTH', { color: 'var(--ptr-yellow)' }), comp,
    Knob(P('noise'), 'NOISE'), Knob(P('noiseCol'), 'COLOR'), loopGrp, loopComp);
  const shapeSection = h('div', 'col', { style: 'gap:9px;margin-left:12px' }, shapeHeader, shapeKnobs);
  const tvfRow = h('div', 'row', { style: 'align-items:flex-start' },
    filterSection, h('div', 'divider-v', { style: 'margin-left:24px' }), shapeSection);

  // ---- VOICE 1 \u00b7 WAVE-BALANCE \u00b7 VOICE 2 (v18)
  const voiceSel = (vKey, sKey, label) => {
    const voicing = h('div', 'lcd click', { style: 'width:54px;height:16px', onclick: () => openMenu('VOICING \u2014 TONE ' + TONES[i], VOICINGS, P(vKey)) });
    const spread = h('div', 'lcd click', { style: 'width:44px;height:16px', onclick: () => openMenu('DREAMY SPREAD \u2014 TONE ' + TONES[i], SPREADS, P(sKey)) });
    const pv = () => { voicing.textContent = VOICINGS[P(vKey).get()]; spread.style.display = P(vKey).get() === 3 ? 'flex' : 'none'; };
    pv(); P(vKey).sub(pv);
    const ps = () => spread.textContent = SPREADS[P(sKey).get()]; ps(); P(sKey).sub(ps);
    return [h('div', 'lbl-hi', { style: 'width:38px;color:' + col }, label), voicing, spread];
  };
  const wbal = Knob(P('wbal'), 'WAVE 1 \u00b7 WAVE 2', { size: 40, bip: true, def: 0 });
  wbal._knob.classList.add('beige'); wbal.style.margin = '0 14px';
  if (wbal.lastChild) { wbal.lastChild.style.color = '#e8dfc6'; wbal.lastChild.style.whiteSpace = 'nowrap'; }
  const voiceRow = h('div', 'row', { style: 'gap:8px' },
    ...voiceSel('voicing', 'spread', 'VOICE 1'), wbal, ...voiceSel('voicing2', 'spread2', 'VOICE 2'), h('div', 'grow'));

  // ---- LFO rows (bottom-aligned with the GLOBAL block lower border)
  const lfoRow = (n, rk, dk, sk, dk2, shk) => {
    const synced = P(sk).get();
    /* division law = engine's round(v*11) (DreamVoice.h toneLfoDivisionBeats) —
     * floor(v*12) disagreed at boundary values. ⚠ GUI-Claude fold upstream. */
    const rate = Knob(P(rk), synced ? SYNCDIVS[Math.round(P(rk).get() * 11)] : 'RATE', { size: 26, color: synced ? 'var(--ptr-yellow)' : null });
    /* C2: label snapshot — external sync change rebuilds the row (color+label);
     * rate motion while synced re-labels with round(v*11). ⚠ GUI-Claude fold upstream. */
    P(sk).sub(v => { if (UI.sel === i && !!v !== !!synced) rebuildToneEdit(); });
    P(rk).sub(v => { if (P(sk).get()) rate._setLabel(SYNCDIVS[Math.round(v * 11)]); });
    const syncBtn = h('div', 'btn', { style: 'width:32px;height:15px', onclick: () => { const v = !P(sk).get(); P(sk).set(v); touch(n + ' SYNC', v ? 1 : 0); rebuildToneEdit(); } }, 'SYNC');
    const shapeStep = Stepper(P(shk), WAVE_SHAPES, { lcd: true, readStyle: 'width:34px;height:16px', arrowStyle: 'width:14px;height:16px', label: n + ' SHAPE', onLcd: () => openMenu(n + ' SHAPE \u2014 TONE ' + TONES[i], WAVE_SHAPES, P(shk)) });
    const dest = h('div', 'btn', { style: 'width:56px;height:16px', onclick: () => openMenu(n + ' DEST \u2014 TONE ' + TONES[i], LFODESTS, P(dk2)) });
    const paintDest = () => dest.textContent = LFODESTS[P(dk2).get()]; paintDest(); P(dk2).sub(paintDest);
    return h('div', 'row', { style: 'gap:8px' }, h('div', 'lbl-hi', { style: 'width:32px;color:' + col }, n),
      rate, Knob(P(dk), 'DEPTH', { size: 26 }), shapeStep,
      h('div', 'row', { style: 'gap:4px' }, Led(P(sk)), syncBtn), L('DEST'), dest);
  };
  const lfoWrap = h('div', 'col', { style: 'gap:9px;margin-top:auto;margin-bottom:2px' },
    lfoRow('LFO1', 'l1r', 'l1d', 'l1sync', 'l1dest', 'l1shape'),
    lfoRow('LFO2', 'l2r', 'l2d', 'l2sync', 'l2dest', 'l2shape'));

  // ---- GLOBAL sub-block (UNISON stepper reads OFF at 1 voice)
  /* v18 USER-STATED semantics: UNISON/DETUNE are TRUE-GLOBAL \u2014 the stepper/knob
   * write detune_voices/detune_amount to ALL FOUR tones and display tone A's
   * value (same all-tones write-through idiom as the envelope editor's ALL
   * mode; the params stay per-tone in the APVTS). \u26a0 GUI-Claude fold upstream. */
  const allTones = key => ({
    get: () => tone(key, 0).get(),
    set: v => TONES.forEach((_, t) => tone(key, t).set(v)),
    sub: f => tone(key, 0).sub(f),
  });
  const detV = allTones('detVoices'), detA = allTones('detAmt');
  const detVoices = Stepper(detV, ['OFF','2','3','4'], { lcd: true, readStyle: 'width:34px;height:16px', arrowStyle: 'width:13px;height:16px', clamp: [0, 3], label: 'UNISON VOICES' });
  const detKnob = Knob(detA, 'DETUNE', { size: 30, color: 'var(--ptr-yellow)', def: 0, fmt: v => '\u00b1' + Math.round(v * 25) + ' c' });
  const dimDet = () => detKnob.classList.toggle('dim', detV.get() === 0);
  dimDet(); detV.sub(dimDet);
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
  const globalBlock = h('div', 'group', { style: 'position:absolute;right:12px;bottom:10px;width:321px;padding:13px 10px 8px;display:flex;flex-direction:column;gap:9px;background:rgba(16,18,40,.45)' },
    h('div', 'group-title', { style: 'background:#2a2f56;font-size:9px' }, 'GLOBAL'),
    h('div', 'row', { style: 'gap:9px' },
      h('div', 'col', { style: 'align-items:center;gap:3px' }, detVoices, h('div', 'lbl-sm', null, 'UNISON')),
      detKnob, h('div', 'divider-v'), gOctStep),
    h('div', 'row', { style: 'gap:7px' }, offLed, offBtn,
      h('div', null, { style: 'font:600 6.5px var(--f-silk);color:var(--silk-dim);line-height:1.15' }, 'AMP ADSR + CUT/RES \u2192 GLOBAL \u00b1')));

  const block = grp('TONE EDIT', 'padding:16px 12px 8px;display:flex;flex-direction:column;gap:9px',
    waveWrap, tvfRow, voiceRow, lfoWrap, globalBlock);
  block.querySelector('.group-title').append(' ', h('span', null, { style: 'color:' + col }, TONES[i]));
  return block;
}
function cyc(handle, len, d) { handle.set((handle.get() + d + len) % len); }

/* ---- FILTERS -------------------------------------------------------------- */
function buildFilters() {
  const strip = (n, typeKey, cutKey, resKey, thirdKey, thirdLabel, thirdColor, thirdBip) => {
    const typeLcd = h('div', 'lcd click grow', { style: 'height:20px', onclick: () => openMenu('FILTER ' + n + ' TYPE', TVFTYPES, glob(typeKey)) });
    const paint = () => typeLcd.textContent = TVFTYPES[glob(typeKey).get()]; paint(); glob(typeKey).sub(paint);
    const typeRow = h('div', 'row', { style: 'gap:5px;width:100%' }, h('div', null, { style: 'font:700 9px var(--f-silk);color:var(--silk-hi);width:10px' }, String(n)),
      h('div', 'step', { onclick: () => cyc(glob(typeKey), TVFTYPES.length, -1) }, '\u2039'), typeLcd, h('div', 'step', { onclick: () => cyc(glob(typeKey), TVFTYPES.length, 1) }, '\u203a'));
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

/* ---- ENVELOPES (v18: shared per-tone + global editor; replaces DREAM VECTOR) */
function buildEnvelopes() {
  const ENV_ADSR = { AMP:['aa','ad','as','ar'], FILT:['fa','fd','fs','fr'], AUX:['xa','xd','xs','xr'] };
  const ENV_GLOB = { AMP:['gAmpA','gAmpD','gAmpS','gAmpR'], FILT:['gFltA','gFltD','gFltS','gFltR'], AUX:['gAuxA','gAuxD','gAuxS','gAuxR'] };
  const ENV_OVR  = { AMP:'ampOvr', FILT:'filtOvr', AUX:'auxOvr' };
  const ENV_ALLC = '#B48CFF';
  const ENV_ROUTE_TXT = { AMP:'to voice amplitude', FILT:'to filter ENV AMT', AUX:'assignable MOD MATRIX source' };
  const lblCell = t => h('div', null, { style: 'font:700 8px var(--f-silk);color:var(--silk);letter-spacing:.16em;width:36px' }, t);
  const routeBtns = h('div', 'row', { style: 'gap:6px' });
  const toneBtns = h('div', 'row', { style: 'gap:6px' });
  const curve = s('svg', { viewBox: '0 0 300 100', preserveAspectRatio: 'none', style: 'position:absolute;inset:0;width:100%;height:100%' });
  const title = h('div', null, { style: 'position:absolute;top:5px;left:8px;font:700 8px var(--f-silk);letter-spacing:.1em' });
  const sub = h('div', null, { style: 'position:absolute;bottom:4px;left:8px;font:600 6.5px var(--f-silk);color:#6d7a2a;letter-spacing:.05em' });
  const lcd = h('div', 'grow', { style: 'position:relative;background:#0b0d05;border:1px solid #3a3410;border-radius:4px;overflow:hidden;box-shadow:inset 0 2px 10px rgba(0,0,0,.75)' }, curve, title, sub);
  const sliders = h('div', 'row', { style: 'gap:11px;align-items:flex-end;padding-right:2px' });
  const auxDest = h('div', 'btn', { style: 'width:56px;height:16px', onclick: () => openMenu('AUX ENV DEST \u2014 TONE ' + TONES[UI.sel], AUXDESTS, tone('auxDest', UI.sel)) });
  const auxAmtHost = h('div', 'row', { style: 'gap:4px' });
  const hint = h('div', null, { style: 'font:600 6.5px var(--f-silk);color:var(--silk-dim);letter-spacing:.03em;text-align:right' });
  hint.innerHTML = 'FILT \u2192 ENV AMT \u00b7 AUX \u2192 MATRIX<br>\u25cf = TONE OVERRIDE';
  const T = k => tone(k, UI.sel);
  const envVal = (dest, all, ti, idx) => (all || !tone(ENV_OVR[dest], ti).get()) ? glob(ENV_GLOB[dest][idx]).get() : tone(ENV_ADSR[dest][idx], ti).get();
  function commitEnv(idx, v) {
    const dest = UI.envDest, all = UI.envAll, ov = ENV_OVR[dest], gk = ENV_GLOB[dest], pk = ENV_ADSR[dest];
    if (all) glob(gk[idx]).set(v);
    else { if (!T(ov).get()) { pk.forEach((k, j) => T(k).set(glob(gk[j]).get())); T(ov).set(true); } T(pk[idx]).set(v); }
    touch(dest + ' ' + ['A','D','S','R'][idx], v);
  }
  const TOP = 8, BOT = 92, X0 = 6;
  const curvePts = (a, d, su, r) => { let x = X0; const p = [[x, BOT]]; x += 8 + a * 78; p.push([x, TOP]); x += 8 + d * 74; const sy = BOT - (BOT - TOP) * su; p.push([x, sy]); x += 66; p.push([x, sy]); x += 8 + r * 80; p.push([x, BOT]); return p; };
  const ptsStr = p => p.map(q => q[0].toFixed(1) + ',' + q[1].toFixed(1)).join(' ');
  function paintCurve() {
    const dest = UI.envDest, all = UI.envAll, i = UI.sel, color = all ? ENV_ALLC : TONE_COLORS[i];
    curve.innerHTML = '';
    [33, 66].forEach(y => curve.append(s('line', { x1: 0, y1: y, x2: 300, y2: y, stroke: '#1c2109', 'stroke-width': 1 })));
    // ghost underlays: every OTHER envelope on this route, dashed, in its identity color
    const ghost = (vals, gcol) => curve.append(s('polyline', { points: ptsStr(curvePts(vals[0], vals[1], vals[2], vals[3])), fill: 'none', stroke: gcol, 'stroke-width': 1.5, opacity: .38, 'stroke-dasharray': '4 3', 'stroke-linejoin': 'round' }));
    if (!all) ghost(ENV_GLOB[dest].map(k => glob(k).get()), ENV_ALLC);
    TONES.forEach((n, ti) => { if ((all || ti !== i) && tone(ENV_OVR[dest], ti).get()) ghost([0,1,2,3].map(idx => tone(ENV_ADSR[dest][idx], ti).get()), TONE_COLORS[ti]); });
    const p = curvePts(envVal(dest, all, i, 0), envVal(dest, all, i, 1), envVal(dest, all, i, 2), envVal(dest, all, i, 3));
    const line = ptsStr(p);
    curve.append(s('polygon', { points: X0 + ',' + BOT + ' ' + line, fill: color, opacity: .14 }));
    curve.append(s('polyline', { points: line, fill: 'none', stroke: color, 'stroke-width': 2.5, 'stroke-linejoin': 'round' }));
    const xA = p[1][0], xS = p[2][0], syC = p[2][1], xR = p[4][0];
    const mkHandle = (cx, cy, onMove) => {
      const hit = s('circle', { cx, cy, r: 9, fill: 'transparent', style: 'cursor:grab' });
      hit.addEventListener('pointerdown', e => {
        e.preventDefault(); e.stopPropagation();
        const rect = curve.getBoundingClientRect();
        const mv = ev => onMove((ev.clientX - rect.left) / rect.width * 300, (ev.clientY - rect.top) / rect.height * 100);
        const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); };
        addEventListener('pointermove', mv); addEventListener('pointerup', up);
      });
      curve.append(hit, s('circle', { cx, cy, r: 4.5, fill: color, stroke: '#0b0d05', 'stroke-width': 1.5, style: 'pointer-events:none' }));
    };
    mkHandle(xA, TOP, vx => { commitEnv(0, clamp((vx - X0 - 8) / 78, 0, 1)); renderEnv(); });
    mkHandle(xS, syC, (vx, vy) => { commitEnv(1, clamp((vx - xA - 8) / 74, 0, 1)); commitEnv(2, clamp((BOT - vy) / (BOT - TOP), 0, 1)); renderEnv(); });
    mkHandle(xR, BOT, vx => { commitEnv(3, clamp((vx - xS - 66 - 8) / 80, 0, 1)); renderEnv(); });
  }
  function renderEnv() {
    const dest = UI.envDest, all = UI.envAll, i = UI.sel, color = all ? ENV_ALLC : TONE_COLORS[i];
    const ov = ENV_OVR[dest];
    routeBtns.innerHTML = '';
    ['AMP','FILT','AUX'].forEach(key => { const on = dest === key;
      routeBtns.append(h('div', null, { style: 'min-width:44px;height:20px;padding:0 10px;display:flex;align-items:center;justify-content:center;border-radius:3px;cursor:pointer;font:700 9px var(--f-silk);letter-spacing:.08em;box-shadow:0 1px 2px rgba(0,0,0,.4);background:' + (on ? color : '#181b34') + ';border:1px solid ' + (on ? color : '#464e94') + ';color:' + (on ? '#07070a' : '#c9cdf2'),
        onclick: () => { UI.envDest = key; renderEnv(); } }, key)); });
    toneBtns.innerHTML = '';
    [['0','1'],['1','2'],['2','3'],['3','4'],['ALL','ALL']].forEach(pair => { const m = pair[0], lab = pair[1];
      const isAll = m === 'ALL', on = isAll ? all : (!all && i === +m);
      const btn = h('div', null, { style: 'position:relative;min-width:22px;height:20px;padding:0 8px;display:flex;align-items:center;justify-content:center;border-radius:3px;cursor:pointer;font:700 9px var(--f-silk);box-shadow:0 1px 2px rgba(0,0,0,.4);background:' + (on ? color : '#181b34') + ';border:1px solid ' + (on ? color : '#464e94') + ';color:' + (on ? '#07070a' : '#c9cdf2'),
        onclick: () => { if (isAll) { UI.envAll = true; renderEnv(); } else { UI.envAll = false; if (i !== +m) { UI.sel = +m; rebuildToneEdit(); notify(); } else renderEnv(); } } }, lab);
      if (!isAll && tone(ov, +m).get()) btn.append(h('div', null, { style: 'position:absolute;top:2px;right:2px;width:4px;height:4px;border-radius:50%;background:#ffd23f;box-shadow:0 0 4px #ffd23f' }));
      toneBtns.append(btn); });
    title.style.color = color;
    title.textContent = dest + ' \u00b7 ' + (all ? 'ALL (GLOBAL)' : 'TONE ' + TONES[i]);
    const subTxt = () => all ? ('GLOBAL \u2014 ' + ENV_ROUTE_TXT[dest]) : (T(ov).get() ? 'PER-TONE OVERRIDE' : 'FOLLOWS GLOBAL');
    sub.textContent = subTxt();
    sliders.innerHTML = '';
    ['A','D','S','R'].forEach((lab, idx) => {
      const subs = [];
      const handle = { get: () => envVal(dest, all, UI.sel, idx), sub: f => subs.push(f),
        set: v => { commitEnv(idx, v); subs.forEach(f => f(v)); paintCurve(); sub.textContent = subTxt(); } };
      sliders.append(Slider(handle, lab, { h: 78 }));
    });
    const paintAuxDest = () => auxDest.textContent = AUXDESTS[tone('auxDest', UI.sel).get()];
    paintAuxDest(); tone('auxDest', UI.sel).sub(paintAuxDest);
    auxAmtHost.innerHTML = '';
    const amt = Knob(tone('auxAmt', UI.sel), 'AUX AMT', { size: 22, color: 'var(--ptr-yellow)', bip: true, def: .5, fmt: fmtBip });
    if (amt.lastChild && amt.lastChild.classList.contains('lbl')) amt.lastChild.remove();
    auxAmtHost.append(amt, L('AMT'));
    paintCurve();
  }
  listeners.add(renderEnv);
  /* A1/TD-005: preset-load refresh wiring (bridge seam only) — renderEnv()
   * rebuilds the whole ENVELOPES section from LIVE param values, but was only
   * invoked at build and on user gesture, never on external param change (a
   * preset load mutated A/D/S/R yet the display stayed stale). Subscribe the
   * EXISTING renderEnv to every param it reads: per-tone A/D/S/R (4 tones ×
   * 3 routes), the global env tier (ENV_GLOB — REAL relays in v18), and the
   * override flags (ENV_OVR). renderEnv only .get()s these (never .set()) —
   * one-way read refresh, no feedback loop. ⚠ GUI-Claude fold upstream. */
  ['AMP', 'FILT', 'AUX'].forEach(dst => {
    ENV_GLOB[dst].forEach(gk => glob(gk).sub(renderEnv));
    ENV_ADSR[dst].forEach(pk => TONES.forEach((_, t) => tone(pk, t).sub(renderEnv)));
    TONES.forEach((_, t) => tone(ENV_OVR[dst], t).sub(renderEnv));
  });
  const block = grp('ENVELOPES', 'padding:18px 12px 8px;display:flex;flex-direction:column;gap:7px',
    h('div', 'row', { style: 'gap:6px' }, lblCell('ROUTE'), routeBtns),
    h('div', 'row', { style: 'gap:6px' }, lblCell('TONE'), toneBtns),
    h('div', 'row', { style: 'gap:12px;align-items:stretch;flex:1;min-height:0' }, lcd, sliders),
    h('div', 'row', { style: 'gap:7px;border-top:1px solid rgba(70,78,148,.4);padding-top:6px' },
      h('div', 'lbl-hi', null, 'AUX'), L('DEST'), auxDest, auxAmtHost, h('div', 'grow'), hint));
  renderEnv();
  return block;
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
  const glfo = h('div', 'col', { style: 'gap:4px;margin-bottom:auto' },
    glfoRowUI('G-LFO 1', 'glfoRate', 'glfoSync', 'glfoWave'),
    glfoRowUI('G-LFO 2', 'glfo2Rate', 'glfo2Sync', 'glfo2Wave'));
  return grp('MOD MATRIX', 'padding:16px 10px 8px;display:flex;flex-direction:column;gap:5px', glfo, ...rows);
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
        const isTime = syncKey && k === knobDefs[1][0];
        const synced = isTime && glob(syncKey).get();
        /* division law = engine's round(v*11) (PluginProcessor delay-sync read) — ⚠ GUI-Claude fold upstream. */
        const kn = Knob(glob(k), synced ? SYNCDIVS[Math.round(glob(k).get() * 11)] : l, { size: 26, color: synced ? 'var(--ptr-yellow)' : null });
        if (isTime) {
          /* C2: label snapshot — external sync change rebuilds the FX block
           * (GUI sync click already does); time motion while synced re-labels
           * with round(v*11). ⚠ GUI-Claude fold upstream. */
          glob(syncKey).sub(v => { if (!!v !== !!synced) rebuildFX(); });
          glob(k).sub(v => { if (glob(syncKey).get()) kn._setLabel(SYNCDIVS[Math.round(v * 11)]); });
        }
        return kn;
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
  const lofiFocus = h('div', 'lcd click', { style: 'width:62px;height:16px', onclick: () => { cyc(glob('lofiFocus'), LOFIFOCUS.length, 1); } });
  const talkFocus = h('div', 'lcd click', { style: 'width:62px;height:16px', onclick: () => cyc(glob('talkFocus'), TALKFOCUS.length, 1) });
  const bind = (lcd, arr, key) => { const p = () => lcd.textContent = arr[glob(key).get()]; p(); glob(key).sub(p); };
  bind(lofiFocus, LOFIFOCUS, 'lofiFocus'); bind(talkFocus, TALKFOCUS, 'talkFocus');
  // LO-FI PARAMS knob is focus-aware: BITS/SRATE/COMPAND/ALIAS each drive their own APVTS param
  const LOFI_IDS = ['lofiBits', 'lofiSrate', 'lofiCompand', 'lofiAlias'];
  let lofiParamKnob = Knob(glob(LOFI_IDS[glob('lofiFocus').get()]), 'PARAMS', { size: 30, color: 'var(--ptr-yellow)' });
  /* C3: prePost gets the standard paint+sub (was the file's one sub-less
   * widget — stale on external fx_prepost change while the modal is open).
   * ⚠ GUI-Claude fold upstream. */
  const prePost = h('div', 'btn', { style: 'width:88px;height:18px', onclick: () => { glob('prePost').set(!glob('prePost').get()); } });
  const pp = () => prePost.textContent = (glob('prePost').get() ? 'POST' : 'PRE') + ' FILTERS'; pp(); glob('prePost').sub(pp);
  const row = (...k) => h('div', 'row', { style: 'gap:10px;align-items:center' }, ...k);
  const modal = h('div', 'modal', { onclick: e => e.stopPropagation() },
    h('div', 'menu-head', { style: "font:700 10px var(--f-silk);letter-spacing:.18em;color:var(--silk-hi)" }, h('span', null, null, 'UTILITY \u2014 LO-FI \u00b7 WIDTH \u00b7 TALK'), h('span', { style: 'color:var(--silk)' }, null, 'ESC=EXIT')),
    row(Led(glob('lofiOn')), tglBtn('lofiOn', 'LO-FI', 52), lofiFocus, lofiParamKnob),
    row(Led(glob('widthOn')), tglBtn('widthOn', 'WIDTH', 52), knob('widthAmt', 'WIDTH'), knob('haas', 'HAAS'), Led(glob('bassMono')), tglBtn('bassMono', 'BASS MONO', 74)),
    row(Led(glob('talkOn')), tglBtn('talkOn', 'TALK', 52), talkFocus, knob('talkParam', 'PARAMS', 'var(--ptr-yellow)')),
    h('div', 'row', { style: 'gap:10px;border-top:1px solid var(--frame);padding-top:10px' }, Led(glob('limiter_on')), tglBtn('limiter_on', 'LIMITER', 64), h('div', 'lbl-sm', null, 'OUTPUT BRICKWALL \u00b7 DEFAULT ON')),
    h('div', 'row', { style: 'gap:10px;border-top:1px solid var(--frame);padding-top:10px' }, h('div', 'lbl', null, 'LO-FI / WIDTH PLACEMENT'), prePost));
  /* C3: re-bind the LO-FI PARAMS knob when lofi_pfocus changes (external or
   * GUI) so LCD and knob always agree; only while this modal is still in the
   * DOM. ⚠ GUI-Claude fold upstream. */
  glob('lofiFocus').sub(() => {
    if (!modal.isConnected) return;
    const fresh = Knob(glob(LOFI_IDS[glob('lofiFocus').get()]), 'PARAMS', { size: 30, color: 'var(--ptr-yellow)' });
    lofiParamKnob.replaceWith(fresh); lofiParamKnob = fresh;
  });
  showOverlay(modal);
}
function tglBtn(key, text, w) {
  const b = h('div', 'btn', { style: `width:${w}px;height:18px`, onclick: () => { glob(key).set(!glob(key).get()); touch(text, glob(key).get() ? 1 : 0); } }, text);
  const p = () => b.classList.toggle('on', !!glob(key).get()); p(); glob(key).sub(p); return b;
}
/* ---- PRESET BROWSER (factory + user bank) -------------------------------- */
/* B8: the user bank was fetched once at boot — SAVE/RENAME/DELETE reopened the
 * browser from the stale array (invisible saves, empty-bank index bug, auto-
 * name numbering off the stale count). Re-pull getUserPresetList after every
 * mutation before reopening. ⚠ GUI-Claude fold upstream. */
function refreshUserBank() {
  return Bridge.fn('getUserPresetList')()
    .then(l => { if (Array.isArray(l)) { USER_PRESETS.length = 0; l.forEach(p => USER_PRESETS.push({ name: p.name, category: p.category || 'USER', bank: 'USER' })); } })
    .catch(() => {});
}
function openPresetBrowser() {
  const factory = h('div', 'menu-list', { style: 'max-height:430px' });
  PRESETS.forEach((p, i) => {
    const row = h('div', 'menu-row' + (UI.presetSel.bank === 'FACTORY' && UI.presetSel.i === i ? ' cur' : ''), { onclick: () => { UI.presetSel = { bank: 'FACTORY', i }; UI.preset = i; UI.headerName = null; Bridge.fn('loadPreset')(i); refreshHeader(); reopenPreset(); } },
      h('span', null, { style: 'width:34px' }, 'P' + String(i + 1).padStart(3, '0')), h('span', null, { style: 'width:42px' }, p[0]), h('span', null, null, p[1]));
    factory.append(row);
  });
  const user = h('div', 'menu-list', { style: 'flex:1;min-height:64px;max-height:250px' });
  if (!USER_PRESETS.length) user.append(h('div', null, { style: 'padding:6px;font:800 9px var(--f-lcd);color:#3a6d52' }, '(NO USER PRESETS \u2014 SAVE ONE)'));
  USER_PRESETS.forEach((p, i) => {
    const seld = UI.presetSel.bank === 'USER' && UI.presetSel.i === i;
    const row = h('div', 'menu-row', { style: seld ? 'color:#07070a;background:#7dffc0' : 'color:#7dffc0', onclick: () => { UI.presetSel = { bank: 'USER', i }; UI.renameBuf = p.name; Bridge.fn('loadUserPreset')(p.name).then(() => { UI.headerName = p.name; refreshHeader(); }); reopenPreset(); } },
      h('span', null, { style: 'width:28px' }, 'U' + String(i + 1).padStart(2, '0')), h('span', null, null, p.name));
    user.append(row);
  });
  const field = h('input', 'field', { placeholder: 'RENAME SELECTED USER\u2026', value: UI.renameBuf, oninput: e => UI.renameBuf = e.target.value.toUpperCase() });
  const btn = (t, cls, fn) => h('div', 'btn', { style: 'flex:1;height:22px' + (cls || ''), onclick: fn }, t);
  const actions = h('div', 'row', { style: 'gap:5px' },
    btn('SAVE', '', () => { refreshUserBank().then(() => {
      const nm = 'USER ' + String(USER_PRESETS.length + 1).padStart(2, '0');
      return Bridge.fn('saveUserPreset')(nm).then(refreshUserBank).then(() => {
        const idx = USER_PRESETS.findIndex(p => p.name === nm);
        UI.presetSel = { bank: 'USER', i: idx >= 0 ? idx : Math.max(0, USER_PRESETS.length - 1) };
        UI.renameBuf = USER_PRESETS[UI.presetSel.i] ? USER_PRESETS[UI.presetSel.i].name : '';
        touch('SAVE PRESET', 1); reopenPreset(); }); }); }),
    btn('RENAME', '', () => { if (UI.presetSel.bank === 'USER' && USER_PRESETS[UI.presetSel.i]) { Bridge.fn('renameUserPreset')(USER_PRESETS[UI.presetSel.i].name, UI.renameBuf).then(refreshUserBank).then(() => {
      const idx = USER_PRESETS.findIndex(p => p.name === UI.renameBuf);
      if (idx >= 0) UI.presetSel = { bank: 'USER', i: idx };
      reopenPreset(); }); } }),
    h('div', 'btn', { style: 'flex:1;height:22px;border-color:#7a2130;color:#ff8a97', onclick: () => { if (UI.presetSel.bank === 'USER' && USER_PRESETS[UI.presetSel.i]) { Bridge.fn('deleteUserPreset')(USER_PRESETS[UI.presetSel.i].name).then(refreshUserBank).then(() => { UI.presetSel = { bank: 'FACTORY', i: 0 }; reopenPreset(); }); } } }, 'DELETE'));
  const load = h('div', 'btn', { style: 'height:24px;background:#1e2547;color:var(--lcd-ink);font-size:9px;letter-spacing:.12em', onclick: () => { if (UI.presetSel.bank === 'USER') { const p = USER_PRESETS[UI.presetSel.i]; if (p) Bridge.fn('loadUserPreset')(p.name).then(() => { UI.headerName = p.name; refreshHeader(); }); } else { UI.preset = UI.presetSel.i; UI.headerName = null; Bridge.fn('loadPreset')(UI.presetSel.i); refreshHeader(); } closeOverlay(); } }, 'LOAD SELECTED');
  const box = h('div', null, { style: 'width:600px;max-height:540px;background:var(--lcd-bg);border:1px solid var(--frame);border-radius:4px;box-shadow:0 8px 40px #000;padding:12px;display:flex;flex-direction:column;gap:10px', onclick: e => e.stopPropagation() },
    h('div', 'menu-head', null, h('span', null, null, 'PRESET BROWSER'), h('span', null, null, 'ESC=EXIT')),
    h('div', 'row', { style: 'gap:12px;min-height:0' },
      h('div', 'grow col', { style: 'gap:4px;min-height:0' }, h('div', 'lbl-hi', { style: 'color:var(--silk-hi)' }, 'FACTORY \u00b7 ' + PRESETS.length), factory),   /* B9: live count, not a literal \u2014 \u26a0 GUI-Claude fold upstream */
      h('div', null, { style: 'width:1px;background:var(--frame);opacity:.5' }),
      h('div', 'col', { style: 'width:232px;gap:5px;min-height:0' }, h('div', 'lbl-hi', { style: 'color:#7dffc0' }, 'USER BANK'), user, field, actions, load,
        h('div', null, { style: 'font:600 7px var(--f-silk);color:var(--silk-dim)' }, 'FACTORY READ-ONLY \u00b7 SAVE = USER COPY'))));
  showOverlay(box, 'z11');
}
function reopenPreset() { openPresetBrowser(); }

/* ============================================================ ANIMATION ==== */
let mL = 0, mR = 0;

/* B3: real output meters — the editor pushes window.uiMeters({l,r}) at 30 Hz.
 * Receiver stores the latest peaks; tick() applies its existing peak-hold+decay.
 * Browser (no __JUCE__) keeps the simulated feed. ⚠ GUI-Claude fold upstream. */
let realMeters = null;
window.uiMeters = m => { if (m && typeof m.l === 'number' && typeof m.r === 'number') realMeters = m; };

/* B9/TD-010: host program push — editor calls window.uiProgram({index,name,user})
 * on program change AND name changes (index 0 = INIT, 1..N = factory; user:true =
 * user-preset name verbatim + stepper sync). Defensive: no error if fields absent.
 * ⚠ GUI-Claude fold upstream. */
window.uiProgram = p => {
  if (!p || typeof p.index !== 'number') return;
  if (p.user) {
    const nm = p.name != null ? String(p.name) : null;
    UI.headerName = nm;
    const ui = nm != null ? USER_PRESETS.findIndex(x => x.name === nm) : -1;
    if (ui >= 0) UI.presetSel = { bank: 'USER', i: ui };
  } else if (p.index > 0 && p.index <= PRESETS.length) {
    UI.preset = p.index - 1; UI.presetSel = { bank: 'FACTORY', i: UI.preset }; UI.headerName = null;
  } else {
    UI.headerName = p.index === 0 ? 'INIT' : (p.name != null ? String(p.name) : null);
  }
  refreshHeader?.();
};

/* B4: LIM LED from the real limiter GR (native poll at tick cadence, one call
 * in flight); browser QA keeps the mock heuristic. ⚠ GUI-Claude fold upstream. */
let limGR = 0, limPending = false;
function pollLimiter() {
  if (limPending) return; limPending = true;
  Bridge.fn('getLimiterGR')().then(g => { limGR = +g || 0; limPending = false; })
    .catch(() => { limPending = false; });
}

function tick() {
  if (HEADER_ANIM) {
    if (Bridge.live) {
      /* B3/B4: live meters from the uiMeters push + LIM from getLimiterGR — ⚠ GUI-Claude fold upstream. */
      const src = realMeters || { l: 0, r: 0 };
      mL = Math.max(mL * .82, src.l);
      mR = Math.max(mR * .82, src.r);
      pollLimiter();
      HEADER_ANIM.limLed.classList.toggle('on', glob('limiter_on').get() && limGR > 0);
    } else {
      const act = TONES.reduce((s2, n, i) => s2 + (tone('on', i).get() ? tone('level', i).get() : 0), 0) / 4;
      const m = glob('master').get();
      mL = Math.max(mL * .82, m * (.45 + act * .8 + Math.random() * .12));
      mR = Math.max(mR * .82, m * (.45 + act * .8 + Math.random() * .12));
      HEADER_ANIM.limLed.classList.toggle('on', glob('limiter_on').get() && (mL > .9 || mR > .9));
    }
    HEADER_ANIM.mL.style.height = Math.round(Math.min(1, mL) * 100) + '%';
    HEADER_ANIM.mR.style.height = Math.round(Math.min(1, mR) * 100) + '%';
  }
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
  /* 1d/TD-003: base height = CURRENT layout height (.panel 660 folded / 850
   * kbd-open), not a hardcoded 660 — the whole unit incl. the opened keybed
   * scales into the window in both fold states (USER-STATED; overrides the
   * handoff's fixed-660 0.8-scale fit, which was the TD-003 dead-frame bug).
   * ⚠ GUI-Claude fold upstream. */
  const baseH = panel.offsetHeight || 660;
  UI.scale = Math.min(window.innerWidth / 1140, window.innerHeight / baseH);
  /* CSS flex centers the UNscaled 1140×660 box, so the scaled panel sat inside a
   * dead frame (TD-003). Position absolutely from the top-left instead and center
   * the SCALED box ourselves (rubber-rhino fit() rule). */
  panel.style.position = 'absolute';
  panel.style.transformOrigin = '0 0';
  panel.style.transform = `scale(${UI.scale})`;
  panel.style.left = Math.max(0, (window.innerWidth - 1140 * UI.scale) / 2) + 'px';
  panel.style.top = Math.max(0, (window.innerHeight - panel.offsetHeight * UI.scale) / 2) + 'px';
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
    /* 1b: wheels drive the engine (pitchBend -1..+1 / modWheel 0..1 natives);
     * pitch springs back to center on release, as before, and reports it.
     * ⚠ GUI-Claude fold upstream. */
    w.addEventListener('pointerdown', e => { e.preventDefault(); const sy = e.clientY, s0 = val;
      const mv = ev => { val = clamp(s0 + (sy - ev.clientY) / 118, isPitch ? -1 : 0, 1); touch(isPitch ? 'PITCH BEND' : 'MOD WHEEL', isPitch ? (val + 1) / 2 : val); paint(); Bridge.fn(isPitch ? 'pitchBend' : 'modWheel')(val); };
      const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); if (isPitch) { val = 0; paint(); Bridge.fn('pitchBend')(0); } };
      addEventListener('pointermove', mv); addEventListener('pointerup', up); });
    bed.append(w);
  };
  wheel(24, 28, true); wheel(76, 80, false);
  bed.append(h('div', null, { style: 'position:absolute;left:22px;top:12px;font:600 7px var(--f-silk);color:#9aa1d8;letter-spacing:.14em' }, 'PITCH'));
  bed.append(h('div', null, { style: 'position:absolute;left:81px;top:12px;font:600 7px var(--f-silk);color:#9aa1d8;letter-spacing:.14em' }, 'MOD'));
  const keys = h('div', null, { style: 'position:absolute;left:144px;top:0;right:0;bottom:0' });
  /* 1a: keybed → real MIDI via the noteOn/noteOff natives. White key k of the
   * 30-key bed maps C-major from C3 (MIDI 48): degree [0,2,4,5,7,9,11][k%7];
   * blacks (per the existing layout: after whites 0,1,3,4,5 of each octave)
   * are white+1. Release AND pointer-leave-while-held send noteOff (no stuck
   * notes); the flash + touch() behavior is unchanged. ⚠ GUI-Claude fold upstream. */
  const KEY_DEG = [0, 2, 4, 5, 7, 9, 11], KEY_BASE = 48;
  const whiteMidi = k => KEY_BASE + Math.floor(k / 7) * 12 + KEY_DEG[k % 7];
  const press = (el, note) => e => {
    e.preventDefault();
    const orig = el.style.background; el.style.background = 'linear-gradient(180deg,#ffe9a0,#ffd23f)';
    touch('NOTE ON', .5); setTimeout(() => el.style.background = orig, 200);
    Bridge.fn('noteOn')(note, 0.79);
    let done = false;
    const off = () => { if (done) return; done = true; Bridge.fn('noteOff')(note);
      el.removeEventListener('pointerup', off); el.removeEventListener('pointerleave', off); removeEventListener('pointerup', off); };
    el.addEventListener('pointerup', off); el.addEventListener('pointerleave', off); addEventListener('pointerup', off);
  };
  for (let k = 0; k < 30; k++) { const wk = h('div', null, { style: `position:absolute;top:0;width:32.662px;height:170px;border-radius:0 0 3px 3px;box-shadow:1px 0 1px rgba(0,0,0,.35),inset 0 -3px 3px rgba(0,0,0,.25);cursor:pointer;left:${(k * 32.662).toFixed(2)}px;background:linear-gradient(180deg,#f0f1fb 0%,#c7cae0 100%)` }); wk.addEventListener('pointerdown', press(wk, whiteMidi(k))); keys.append(wk); }
  Array.from({ length: 30 }, (_, k) => k).filter(k => [0, 1, 3, 4, 5].includes(k % 7) && k < 29).slice(0, 21).forEach(k => { const bk = h('div', null, { style: `position:absolute;top:0;width:20px;height:104px;border-radius:0 0 3px 3px;box-shadow:0 3px 5px rgba(0,0,0,.5);cursor:pointer;z-index:2;left:${(k * 32.662 + 22.5).toFixed(2)}px;background:linear-gradient(180deg,#26283e 0%,#0c0d18 100%)` }); bk.addEventListener('pointerdown', press(bk, whiteMidi(k) + 1)); keys.append(bk); });
  bed.append(keys);
  bed.style.display = UI.kbdOpen ? 'block' : 'none';
  /* 1c: fold also drives the NATIVE editor height (keyboardFold(folded):
   * kBaseH open / kFoldedH folded), then re-fits on the next rAF so
   * fitToWindow measures the post-fold host window. ⚠ GUI-Claude fold upstream. */
  tabBtn.addEventListener('click', () => { UI.kbdOpen = !UI.kbdOpen; const panel = document.querySelector('.panel'); if (panel) panel.classList.toggle('kbd-open', UI.kbdOpen); bed.style.display = UI.kbdOpen ? 'block' : 'none'; tabBtn.textContent = tabTxt(); Bridge.fn('keyboardFold')(!UI.kbdOpen); fitToWindow(); requestAnimationFrame(fitToWindow); });
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
  row2.append(buildEnvelopes(), buildMatrix(), fxHost);
  panel.append(row2);
  panel.append(buildKeyboard());
  panel.append(h('div', 'grip', { onpointerdown: gripResize, ondblclick: () => { UI.scale = 1; panel.style.transform = 'scale(1)'; } }));
  /* B5: version silk from the binary (getInfo), literal fallback for browser QA.
   * User-sanctioned text wiring. ⚠ GUI-Claude fold upstream. */
  const verSilk = h('div', null, { style: "position:absolute;right:34px;top:626px;font:600 7.5px var(--f-silk);color:var(--silk-dim);letter-spacing:.16em;z-index:6" }, 'VER 1.0');
  Bridge.fn('getInfo')().then(info => {
    if (info && info.version) verSilk.textContent = 'VER ' + info.version;
    /* TD-010: defensive boot header init — preset-name field may be absent; routed through the uiProgram receiver. ⚠ GUI-Claude fold upstream. */
    if (info && info.presetName) window.uiProgram({ index: typeof info.presetIndex === 'number' ? info.presetIndex : -1, name: info.presetName, user: !!info.presetUser });
  }).catch(() => {});
  panel.append(verSilk);
  document.getElementById('root').append(panel);
  hydrate(); startScope(); fitToWindow();
  requestAnimationFrame(fitToWindow);   // re-fit after the WebView2 viewport settles (TD-003) — ⚠ GUI-Claude fold upstream
  requestAnimationFrame(tick);
}
function gripResize(e) {
  e.preventDefault(); const sy = e.clientY, s0 = UI.scale, panel = document.querySelector('.panel');
  const mv = ev => { UI.scale = clamp(s0 + (ev.clientY - sy) / 660, .5, 2); panel.style.transform = `scale(${UI.scale})`; };
  const up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); };
  addEventListener('pointermove', mv); addEventListener('pointerup', up);
}

if (document.readyState !== 'loading') build(); else addEventListener('DOMContentLoaded', build);
