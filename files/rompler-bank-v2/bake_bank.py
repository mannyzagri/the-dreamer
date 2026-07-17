#!/usr/bin/env python3
"""Bake curated AKWF single-cycle bank -> 12-bit ROM-style C++17 headers."""
import numpy as np, wave, os

SRC = "/home/claude/AKWF-FREE/AKWF"
OUT = "/home/claude/rompler-bank"
os.makedirs(OUT, exist_ok=True)

# (category, display name, relative path) -- curated from spectral analysis
BANK = [
  # --- Basic (classic ROM staples) ---
  ("Basic", "Saw",            "AKWF_bw_perfectwaves/AKWF_saw.wav"),
  ("Basic", "Square",         "AKWF_bw_perfectwaves/AKWF_squ.wav"),
  ("Basic", "Triangle",       "AKWF_bw_perfectwaves/AKWF_tri.wav"),
  ("Basic", "Sine",           "AKWF_bw_perfectwaves/AKWF_sin.wav"),
  # --- Pad / Airy ---
  ("Pad", "SoftSaw 1",        "AKWF_bw_sawrounded/AKWF_R_sym_saw_10.wav"),
  ("Pad", "SoftSaw 2",        "AKWF_bw_sawrounded/AKWF_R_sym_saw_12.wav"),
  ("Pad", "SoftSaw 3",        "AKWF_bw_sawrounded/AKWF_R_sym_saw_14.wav"),
  ("Pad", "AsymSaw 1",        "AKWF_bw_sawrounded/AKWF_R_asym_saw_09.wav"),
  ("Pad", "AsymSaw 2",        "AKWF_bw_sawrounded/AKWF_R_asym_saw_11.wav"),
  ("Pad", "Hollow 1",         "AKWF_theremin/AKWF_theremin_0003.wav"),
  ("Pad", "Hollow 2",         "AKWF_theremin/AKWF_theremin_0008.wav"),
  ("Pad", "Hollow 3",         "AKWF_theremin/AKWF_theremin_0013.wav"),
  ("Pad", "Hollow 4",         "AKWF_theremin/AKWF_theremin_0016.wav"),
  ("Pad", "Tannerin",         "AKWF_theremin/AKWF_tannerin_0002.wav"),
  ("Pad", "Breath 1",         "AKWF_flute/AKWF_flute_0003.wav"),
  ("Pad", "Breath 2",         "AKWF_clarinett/AKWF_clarinett_0014.wav"),
  ("Pad", "Breath 3",         "AKWF_clarinett/AKWF_clarinett_0019.wav"),
  ("Pad", "Glass Organ 1",    "AKWF_eorgan/AKWF_eorgan_0030.wav"),
  ("Pad", "Glass Organ 2",    "AKWF_eorgan/AKWF_eorgan_0048.wav"),
  ("Pad", "Glass Organ 3",    "AKWF_eorgan/AKWF_eorgan_0154.wav"),
  ("Pad", "SinHarm 1",        "AKWF_sinharm/AKWF_sinharm_0005.wav"),
  ("Pad", "SinHarm 2",        "AKWF_sinharm/AKWF_sinharm_0009.wav"),
  # --- Strings ---
  ("String", "StringBox 1",   "AKWF_stringbox/AKWF_cheeze_0001.wav"),
  ("String", "StringBox 2",   "AKWF_stringbox/AKWF_cheeze_0003.wav"),
  ("String", "StringBox 3",   "AKWF_stringbox/AKWF_cheeze_0006.wav"),
  ("String", "Violin 1",      "AKWF_violin/AKWF_violin_0008.wav"),
  ("String", "Violin 2",      "AKWF_violin/AKWF_violin_0003.wav"),
  ("String", "Cello 1",       "AKWF_cello/AKWF_cello_0003.wav"),
  ("String", "Cello 2",       "AKWF_cello/AKWF_cello_0005.wav"),
  ("String", "Cello 3",       "AKWF_cello/AKWF_cello_0010.wav"),
  # --- Vox ---
  ("Vox", "Voice 1",          "AKWF_hvoice/AKWF_hvoice_0007.wav"),
  ("Vox", "Voice 2",          "AKWF_hvoice/AKWF_hvoice_0017.wav"),
  ("Vox", "Voice 3",          "AKWF_hvoice/AKWF_hvoice_0024.wav"),
  ("Vox", "Voice 4",          "AKWF_hvoice/AKWF_hvoice_0097.wav"),
  ("Vox", "Voice 5",          "AKWF_hvoice/AKWF_hvoice_0099.wav"),
  ("Vox", "Voice Bright 1",   "AKWF_hvoice/AKWF_hvoice_0040.wav"),
  ("Vox", "Voice Bright 2",   "AKWF_hvoice/AKWF_hvoice_0060.wav"),
  # --- Bell ---
  ("Bell", "FM Bell 1",       "AKWF_fmsynth/AKWF_fmsynth_0118.wav"),
  ("Bell", "FM Bell 2",       "AKWF_fmsynth/AKWF_fmsynth_0014.wav"),
  ("Bell", "FM Bell 3",       "AKWF_fmsynth/AKWF_fmsynth_0054.wav"),
  ("Bell", "FM Bell 4",       "AKWF_fmsynth/AKWF_fmsynth_0058.wav"),
  ("Bell", "FM Bell 5",       "AKWF_fmsynth/AKWF_fmsynth_0057.wav"),
  ("Bell", "FM Bell 6",       "AKWF_fmsynth/AKWF_fmsynth_0102.wav"),
  ("Bell", "Tine Bright",     "AKWF_fmsynth/AKWF_fmsynth_0040.wav"),
  ("Bell", "EP Tine 1",       "AKWF_epiano/AKWF_epiano_0010.wav"),
  ("Bell", "EP Tine 2",       "AKWF_epiano/AKWF_epiano_0042.wav"),
  ("Bell", "DigiBell 1",      "AKWF_vgame/AKWF_vgame_0112.wav"),
  ("Bell", "DigiBell 2",      "AKWF_vgame/AKWF_vgame_0090.wav"),
  ("Bell", "DigiBell 3",      "AKWF_vgame/AKWF_vgame_0074.wav"),
  ("Bell", "DigiBell 4",      "AKWF_vgame/AKWF_vgame_0086.wav"),
  ("Bell", "DigiBell 5",      "AKWF_vgame/AKWF_vgame_0094.wav"),
  ("Bell", "Chime 1",         "AKWF_c604/AKWF_c604_0004.wav"),
  ("Bell", "Chime 2",         "AKWF_c604/AKWF_c604_0008.wav"),
  ("Bell", "Chime 3",         "AKWF_c604/AKWF_c604_0009.wav"),
  ("Bell", "Hollow Bell 1",   "AKWF_symetric/AKWF_symetric_0005.wav"),
  ("Bell", "Hollow Bell 2",   "AKWF_hdrawn/AKWF_hdrawn_0015.wav"),
  ("Bell", "Hollow Bell 3",   "AKWF_hdrawn/AKWF_hdrawn_0018.wav"),
  # --- Metallic ---
  ("Metal", "Spectrum 1",     "AKWF_overtone/AKWF_overtone_0014.wav"),
  ("Metal", "Spectrum 2",     "AKWF_overtone/AKWF_overtone_0015.wav"),
  ("Metal", "Spectrum 3",     "AKWF_overtone/AKWF_overtone_0021.wav"),
  ("Metal", "Spectrum 4",     "AKWF_overtone/AKWF_overtone_0034.wav"),
  ("Metal", "Spectrum 5",     "AKWF_overtone/AKWF_overtone_0036.wav"),
  ("Metal", "Spectrum 6",     "AKWF_overtone/AKWF_overtone_0038.wav"),
  ("Metal", "Spectrum 7",     "AKWF_overtone/AKWF_overtone_0042.wav"),
  ("Metal", "RawMetal 1",     "AKWF_raw/AKWF_raw_0001.wav"),
  ("Metal", "RawMetal 2",     "AKWF_raw/AKWF_raw_0015.wav"),
  ("Metal", "RawMetal 3",     "AKWF_raw/AKWF_raw_0029.wav"),
  ("Metal", "RawMetal 4",     "AKWF_raw/AKWF_raw_0031.wav"),
  ("Metal", "Clang 1",        "AKWF_snippets/AKWF_snippet_0024.wav"),
  ("Metal", "Clang 2",        "AKWF_snippets/AKWF_snippet_0033.wav"),
  ("Metal", "Clang 3",        "AKWF_snippets/AKWF_snippet_0041.wav"),
  ("Metal", "BitHash 1",      "AKWF_bitreduced/AKWF_bitreduced_0018.wav"),
  ("Metal", "BitHash 2",      "AKWF_bitreduced/AKWF_bitreduced_0030.wav"),
  ("Metal", "BitHash 3",      "AKWF_bitreduced/AKWF_bitreduced_0032.wav"),
  ("Metal", "BitHash 4",      "AKWF_bitreduced/AKWF_bitreduced_0034.wav"),
  ("Metal", "GrainMetal 1",   "AKWF_granular/AKWF_granular_0008.wav"),
  ("Metal", "GrainMetal 2",   "AKWF_c604/AKWF_c604_0016.wav"),
  ("Metal", "GrainMetal 3",   "AKWF_c604/AKWF_c604_0029.wav"),
]

