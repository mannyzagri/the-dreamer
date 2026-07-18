// The Dreamer -- GUI v7 production logic (JUCE 8 WebView2 editor).
// v6 face + v7 renovation (design-handoff/v7/README.md deltas), rebound
// to the canonical DSP_BUILD.md section-9 parameter IDs (per-tone params
// use SUFFIXES _a.._d; see plugin/Params.h -- IDs are LOCKED).
// House pattern: rubber-rhino gui.
//
// v7 deltas: brushed-metal faceplate finish (seeded, default ON; star-field
// stays an off-by-default variant), preset steppers + preset browser (24
// factory presets, UI-side index/name; apply writes every param through the
// bound states), TONES mixer strip (PAN mini-knobs, 130px faders), TONE EDIT
// reflow (RND divider row, FILTER/AMPLITUDE/AUX banks, LFO1/LFO2 rows with
// tempo-sync divisions, AUX ENV AMT+DEST row), FILTERS BAL 1<->2 knob,
// DREAM VECTOR widened with per-tone DIR/INT knobs.
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
// list is bank3 VERBATIM (104, bank3 order: 78 cycles + 16 [ENS] loops +
// 10 [SHOT] one-shots); display uppercases at render time, tag shows
// right-aligned in the overlay. (The v7 README's "0-77" line is stale --
// the 104-wave bank3 freeze in Params.h is authoritative.)
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
const SYNCDIVS  = ["4/1", "2/1", "1/1", "1/2", "1/2T", "1/4",   // lfoN_sync lit:
                   "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"];  // idx=round(v*11)
const MSRC      = ["G-LFO", "VEC PHS", "AUX", "VELO", "WHEEL"];
const MDST      = ["PITCH", "CUT 1", "CUT 2", "MORPH", "SHAPE", "VEC PHS", "PAN", "NOISE"];
const MODFX     = ["CHORUS", "FLANGER", "PHASER", "ENSEMBLE"];
const DLYMODES  = ["DIGITAL", "TAPE", "PONG"];
const REVTYPES  = ["ROOM", "HALL", "PLATE"];
const TONES     = ["A", "B", "C", "D"];
const SFX       = ["_a", "_b", "_c", "_d"];   // per-tone id SUFFIX (Params.h tid)

//==============================================================================
// Factory presets (24, prototype PRESETS order/grouping). The prototype
// array carries category + name only; the parameter payloads below are
// production-authored full patches: every preset = complete base patch +
// overrides, applied through the bound states (setNormalisedValue /
// setValue / setChoiceIndex) so the host learns of every change in plugin
// mode. Preset index/name is UI-side state only (no APVTS param).
// Wave numbers are bank3 indices (the WAVES array above).
const TONE_SLIDER_BASE = {   // per-tone base (suffixed _a.._d on apply)
  oct: .5, fine: .5, start: 0, level: .7, velo: .4, pan: .5, shape_depth: 0,
  noise: 0, noise_color: .5, vint: .6, aux_amt: .5,
  tvf_cut: .6, tvf_res: .25, tvf_env: .5, tvf_kf: .3,
  tvf_a: .05, tvf_d: .6, tvf_s: .5, tvf_r: .55,
  tva_a: .2, tva_d: .7, tva_s: .8, tva_r: .65,
  aux_a: 0, aux_d: .4, aux_s: 0, aux_r: .3,
  lfo1_rate: .4, lfo1_depth: .15, lfo2_rate: .25, lfo2_depth: 0,
};
const TONE_TOGGLE_BASE = { on: false, start_random: false, lfo1_sync: false, lfo2_sync: false };
const TONE_COMBO_BASE  = { wave: 0, shape: 0, tvf_type: 0, aux_dest: 0, lfo1_dest: 0, lfo2_dest: 1 };
const GLOB_SLIDER_BASE = {
  master: .78, vec_phase: 0, vec_orbit_rate: .3,
  vec_penv_start: 0, vec_penv_end: .5, vec_penv_time: .4, flt_bal: .5,
  flt1_cut: 1, flt1_res: 0, flt1_env: 0, flt2_cut: 1, flt2_res: 0, flt2_morph: 0,
  lfo_rate: .4, mtx1_amt: .5, mtx2_amt: .5, mtx3_amt: .5,
  modfx_rate: .3, modfx_depth: .5, modfx_mix: .45,
  dly_time: .5, dly_fb: .35, dly_mix: .3, rev_size: .6, rev_damp: .4, rev_mix: .35,
};
const GLOB_TOGGLE_BASE = { vec_orbit_on: false, vec_penv_on: false, vec_penv_loop: false,
  modfx_on: false, dly_on: false, rev_on: false };
const GLOB_COMBO_BASE  = { vec_orbit_shape: 2, lfo_shape: 0, flt_route: 0,
  flt1_type: 0, flt2_type: 0, mtx1_src: 0, mtx1_dst: 3, mtx2_src: 1, mtx2_dst: 4,
  mtx3_src: 2, mtx3_dst: 0, modfx_type: 0, dly_mode: 0, rev_type: 1 };

