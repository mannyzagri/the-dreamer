// Auto-generated waveform bank index (AKWF-FREE, CC0)
#pragma once
#include "RomplerBankData.h"

namespace rompler::bank {

struct Waveform {
    const char* category;
    const char* name;
    const int16_t* samples;   // 600 samples, 12-bit left-justified in int16
};

inline constexpr Waveform kWaveforms[] = {
    { "Basic", "Saw", data::wf_basic_saw },
    { "Basic", "Square", data::wf_basic_square },
    { "Basic", "Triangle", data::wf_basic_triangle },
    { "Basic", "Sine", data::wf_basic_sine },
    { "Pad", "SoftSaw 1", data::wf_pad_softsaw_1 },
    { "Pad", "SoftSaw 2", data::wf_pad_softsaw_2 },
    { "Pad", "SoftSaw 3", data::wf_pad_softsaw_3 },
    { "Pad", "AsymSaw 1", data::wf_pad_asymsaw_1 },
    { "Pad", "AsymSaw 2", data::wf_pad_asymsaw_2 },
    { "Pad", "Hollow 1", data::wf_pad_hollow_1 },
    { "Pad", "Hollow 2", data::wf_pad_hollow_2 },
    { "Pad", "Hollow 3", data::wf_pad_hollow_3 },
    { "Pad", "Hollow 4", data::wf_pad_hollow_4 },
    { "Pad", "Tannerin", data::wf_pad_tannerin },
    { "Pad", "Breath 1", data::wf_pad_breath_1 },
    { "Pad", "Breath 2", data::wf_pad_breath_2 },
    { "Pad", "Breath 3", data::wf_pad_breath_3 },
    { "Pad", "Glass Organ 1", data::wf_pad_glass_organ_1 },
    { "Pad", "Glass Organ 2", data::wf_pad_glass_organ_2 },
    { "Pad", "Glass Organ 3", data::wf_pad_glass_organ_3 },
    { "Pad", "SinHarm 1", data::wf_pad_sinharm_1 },
    { "Pad", "SinHarm 2", data::wf_pad_sinharm_2 },
    { "String", "StringBox 1", data::wf_string_stringbox_1 },
    { "String", "StringBox 2", data::wf_string_stringbox_2 },
    { "String", "StringBox 3", data::wf_string_stringbox_3 },
    { "String", "Violin 1", data::wf_string_violin_1 },
    { "String", "Violin 2", data::wf_string_violin_2 },
    { "String", "Cello 1", data::wf_string_cello_1 },
    { "String", "Cello 2", data::wf_string_cello_2 },
    { "String", "Cello 3", data::wf_string_cello_3 },
    { "Vox", "Voice 1", data::wf_vox_voice_1 },
    { "Vox", "Voice 2", data::wf_vox_voice_2 },
    { "Vox", "Voice 3", data::wf_vox_voice_3 },
    { "Vox", "Voice 4", data::wf_vox_voice_4 },
    { "Vox", "Voice 5", data::wf_vox_voice_5 },
    { "Vox", "Voice Bright 1", data::wf_vox_voice_bright_1 },
    { "Vox", "Voice Bright 2", data::wf_vox_voice_bright_2 },
    { "Bell", "FM Bell 1", data::wf_bell_fm_bell_1 },
    { "Bell", "FM Bell 2", data::wf_bell_fm_bell_2 },
    { "Bell", "FM Bell 3", data::wf_bell_fm_bell_3 },
    { "Bell", "FM Bell 4", data::wf_bell_fm_bell_4 },
    { "Bell", "FM Bell 5", data::wf_bell_fm_bell_5 },
    { "Bell", "FM Bell 6", data::wf_bell_fm_bell_6 },
    { "Bell", "Tine Bright", data::wf_bell_tine_bright },
    { "Bell", "EP Tine 1", data::wf_bell_ep_tine_1 },
    { "Bell", "EP Tine 2", data::wf_bell_ep_tine_2 },
    { "Bell", "DigiBell 1", data::wf_bell_digibell_1 },
    { "Bell", "DigiBell 2", data::wf_bell_digibell_2 },
    { "Bell", "DigiBell 3", data::wf_bell_digibell_3 },
    { "Bell", "DigiBell 4", data::wf_bell_digibell_4 },
    { "Bell", "DigiBell 5", data::wf_bell_digibell_5 },
    { "Bell", "Chime 1", data::wf_bell_chime_1 },
    { "Bell", "Chime 2", data::wf_bell_chime_2 },
    { "Bell", "Chime 3", data::wf_bell_chime_3 },
    { "Bell", "Hollow Bell 1", data::wf_bell_hollow_bell_1 },
    { "Bell", "Hollow Bell 2", data::wf_bell_hollow_bell_2 },
    { "Bell", "Hollow Bell 3", data::wf_bell_hollow_bell_3 },
    { "Metal", "Spectrum 1", data::wf_metal_spectrum_1 },
    { "Metal", "Spectrum 2", data::wf_metal_spectrum_2 },
    { "Metal", "Spectrum 3", data::wf_metal_spectrum_3 },
    { "Metal", "Spectrum 4", data::wf_metal_spectrum_4 },
    { "Metal", "Spectrum 5", data::wf_metal_spectrum_5 },
    { "Metal", "Spectrum 6", data::wf_metal_spectrum_6 },
    { "Metal", "Spectrum 7", data::wf_metal_spectrum_7 },
    { "Metal", "RawMetal 1", data::wf_metal_rawmetal_1 },
    { "Metal", "RawMetal 2", data::wf_metal_rawmetal_2 },
    { "Metal", "RawMetal 3", data::wf_metal_rawmetal_3 },
    { "Metal", "RawMetal 4", data::wf_metal_rawmetal_4 },
    { "Metal", "Clang 1", data::wf_metal_clang_1 },
    { "Metal", "Clang 2", data::wf_metal_clang_2 },
    { "Metal", "Clang 3", data::wf_metal_clang_3 },
    { "Metal", "BitHash 1", data::wf_metal_bithash_1 },
    { "Metal", "BitHash 2", data::wf_metal_bithash_2 },
    { "Metal", "BitHash 3", data::wf_metal_bithash_3 },
    { "Metal", "BitHash 4", data::wf_metal_bithash_4 },
    { "Metal", "GrainMetal 1", data::wf_metal_grainmetal_1 },
    { "Metal", "GrainMetal 2", data::wf_metal_grainmetal_2 },
    { "Metal", "GrainMetal 3", data::wf_metal_grainmetal_3 },
};

inline constexpr int kNumWaveforms = sizeof(kWaveforms) / sizeof(kWaveforms[0]);
inline constexpr int kCycleLength  = data::kCycleLength;

} // namespace rompler::bank