def load(path):
    with wave.open(path, 'rb') as w:
        d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768.0
    return d

def condition(x):
    x = x - x.mean()                      # DC removal
    peak = np.max(np.abs(x))
    if peak > 0: x = x / peak * 0.9995    # normalize
    return x

def requantize12(x):
    """12-bit signed ROM quantization, stored left-justified in int16 (low 4 bits zero)."""
    q = np.clip(np.round(x * 2047.0), -2048, 2047).astype(np.int32)
    return (q << 4).astype(np.int16)      # playback: sample/32768.0, grain preserved

names_seen = set()
entries = []
for cat, name, rel in BANK:
    p = os.path.join(SRC, rel)
    assert os.path.exists(p), p
    data = requantize12(condition(load(p)))
    assert len(data) == 600, (rel, len(data))
    sym = f"wf_{cat.lower()}_{name.lower().replace(' ', '_')}"
    assert sym not in names_seen; names_seen.add(sym)
    entries.append((cat, name, sym, data, rel))

# ---- emit data header ----
with open(os.path.join(OUT, "RomplerBankData.h"), "w") as f:
    f.write("// Auto-generated from AKWF-FREE (CC0) -- 12-bit requantized ROM bank\n")
    f.write("// 600 samples per cycle, int16 left-justified 12-bit (low nibble zero)\n")
    f.write("#pragma once\n#include <cstdint>\n\nnamespace rompler::bank::data {\n\n")
    f.write("inline constexpr int kCycleLength = 600;\n\n")
    for cat, name, sym, data, rel in entries:
        f.write(f"// {cat} / {name}  ({rel})\n")
        f.write(f"inline constexpr int16_t {sym}[600] = {{\n")
        for i in range(0, 600, 12):
            f.write("    " + ", ".join(f"{v:6d}" for v in data[i:i+12]) + ",\n")
        f.write("};\n\n")
    f.write("} // namespace rompler::bank::data\n")

# ---- emit index header ----
with open(os.path.join(OUT, "RomplerBank.h"), "w") as f:
    f.write("""// Auto-generated waveform bank index (AKWF-FREE, CC0)
#pragma once
#include "RomplerBankData.h"

namespace rompler::bank {

struct Waveform {
    const char* category;
    const char* name;
    const int16_t* samples;   // 600 samples, 12-bit left-justified in int16
};

inline constexpr Waveform kWaveforms[] = {
""")
    for cat, name, sym, data, rel in entries:
        f.write(f'    {{ "{cat}", "{name}", data::{sym} }},\n')
    f.write("""};

inline constexpr int kNumWaveforms = sizeof(kWaveforms) / sizeof(kWaveforms[0]);
inline constexpr int kCycleLength  = data::kCycleLength;

} // namespace rompler::bank
""")

print(f"Baked {len(entries)} waveforms")
for c in ["Basic","Pad","String","Vox","Bell","Metal"]:
    print(f"  {c}: {sum(1 for e in entries if e[0]==c)}")