// Per preset: t = 4 tone entries {w wave, lv level, off?, s/b/c overrides
// keyed by base ids}; s/b/c = global slider/toggle/combo overrides.
const PRESETS = [
  { cat: "PAD", name: "ETHEREAL DAWN",   // = boot demo state / handoff PNG
    t: [{ w: 11, lv: .8,  s: { fine: .52, start: .1,  tvf_cut: .5 } },
        { w: 28, lv: .62, s: { fine: .48, start: .2,  tvf_cut: .6 } },
        { w: 33, lv: .5,  s: { start: .05, tvf_cut: .45 } },
        { w: 40, lv: .4,  s: { fine: .54, start: .15, tvf_cut: .55 } }],
    s: { vec_phase: .12, flt1_cut: .52, flt1_res: .35, flt1_env: .5,
         flt2_cut: .7, flt2_res: .2, flt2_morph: .4, mtx1_amt: .8, mtx2_amt: .35 },
    b: { vec_orbit_on: true, modfx_on: true, dly_on: true, rev_on: true },
    c: { flt1_type: 6, flt2_type: 13 } },
  { cat: "PAD", name: "SLOW MEMORY",
    t: [{ w: 9,  lv: .75, s: { tva_a: .5,  tva_r: .85 } },
        { w: 82, lv: .6,  s: { tva_a: .45, tva_r: .85 } },
        { w: 15, lv: .55, s: { tva_a: .55, tva_r: .9 } },
        { w: 20, lv: .45, s: { tva_a: .5,  tva_r: .85 } }],
    s: { flt1_cut: .45, flt2_morph: .5, rev_mix: .5, rev_size: .75, dly_mix: .22 },
    b: { vec_orbit_on: true, rev_on: true, dly_on: true },
    c: { flt2_type: 13 } },
  { cat: "PAD", name: "GHOST HARBOR",
    t: [{ w: 85, lv: .7,  s: { tva_a: .6,  tva_r: .9 } },
        { w: 30, lv: .55, s: { tva_a: .5,  tva_r: .85 } },
        { w: 12, lv: .5,  s: { tva_a: .55 } },
        { w: 87, lv: .45, s: { tva_a: .6,  tva_r: .9 } }],
    s: { flt1_cut: .4, flt1_res: .3, rev_mix: .55, rev_size: .8, modfx_mix: .5 },
    b: { vec_orbit_on: true, rev_on: true, modfx_on: true },
    c: { flt1_type: 4 } },
  { cat: "PAD", name: "VIOLET SLEEP",
    t: [{ w: 17, lv: .72, s: { tva_a: .45, tva_r: .8 } },
        { w: 21, lv: .6,  s: { tva_a: .5 } },
        { w: 5,  lv: .5,  s: { tva_a: .4 } },
        { w: 79, lv: .42, s: { tva_a: .55, tva_r: .85 } }],
    s: { flt1_cut: .5, flt2_morph: .35, rev_mix: .45, rev_size: .7 },
    b: { vec_orbit_on: true, rev_on: true },
    c: { flt2_type: 13 } },
  { cat: "PAD", name: "TIDE GARDEN",
    t: [{ w: 87, lv: .7,  s: { tva_a: .55, tva_r: .85 } },
        { w: 14, lv: .55, s: { tva_a: .5 } },
        { w: 22, lv: .5,  s: { tva_a: .45 } },
        { w: 3,  lv: .4,  s: { oct: .25, tva_a: .5 } }],
    s: { flt1_cut: .55, rev_mix: .5, rev_size: .75, dly_mix: .25, vec_orbit_rate: .2 },
    b: { vec_orbit_on: true, rev_on: true, dly_on: true },
    c: {} },
  { cat: "PAD", name: "HALF LIGHT",
    t: [{ w: 6,  lv: .7,  s: { tva_a: .4,  tva_r: .75 } },
        { w: 10, lv: .6,  s: { tva_a: .45 } },
        { w: 35, lv: .45, s: { tva_a: .5 } },
        { w: 44, lv: .4,  s: { tva_a: .3 } }],
    s: { flt1_cut: .48, flt1_res: .2, rev_mix: .4, modfx_mix: .4 },
    b: { vec_orbit_on: true, rev_on: true, modfx_on: true },
    c: { flt1_type: 5 } },
  { cat: "SPLIT", name: "GLASS RIVER",
    t: [{ w: 46, lv: .7,  s: { oct: .75, tva_a: .05, tva_d: .6, tva_s: .4, tva_r: .5 } },
        { w: 17, lv: .6,  s: { tva_a: .4 } },
        { w: 27, lv: .6,  s: { oct: .25, tva_a: .3, tva_r: .7 } },
        { w: 3,  lv: .45, s: { oct: .25 } }],
    s: { flt2_morph: .3, rev_mix: .4, dly_mix: .3 },
    b: { rev_on: true, dly_on: true },
    c: { flt_route: 1, flt2_type: 13, dly_mode: 1 } },
  { cat: "SPLIT", name: "NIGHT DRIVE 84",
    t: [{ w: 0,  lv: .7,  s: { oct: .25, tva_a: .02, tva_r: .3, tvf_env: .6, vint: .2 } },
        { w: 1,  lv: .55, s: { vint: .3 } },
        { w: 82, lv: .55, s: { tva_a: .4, tva_r: .7 } },
        { w: 51, lv: .4,  s: { oct: .75, tva_a: .05, tva_s: .3 } }],
    s: { flt1_cut: .5, flt1_res: .35, flt1_env: .45, dly_mix: .35, dly_fb: .45 },
    b: { dly_on: true, rev_on: true },
    c: { flt1_type: 6, dly_mode: 2 } },
  { cat: "BELL", name: "TINE CATHEDRAL",
    t: [{ w: 44, lv: .75, s: { tva_a: .02, tva_d: .75, tva_s: .35, tva_r: .75 } },
        { w: 45, lv: .6,  s: { tva_a: .02, tva_d: .7,  tva_s: .3,  tva_r: .7 } },
        { w: 79, lv: .5,  s: { tva_a: .5,  tva_r: .9 } },
        { w: 54, lv: .45, s: { tva_a: .03, tva_d: .7,  tva_s: .3,  tva_r: .7 } }],
    s: { rev_mix: .55, rev_size: .85, flt1_cut: .7 },
    b: { rev_on: true },
    c: {} },
  { cat: "BELL", name: "MUSICBOX MOON",
    t: [{ w: 49, lv: .75, s: { tva_a: 0, tva_d: .6,  tva_s: .2, tva_r: .6 } },
        { w: 51, lv: .55, s: { tva_a: 0, tva_d: .55, tva_s: .2, tva_r: .55 } },
        { w: 99, lv: .5,  s: { tva_a: 0, tva_d: .5,  tva_s: 0,  tva_r: .4 } },
        { w: 3,  lv: .35, s: { oct: .75, tva_a: 0, tva_s: .2 } }],
    s: { rev_mix: .45, rev_size: .6, dly_mix: .3, dly_time: .6 },
    b: { rev_on: true, dly_on: true },
    c: { rev_type: 2 } },
  { cat: "BELL", name: "COLD CHIMES",
    t: [{ w: 52, lv: .7,  s: { tva_a: 0, tva_d: .7,  tva_s: .25, tva_r: .7 } },
        { w: 53, lv: .55, s: { tva_a: 0, tva_d: .65, tva_s: .25, tva_r: .65 } },
        { w: 58, lv: .4,  s: { tva_a: 0, tva_d: .6,  tva_s: .2 } },
        { w: 92, lv: .45, s: { tva_a: .4, tva_r: .8 } }],
    s: { flt1_cut: .75, rev_mix: .5, rev_size: .7, flt2_morph: .45 },
    b: { rev_on: true },
    c: { flt2_type: 11 } },
  { cat: "STR", name: "ORBITAL STRINGS",
    t: [{ w: 22, lv: .75, s: { tva_a: .35, tva_r: .7 } },
        { w: 23, lv: .6,  s: { tva_a: .4,  tva_r: .7 } },
        { w: 26, lv: .55, s: { tva_a: .35, tva_r: .65 } },
        { w: 78, lv: .5,  s: { tva_a: .45, tva_r: .8 } }],
    s: { vec_orbit_rate: .45, flt1_cut: .6, rev_mix: .4, modfx_mix: .4 },
    b: { vec_orbit_on: true, rev_on: true, modfx_on: true },
    c: {} },
  { cat: "STR", name: "SOLINA FIELDS",
    t: [{ w: 82, lv: .8,  s: { tva_a: .4, tva_r: .8 } },
        { w: 24, lv: .55, s: { tva_a: .4 } },
        { w: 25, lv: .5,  s: { tva_a: .35 } },
        { w: 16, lv: .4,  s: { tva_a: .5 } }],
    s: { modfx_mix: .6, modfx_depth: .6, rev_mix: .45, flt1_cut: .55 },
    b: { modfx_on: true, rev_on: true },
    c: { modfx_type: 3 } },
  { cat: "VOX", name: "CHOIR OF WIRES",
    t: [{ w: 80, lv: .75, s: { tva_a: .45, tva_r: .8 } },
        { w: 31, lv: .6,  s: { tva_a: .4 } },
        { w: 34, lv: .5,  s: { tva_a: .45 } },
        { w: 91, lv: .45, s: { tva_a: .5, tva_r: .85 } }],
    s: { flt1_cut: .5, rev_mix: .5, rev_size: .75, flt2_morph: .5 },
    b: { vec_orbit_on: true, rev_on: true },
    c: { flt2_type: 11 } },
  { cat: "VOX", name: "BREATH MACHINE",
    t: [{ w: 14, lv: .7,  s: { noise: .25, noise_color: .6, tva_a: .5, tva_r: .85 } },
        { w: 85, lv: .6,  s: { tva_a: .55, tva_r: .9 } },
        { w: 32, lv: .45, s: { tva_a: .45 } },
        { w: 15, lv: .4,  s: { noise: .15, tva_a: .5 } }],
    s: { flt1_cut: .45, rev_mix: .5, dly_mix: .2 },
    b: { rev_on: true, dly_on: true },
    c: { flt1_type: 4 } },
  { cat: "BASS", name: "RUBBER ORBIT",
    t: [{ w: 0,   lv: .8, s: { oct: .25, tva_a: .02, tva_d: .45, tva_s: .6, tva_r: .2,
                               tvf_env: .6, tvf_cut: .4, vint: .3 } },
        { w: 3,   lv: .6, s: { oct: .25, tva_a: .02, tva_r: .2, vint: .2 } },
        { w: 71,  lv: .35, s: { tva_a: .02, tva_s: .4, tva_r: .25 } },
        { w: 102, lv: .5,  s: { tva_a: 0, tva_s: 0, tva_d: .35, tva_r: .3 } }],
    s: { flt1_cut: .45, flt1_res: .3, flt1_env: .55, vec_orbit_rate: .5 },
    b: { vec_orbit_on: true },
    c: { flt1_type: 6 } },
  { cat: "BASS", name: "SUB DREAMS",
    t: [{ w: 3,  lv: .85, s: { oct: .25, vint: 0, tva_a: .03, tva_r: .35 } },
        { w: 2,  lv: .5,  s: { oct: .25, vint: 0, tva_a: .05 } },
        { w: 1,  lv: .3,  s: { vint: .2, tva_a: .05 } },
        { w: 96, lv: .4,  s: { tva_a: 0, tva_s: 0, tva_d: .3, tva_r: .25 } }],
    s: { flt1_cut: .35, rev_mix: .15 },
    b: { rev_on: true },
    c: {} },
  { cat: "LEAD", name: "SYNC COMET",
    t: [{ w: 0,  lv: .8,  s: { tva_a: .02, tva_r: .3, tvf_env: .55, tvf_cut: .5, vint: .15 } },
        { w: 1,  lv: .55, s: { fine: .56, vint: .15, tva_a: .02 } },
        { w: 63, lv: .4,  s: { tva_a: .05, tva_s: .5 } },
        { w: 94, lv: .5,  s: { tva_a: 0, tva_s: 0, tva_d: .3 } }],
    s: { flt1_cut: .6, flt1_res: .4, dly_mix: .3, dly_fb: .4 },
    b: { dly_on: true },
    c: { flt1_type: 5 } },
  { cat: "LEAD", name: "12-BIT STAR",
    t: [{ w: 1,  lv: .75, s: { tva_a: .02, tva_r: .35, vint: .2 } },
        { w: 74, lv: .5,  s: { tva_a: .02, tva_s: .5 } },
        { w: 43, lv: .5,  s: { tva_a: .02, tva_d: .6, tva_s: .3 } },
        { w: 3,  lv: .4,  s: { oct: .75, vint: .3 } }],
    s: { flt1_cut: .65, flt1_res: .25, dly_mix: .3, rev_mix: .3 },
    b: { dly_on: true, rev_on: true },
    c: {} },
  { cat: "KEY", name: "EPIANO HAZE",
    t: [{ w: 44, lv: .8,  s: { tva_a: .02, tva_d: .65, tva_s: .55, tva_r: .4 } },
        { w: 45, lv: .6,  s: { tva_a: .02, tva_d: .6,  tva_s: .5,  tva_r: .4 } },
        { w: 43, lv: .4,  s: { tva_a: .02, tva_d: .55, tva_s: .4 } },
        { w: 95, lv: .3,  s: { tva_a: 0, tva_s: 0, tva_d: .35 } }],
    s: { modfx_mix: .4, rev_mix: .35, flt1_cut: .7 },
    b: { modfx_on: true, rev_on: true },
    c: {} },
  { cat: "KEY", name: "ORGAN NEBULA",
    t: [{ w: 90, lv: .8,  s: { tva_a: .05, tva_s: 1, tva_r: .3 } },
        { w: 18, lv: .55, s: { tva_a: .05, tva_s: 1 } },
        { w: 19, lv: .5,  s: { tva_a: .05, tva_s: 1 } },
        { w: 3,  lv: .4,  s: { oct: .25, tva_a: .05, tva_s: 1 } }],
    s: { modfx_mix: .5, modfx_rate: .45, rev_mix: .35 },
    b: { modfx_on: true, rev_on: true },
    c: { modfx_type: 1 } },
  { cat: "SFX", name: "RE-ENTRY",
    t: [{ w: 98,  lv: .75, s: { tva_a: .6, tva_r: .9, aux_amt: .85, aux_d: .8 } },
        { w: 103, lv: .55, s: { tva_a: .3 } },
        { w: 84,  lv: .5,  s: { tva_a: .55, tva_r: .9 } },
        { w: 87,  lv: .45, s: { tva_a: .6 } }],
    s: { flt1_cut: .4, flt1_env: .6, rev_mix: .6, rev_size: .85, dly_mix: .35 },
    b: { rev_on: true, dly_on: true, vec_orbit_on: true },
    c: {} },
  { cat: "SFX", name: "STATIC BLOOM",
    t: [{ w: 93, lv: .7,  s: { tva_a: .5, tva_r: .85, noise: .4, noise_color: .7,
                               lfo1_rate: .45, lfo1_depth: .4 }, b: { lfo1_sync: true } },
        { w: 72, lv: .5,  s: { tva_a: .4, noise: .3 } },
        { w: 98, lv: .45, s: { tva_a: .55 } },
        { w: 89, lv: .4,  s: { tva_a: .5 } }],
    s: { flt1_res: .45, flt1_cut: .5, modfx_mix: .55, rev_mix: .5 },
    b: { modfx_on: true, rev_on: true },
    c: { flt1_type: 7, modfx_type: 2 } },
  { cat: "INIT", name: "INIT PATCH",
    t: [{ w: 0, lv: .8, s: { tva_a: .05, tva_d: .35, tva_s: 1, tva_r: .35,
                             tvf_a: .05, tvf_d: .35, tvf_s: 1, tvf_r: .35,
                             tvf_cut: .8, tvf_res: 0, tvf_env: 0,
                             vint: 0, lfo1_depth: 0, velo: .5 } },
        { w: 0, off: 1 }, { w: 0, off: 1 }, { w: 0, off: 1 }],
    s: {}, b: {}, c: { vec_orbit_shape: 0 } },
];

//==============================================================================
// Standalone-mock demo defaults (normalised sliders / bools / choice indices)
// -- cosmetics only, mirrors the prototype's boot state / handoff PNG
// (= factory preset P001 ETHEREAL DAWN). The real plugin never reads these
// (relay initial-update wins).
const MOCK = { sliders: {}, toggles: {}, combos: {} };
{
  const lv = [.8, .62, .5, .4], st = [.1, .2, .05, .15];
  const ct = [.5, .6, .45, .55], fn = [.52, .48, .5, .54];
  const wv = [11, 28, 33, 40];
  SFX.forEach((sx, i) => {
    Object.assign(MOCK.sliders, {
      ["oct" + sx]: .5, ["fine" + sx]: fn[i], ["level" + sx]: lv[i],
      ["velo" + sx]: .4, ["start" + sx]: st[i], ["pan" + sx]: .5,
      ["dir" + sx]: i * .25, ["vint" + sx]: .6, ["shape_depth" + sx]: 0,
      ["noise" + sx]: 0, ["noise_color" + sx]: .5,
      ["tvf_cut" + sx]: ct[i], ["tvf_res" + sx]: .25, ["tvf_env" + sx]: .5,
      ["tvf_kf" + sx]: .3,
      ["tvf_a" + sx]: .05, ["tvf_d" + sx]: .6, ["tvf_s" + sx]: .5, ["tvf_r" + sx]: .55,
      ["tva_a" + sx]: .2,  ["tva_d" + sx]: .7, ["tva_s" + sx]: .8, ["tva_r" + sx]: .65,
      ["aux_a" + sx]: 0,   ["aux_d" + sx]: .4, ["aux_s" + sx]: 0,  ["aux_r" + sx]: .3,
      ["aux_amt" + sx]: .5,
      ["lfo1_rate" + sx]: .4, ["lfo1_depth" + sx]: .15,
      ["lfo2_rate" + sx]: .25, ["lfo2_depth" + sx]: 0,
    });
    MOCK.toggles["on" + sx] = true;
    MOCK.toggles["start_random" + sx] = false;
    MOCK.toggles["lfo1_sync" + sx] = false;
    MOCK.toggles["lfo2_sync" + sx] = false;
    Object.assign(MOCK.combos, {
      ["wave" + sx]: wv[i], ["shape" + sx]: 0, ["tvf_type" + sx]: 0,
      ["aux_dest" + sx]: 0, ["lfo1_dest" + sx]: 0, ["lfo2_dest" + sx]: 1,
    });
  });
  Object.assign(MOCK.sliders, {
    vec_phase: .12, vec_orbit_rate: .3,
    vec_penv_start: 0, vec_penv_end: .5, vec_penv_time: .4,
    lfo_rate: .4, mtx1_amt: .8, mtx2_amt: .35, mtx3_amt: .5,
    flt1_cut: .52, flt1_res: .35, flt1_env: .5,
    flt2_cut: .7, flt2_res: .2, flt2_morph: .4, flt_bal: .5,
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
    lfo_shape: 0, vec_orbit_shape: 2,
    mtx1_src: 0, mtx2_src: 1, mtx3_src: 2,
    mtx1_dst: 3, mtx2_dst: 4, mtx3_dst: 0,
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
// UI-only state: tone selection + preset index + redraw hooks run when the
// selection changes.
const SEL = { i: 0 };
const PRESET = { i: 0 };
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
// dragging (octave detents, bipolar center snap).
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
// v7 bipolar center detent (pan / AUX AMT / BAL / matrix AMT): +-.035 snap.
const detent = (v) => (Math.abs(v - .5) < .035 ? .5 : v);

//==============================================================================
// Knob factory: skirt / fluted cap (76%) / solid centre / 2px pointer
// rotating -135..+135 deg. o: {states, cur, label, ptr, size, touch, fmt,
// quant, w, inset (capin, default 5), ptrTop (default 2), horiz (label
// beside instead of beneath), lblCls}
function makeKnob(mount, o) {
  const size = o.size || 36;
  const inner = Math.round(size * .76);
  const wrap = el("div", "kwrap" + (o.horiz ? " horiz" : ""));
  if (!o.horiz) wrap.style.width = (o.w || (size + 12)) + "px";
  const skirt = el("div", "kskirt");
  skirt.style.width = skirt.style.height = size + "px";
  const cap = el("div", "kcap");
  cap.style.width = cap.style.height = inner + "px";
  const capin = el("div", "kcapin");
  if (o.inset) capin.style.inset = o.inset + "px";
  const rot = el("div", "krot");
  const ptr = el("div", "kptr");
  ptr.style.height = Math.round(inner * .34) + "px";
  if (o.ptrTop !== undefined) ptr.style.top = o.ptrTop + "px";
  if (o.ptr) ptr.style.background = o.ptr;
  rot.appendChild(ptr);
  cap.append(capin, rot);
  skirt.appendChild(cap);
  wrap.appendChild(skirt);
  if (o.label) {
    const l = el("div", o.lblCls || "lbl");
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
// Faceplate finish (v7, default ON): seeded brushed-metal grain (LCG
// a=16807 seed 19971, prototype-verbatim draw order so the screw slot
// angles match), top-light gradient, 122deg sheen, edge vignette, 4 corner
// screws (r8 radial-gradient heads, random-angle slot, centers 22px from
// corners). Star-field stays an off-by-default variant (seed 1994).
{
  const plate = $("plate");
  let seed = 19971;
  const mr = () => (seed = seed * 16807 % 2147483647) / 2147483647;

  const metal = svgEl("svg");
  metal.setAttribute("viewBox", "0 0 1140 660");
  for (let y = 1; y < 660; y += 4 + Math.floor(mr() * 5)) {
    const light = mr() > .45, o = (0.008 + mr() * .024).toFixed(3);
    const h = (0.5 + mr() * .7).toFixed(2);
    const r = svgEl("rect");
    r.setAttribute("x", 0); r.setAttribute("y", y);
    r.setAttribute("width", 1140); r.setAttribute("height", h);
    r.setAttribute("fill", light ? "#fff" : "#000");
    r.setAttribute("opacity", o);
    metal.appendChild(r);
  }
  for (let i = 0; i < 18; i++) {   // long highlight streaks
    const y = Math.floor(mr() * 660), x0 = Math.floor(mr() * 800);
    const len = 120 + Math.floor(mr() * 560);
    const r = svgEl("rect");
    r.setAttribute("x", x0); r.setAttribute("y", y);
    r.setAttribute("width", len); r.setAttribute("height", 0.8);
    r.setAttribute("rx", 0.4); r.setAttribute("fill", "#fff");
    r.setAttribute("opacity", (0.03 + mr() * .05).toFixed(3));
    metal.appendChild(r);
  }
  plate.appendChild(metal);
  plate.appendChild(el("div", "grad1"));   // top-light
  plate.appendChild(el("div", "grad2"));   // 122deg sheen
  plate.appendChild(el("div", "grad3"));   // vignette

  const screws = svgEl("svg");
  screws.setAttribute("viewBox", "0 0 1140 660");
  const defs = svgEl("defs");
  const grad = svgEl("radialGradient");
  grad.setAttribute("id", "scrG");
  grad.setAttribute("cx", ".38"); grad.setAttribute("cy", ".38"); grad.setAttribute("r", ".75");
  [[0, "#6b708f"], [.7, "#292b42"], [1, "#0d0d1a"]].forEach(([off, col]) => {
    const s = svgEl("stop");
    s.setAttribute("offset", off); s.setAttribute("stop-color", col);
    grad.appendChild(s);
  });
  defs.appendChild(grad);
  screws.appendChild(defs);
  [[22, 22], [1118, 22], [22, 638], [1118, 638]].forEach(([cx, cy]) => {
    const rot = Math.floor(mr() * 180);
    const head = svgEl("circle");
    head.setAttribute("cx", cx); head.setAttribute("cy", cy);
    head.setAttribute("r", 8); head.setAttribute("fill", "url(#scrG)");
    const slot = svgEl("rect");
    slot.setAttribute("x", cx - 4.5); slot.setAttribute("y", cy - 0.8);
    slot.setAttribute("width", 9); slot.setAttribute("height", 1.6);
    slot.setAttribute("rx", 0.6); slot.setAttribute("fill", "#05050c");
    slot.setAttribute("transform", "rotate(" + rot + " " + cx + " " + cy + ")");
    screws.append(head, slot);
  });
  plate.appendChild(screws);

  // star-field variant (80 seeded 1-2px dots), off by default
  const stars = $("stars");
  let sSeed = 1994;
  const sr = () => (sSeed = sSeed * 16807 % 2147483647) / 2147483647;
  for (let i = 0; i < 80; i++) {
    const d = el("div");
    d.style.left = (sr() * 100).toFixed(1) + "%";
    d.style.top = (sr() * 100).toFixed(1) + "%";
    d.style.width = d.style.height = (sr() > .8 ? 2 : 1) + "px";
    d.style.opacity = (.12 + sr() * .3).toFixed(2);
    stars.appendChild(d);
  }
}

//==============================================================================
// Header: preset steppers + LCD preset line (browser), wave glyph,
// MASTER knob (binds `master`), power LED is static.
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
// Preset apply: writes the complete patch (base + preset overrides) through
// the bound states -- setNormalisedValue / setValue / setChoiceIndex are the
// relay setters, so the host learns of every change in plugin mode; the
// mock states fire the same listeners standalone. Index/name = UI-only.
function drawPresetLbl() {
  $("presetLbl").textContent =
    "P" + String(PRESET.i + 1).padStart(3, "0") + " " + PRESETS[PRESET.i].name;
}
function applyPreset(pi) {
  PRESET.i = ((pi % PRESETS.length) + PRESETS.length) % PRESETS.length;
  const P = PRESETS[PRESET.i];
  P.t.forEach((td, i) => {
    const sx = SFX[i];
    const sl = Object.assign({}, TONE_SLIDER_BASE,
      { dir: i * .25, level: td.lv !== undefined ? td.lv : .7 }, td.s || {});
    const tg = Object.assign({}, TONE_TOGGLE_BASE, { on: !td.off }, td.b || {});
    const cb = Object.assign({}, TONE_COMBO_BASE, { wave: td.w || 0 }, td.c || {});
    Object.keys(sl).forEach((k) => sliderState(k + sx).setNormalisedValue(sl[k]));
    Object.keys(tg).forEach((k) => toggleState(k + sx).setValue(!!tg[k]));
    Object.keys(cb).forEach((k) => comboState(k + sx).setChoiceIndex(cb[k]));
  });
  const gs = Object.assign({}, GLOB_SLIDER_BASE, P.s || {});
  const gt = Object.assign({}, GLOB_TOGGLE_BASE, P.b || {});
  const gc = Object.assign({}, GLOB_COMBO_BASE, P.c || {});
  Object.keys(gs).forEach((k) => sliderState(k).setNormalisedValue(gs[k]));
  Object.keys(gt).forEach((k) => toggleState(k).setValue(!!gt[k]));
  Object.keys(gc).forEach((k) => comboState(k).setChoiceIndex(gc[k]));
  drawPresetLbl();
}
$("presetUp").addEventListener("click", () => applyPreset(PRESET.i - 1));   // wraps
$("presetDn").addEventListener("click", () => applyPreset(PRESET.i + 1));
$("presetLbl").addEventListener("click", () =>
  openMenu("PRESET SELECT", PRESETS.map((p) => ({ cat: p.cat, name: p.name })),
    PRESET.i, applyPreset));

//==============================================================================
// TONES mixer strip: per column select LED+button, 130px level fader,
// PAN mini-knob (24px, bipolar, center detent), ON LED+button.
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

    makeVSlider(col, { states: [sliderState("level" + SFX[i])], h: 130, touch: () => "LEVEL " + name });

    makeKnob(col, {
      states: [sliderState("pan" + SFX[i])], label: "PAN", size: 24,
      inset: 4, ptrTop: 1, lblCls: "tonlbl", w: 34,
      touch: () => "PAN " + name, fmt: "bip", quant: detent,
    });

    const onLed = el("div", "led"); onLed.style.marginTop = "1px";
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

// v7 knob row (34px knobs at 40px pitch): OCTV FINE START [RND] | divider |
// LEVEL VELO SHP-DPT NOISE NS-COL. PAN lives in the TONES mixer strip.
// The RND button+LED (start_random_[t]) sits next to START per GUI_SPEC.
[["oct", "OCTV", { fmt: "oct", quant: (v) => Math.round(v * 4) / 4 }],
 ["fine", "FINE", { fmt: "bip" }],
 ["start", "START", {}],
 ["level", "LEVEL", {}],
 ["velo", "VELO", {}],
 ["shape_depth", "SHP DPT", { ptr: "#ffd23f" }],
 ["noise", "NOISE", {}],
 ["noise_color", "NS COL", {}]].forEach(([base, lbl, o]) => {
  makeKnob($("teKnobs"), {
    states: tSliders(base), cur: curTone, label: lbl, size: 34, w: 40,
    ptr: o.ptr || null, touch: lbl, fmt: o.fmt, quant: o.quant,
  });
  if (base === "start") {   // START RND: per-tone note-on start randomize
    const col = el("div", "rndcol");
    const led = el("div", "led");
    const btn = el("div", "pbtn");
    const lb = el("div", "tonlbl"); lb.textContent = "RND";
    col.append(led, btn, lb);
    $("teKnobs").appendChild(col);
    $("teKnobs").appendChild(el("div", "tekdiv"));   // 1px divider before LEVEL
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

// TVF: TYPE LCD (click = menu) + 4 red knobs + three ADSR banks
// (FILTER / AMPLITUDE / AUX, 1px dividers between them).
const tvfTypeStates = tCombos("tvf_type");
comboDraw(tvfTypeStates, true, () => {
  $("tvfTypeLcd").textContent = TVFTYPES[idx(tvfTypeStates[SEL.i], TVFTYPES.length)];
});
$("tvfTypeLcd").addEventListener("click", () =>
  openMenu("TVF TYPE — TONE " + letter(), rowsOf(TVFFULL),
    idx(tvfTypeStates[SEL.i], TVFTYPES.length), (i) => tvfTypeStates[SEL.i].setChoiceIndex(i)));

[["tvf_cut", "CUT"], ["tvf_res", "RES"], ["tvf_env", "ENV"], ["tvf_kf", "KF"]].forEach(([base, lbl]) => {
  makeKnob($("tvfKnobs"), {
    states: tSliders(base), cur: curTone, label: lbl, size: 34, ptr: "#ff5b6e", touch: "TVF " + lbl,
  });
});

function makeBank(mountId, prefix, title, touchPfx) {
  const bank = $(mountId);
  const row = el("div", "bankrow");
  bank.appendChild(row);
  [["a", "A"], ["d", "D"], ["s", "S"], ["r", "R"]].forEach(([sfx, l]) => {
    makeVSlider(row, {
      states: tSliders(prefix + "_" + sfx), cur: curTone, h: 62, letter: l,
      touch: () => touchPfx + " " + l,
    });
  });
  const t = el("div", "banktitle");
  const tl = el("div", "bttl");
  tl.textContent = title;
  t.appendChild(tl);
  bank.appendChild(t);
  return t;
}
makeBank("bankTvf", "tvf", "FILTER", "TVF");
makeBank("bankTva", "tva", "AMPLITUDE", "TVA");
makeBank("bankAux", "aux", "AUX", "AUX");

// v7 per-tone LFO1 / LFO2 rows: RATE + DEPTH (26px), SYNC btn+LED, DEST
// select. SYNC lit: RATE pointer turns yellow and the knob label / touched
// line show the quantized tempo division (idx = round(rate*11), SYNCDIVS).
function makeLfoRow(mountId, n) {
  const mount = $(mountId);
  const rateStates = tSliders("lfo" + n + "_rate");
  const depthStates = tSliders("lfo" + n + "_depth");
  const syncStates = tToggles("lfo" + n + "_sync");
  const destStates = tCombos("lfo" + n + "_dest");
  const divOf = () => SYNCDIVS[Math.round(
    clamp01(rateStates[SEL.i].getNormalisedValue()) * (SYNCDIVS.length - 1))];

  const name = el("div", "lfoname"); name.textContent = "LFO" + n;
  mount.appendChild(name);

  // RATE knob (dynamic pointer color + label)
  const wrap = el("div", "kwrap");
  wrap.style.width = "38px";
  const skirt = el("div", "kskirt");
  skirt.style.width = skirt.style.height = "26px";
  const cap = el("div", "kcap");
  cap.style.width = cap.style.height = "20px";
  const capin = el("div", "kcapin"); capin.style.inset = "4px";
  const rot = el("div", "krot");
  const ptr = el("div", "kptr");
  ptr.style.height = "7px";
  rot.appendChild(ptr);
  cap.append(capin, rot);
  skirt.appendChild(cap);
  const rlbl = el("div", "plab7");
  wrap.append(skirt, rlbl);
  mount.appendChild(wrap);
  const drawRate = () => {
    const syn = !!syncStates[SEL.i].getValue();
    rot.style.transform = "rotate(" + (-135 + rateStates[SEL.i].getNormalisedValue() * 270) + "deg)";
    ptr.style.background = syn ? "#ffd23f" : "#e8eaff";
    rlbl.textContent = syn ? divOf() : "RATE";
  };
  rateStates.concat(syncStates).forEach((s) => s.valueChangedEvent.addListener(drawRate));
  rateStates.forEach((s) => s.propertiesChangedEvent.addListener(drawRate));
  rebinds.push(drawRate);
  drawRate();
  bindVDrag(skirt, () => rateStates[SEL.i],
    () => "LFO" + n + " " + (syncStates[SEL.i].getValue() ? divOf() : "RATE"), drawRate);

  makeKnob(mount, {
    states: depthStates, cur: curTone, label: "DEPTH", size: 26, inset: 4,
    w: 38, lblCls: "plab7", touch: "LFO" + n + " DEPTH",
  });

  const sync = el("div", "ledlbl");
  const led = el("div", "led");
  const btn = el("div", "syncbtn"); btn.textContent = "SYNC";
  sync.append(led, btn);
  mount.appendChild(sync);
  const drawSync = () => led.classList.toggle("on", !!syncStates[SEL.i].getValue());
  syncStates.forEach((s) => s.valueChangedEvent.addListener(drawSync));
  rebinds.push(drawSync);
  drawSync();
  btn.addEventListener("click", () => {
    const s = syncStates[SEL.i];
    s.setValue(!s.getValue());
  });

  const dl = el("div", "plbl"); dl.textContent = "DEST";
  const destBtn = el("div", "cbtn destbtn");
  mount.append(dl, destBtn);
  comboDraw(destStates, true, () => {
    destBtn.textContent = LFODESTS[idx(destStates[SEL.i], LFODESTS.length)];
  });
  destBtn.addEventListener("click", () =>
    openMenu("LFO" + n + " DEST — TONE " + letter(), rowsOf(LFODESTS),
      idx(destStates[SEL.i], LFODESTS.length), (i) => destStates[SEL.i].setChoiceIndex(i)));
}
makeLfoRow("lfo1Row", 1);
makeLfoRow("lfo2Row", 2);

// v7 AUX ENV row: bipolar AMT± knob (yellow, center detent, -63..+63) +
// DEST select (incl NOISE) + per-tone footnote.
{
  const mount = $("auxEnvRow");
  const name = el("div", "lfoname"); name.textContent = "AUX ENV";
  mount.appendChild(name);
  makeKnob(mount, {
    states: tSliders("aux_amt"), cur: curTone, label: "AMT ±", size: 26,
    inset: 4, w: 38, lblCls: "plab7", ptr: "#ffd23f",
    touch: "AUX AMT", fmt: "bip", quant: detent,
  });
  const dl = el("div", "plbl"); dl.textContent = "DEST";
  const destBtn = el("div", "cbtn destbtn");
  mount.append(dl, destBtn);
  const auxDestStates = tCombos("aux_dest");
  comboDraw(auxDestStates, true, () => {
    destBtn.textContent = AUXDESTS[idx(auxDestStates[SEL.i], AUXDESTS.length)];
  });
  destBtn.addEventListener("click", () =>
    openMenu("AUX ENV DEST — TONE " + letter(), rowsOf(AUXDESTS),
      idx(auxDestStates[SEL.i], AUXDESTS.length), (i) => auxDestStates[SEL.i].setChoiceIndex(i)));
  const note = el("div"); note.id = "footnote";
  note.textContent = "ALL TONE EDIT CONTROLS ARE PER-TONE";
  mount.appendChild(note);
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
// FILTERS: two strips + SER/PAR routing + v7 BAL 1<->2 balance knob.
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

makeKnob($("fbalMount"), {   // v7 filter output balance (bipolar)
  states: [sliderState("flt_bal")], label: "BAL 1↔2", size: 26, inset: 4,
  w: 38, lblCls: "plab7", ptr: "#ffd23f", touch: "FLT BAL", fmt: "bip", quant: detent,
});

//==============================================================================
// DREAM VECTOR: PHASE/RATE knobs, SHAPE select, ORBIT/P-ENV toggles,
// P-ENV mini page, signal-flow display, per-tone DIR/INT knob rows.
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

// ORBIT SHAPE select below the display (menu, prototype behavior)
const orbitShapeState = comboState("vec_orbit_shape");
comboDraw([orbitShapeState], false, () => {
  $("orbitShapeBtn").textContent = ORBSHAPES[idx(orbitShapeState, ORBSHAPES.length)];
});
$("orbitShapeBtn").addEventListener("click", () =>
  openMenu("ORBIT SHAPE", rowsOf(ORBSHAPES), idx(orbitShapeState, ORBSHAPES.length),
    (i) => orbitShapeState.setChoiceIndex(i)));

// v7 per-tone DIR + INT rows (all four tones bound directly, 22px yellow
// knobs; selected tone letter red).
const dirStates = tSliders("dir"), intStates = tSliders("vint");
const vtLetters = [];
{
  const mount = $("vecTones");
  TONES.forEach((name, i) => {
    const row = el("div", "vtrow");
    const lt = el("div", "vtletter"); lt.textContent = name;
    vtLetters.push(lt);
    row.appendChild(lt);
    makeKnob(row, { states: [dirStates[i]], label: "DIR " + name, size: 22,
      inset: 4, ptrTop: 1, ptr: "#ffd23f", horiz: true, lblCls: "lbl7", touch: "DIR " + name });
    makeKnob(row, { states: [intStates[i]], label: "INT " + name, size: 22,
      inset: 4, ptrTop: 1, ptr: "#ffd23f", horiz: true, lblCls: "lbl7", touch: "INT " + name });
    mount.appendChild(row);
  });
  const cap = el("div"); cap.id = "vecCap";
  cap.textContent = "DIR · INT";
  mount.appendChild(cap);
  const drawSel = () => vtLetters.forEach((l, j) => l.classList.toggle("sel", j === SEL.i));
  rebinds.push(drawSel);
  drawSel();
}

// P-ENV mini page: START/END angle + TIME knobs, LOOP toggle. Page on the
// scrim like the wave menu, instant open/close (no easing).
let penvOpen = false;
function openPenv() { penvOpen = true; $("penvOverlay").style.display = "flex"; }
function closePenv() { penvOpen = false; $("penvOverlay").style.display = "none"; }
$("penvEditBtn").addEventListener("click", openPenv);
$("penvOverlay").addEventListener("click", (e) => { if (e.target === $("penvOverlay")) closePenv(); });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_start")], label: "START", size: 38, ptr: "#ffd23f", touch: "PENV START" });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_end")], label: "END", size: 38, ptr: "#ffd23f", touch: "PENV END" });
makeKnob($("penvKnobs"), { states: [sliderState("vec_penv_time")], label: "TIME", size: 38, touch: "PENV TIME" });
bindToggleRow("penvLoopLed", "penvLoopBtn", toggleState("vec_penv_loop"));

// Params with no panel control stay wired (hidden states; report note):
// global humanize drift, per-voice orbit free-run.
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
// MOD MATRIX: 3 slots (SRC > DST, 26px BIPOLAR AMT knob w/ center detent,
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

  // 26px AMT knob (20px cap, 4px centre inset, 7px yellow pointer, no label)
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
    { fmt: "bip", quant: detent });

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
drawPresetLbl();            // boot: P001 ETHEREAL DAWN (index is UI-side)
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
//   ?menu    boots with the wave menu open (headless overlay screenshot)
//   ?presets boots with the preset browser open
//   ?penv    boots with the P-ENV mini page open
//   ?ens     demo state: tone A on an [ENS] loop + MOD FX on ENSEMBLE
//   ?preset=N  boots with factory preset N (1-based) applied
//   ?flat    faceplate finish off   ?stars  star-field variant on
const presetFlag = location.search.match(/preset=(\d+)/);
if (presetFlag) applyPreset(parseInt(presetFlag[1], 10) - 1);
if (location.search.indexOf("ens") >= 0) {
  waveStates[0].setChoiceIndex(82);              // Ens: SOLINA_DREAM
  comboState("modfx_type").setChoiceIndex(3);    // ENSEMBLE
}
if (location.search.indexOf("flat") >= 0) $("plate").style.display = "none";
if (location.search.indexOf("stars") >= 0) $("stars").style.display = "block";
if (location.search.indexOf("presets") >= 0)
  $("presetLbl").dispatchEvent(new MouseEvent("click"));
else if (location.search.indexOf("menu") >= 0)
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
