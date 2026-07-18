// Bank3 order is LOCKED (wave_[t] choice index = array index)
const WAVES = [
  { cat: "Basic", name: "Saw", tag: "" },   // 0
  { cat: "Basic", name: "Square", tag: "" },   // 1
  { cat: "Basic", name: "Triangle", tag: "" },   // 2
  { cat: "Basic", name: "Sine", tag: "" },   // 3
  { cat: "Pad", name: "SoftSaw 1", tag: "" },   // 4
  { cat: "Pad", name: "SoftSaw 2", tag: "" },   // 5
  { cat: "Pad", name: "SoftSaw 3", tag: "" },   // 6
  { cat: "Pad", name: "AsymSaw 1", tag: "" },   // 7
  { cat: "Pad", name: "AsymSaw 2", tag: "" },   // 8
  { cat: "Pad", name: "Hollow 1", tag: "" },   // 9
  { cat: "Pad", name: "Hollow 2", tag: "" },   // 10
  { cat: "Pad", name: "Hollow 3", tag: "" },   // 11
  { cat: "Pad", name: "Hollow 4", tag: "" },   // 12
  { cat: "Pad", name: "Tannerin", tag: "" },   // 13
  { cat: "Pad", name: "Breath 1", tag: "" },   // 14
  { cat: "Pad", name: "Breath 2", tag: "" },   // 15
  { cat: "Pad", name: "Breath 3", tag: "" },   // 16
  { cat: "Pad", name: "Glass Organ 1", tag: "" },   // 17
  { cat: "Pad", name: "Glass Organ 2", tag: "" },   // 18
  { cat: "Pad", name: "Glass Organ 3", tag: "" },   // 19
  { cat: "Pad", name: "SinHarm 1", tag: "" },   // 20
  { cat: "Pad", name: "SinHarm 2", tag: "" },   // 21
  { cat: "String", name: "StringBox 1", tag: "" },   // 22
  { cat: "String", name: "StringBox 2", tag: "" },   // 23
  { cat: "String", name: "StringBox 3", tag: "" },   // 24
  { cat: "String", name: "Violin 1", tag: "" },   // 25
  { cat: "String", name: "Violin 2", tag: "" },   // 26
  { cat: "String", name: "Cello 1", tag: "" },   // 27
  { cat: "String", name: "Cello 2", tag: "" },   // 28
  { cat: "String", name: "Cello 3", tag: "" },   // 29
  { cat: "Vox", name: "Voice 1", tag: "" },   // 30
  { cat: "Vox", name: "Voice 2", tag: "" },   // 31
  { cat: "Vox", name: "Voice 3", tag: "" },   // 32
  { cat: "Vox", name: "Voice 4", tag: "" },   // 33
  { cat: "Vox", name: "Voice 5", tag: "" },   // 34
  { cat: "Vox", name: "Voice Bright 1", tag: "" },   // 35
  { cat: "Vox", name: "Voice Bright 2", tag: "" },   // 36
  { cat: "Bell", name: "FM Bell 1", tag: "" },   // 37
  { cat: "Bell", name: "FM Bell 2", tag: "" },   // 38
  { cat: "Bell", name: "FM Bell 3", tag: "" },   // 39
  { cat: "Bell", name: "FM Bell 4", tag: "" },   // 40
  { cat: "Bell", name: "FM Bell 5", tag: "" },   // 41
  { cat: "Bell", name: "FM Bell 6", tag: "" },   // 42
  { cat: "Bell", name: "Tine Bright", tag: "" },   // 43
  { cat: "Bell", name: "EP Tine 1", tag: "" },   // 44
  { cat: "Bell", name: "EP Tine 2", tag: "" },   // 45
  { cat: "Bell", name: "DigiBell 1", tag: "" },   // 46
  { cat: "Bell", name: "DigiBell 2", tag: "" },   // 47
  { cat: "Bell", name: "DigiBell 3", tag: "" },   // 48
  { cat: "Bell", name: "DigiBell 4", tag: "" },   // 49
  { cat: "Bell", name: "DigiBell 5", tag: "" },   // 50
  { cat: "Bell", name: "Chime 1", tag: "" },   // 51
  { cat: "Bell", name: "Chime 2", tag: "" },   // 52
  { cat: "Bell", name: "Chime 3", tag: "" },   // 53
  { cat: "Bell", name: "Hollow Bell 1", tag: "" },   // 54
  { cat: "Bell", name: "Hollow Bell 2", tag: "" },   // 55
  { cat: "Bell", name: "Hollow Bell 3", tag: "" },   // 56
  { cat: "Metal", name: "Spectrum 1", tag: "" },   // 57
  { cat: "Metal", name: "Spectrum 2", tag: "" },   // 58
  { cat: "Metal", name: "Spectrum 3", tag: "" },   // 59
  { cat: "Metal", name: "Spectrum 4", tag: "" },   // 60
  { cat: "Metal", name: "Spectrum 5", tag: "" },   // 61
  { cat: "Metal", name: "Spectrum 6", tag: "" },   // 62
  { cat: "Metal", name: "Spectrum 7", tag: "" },   // 63
  { cat: "Metal", name: "RawMetal 1", tag: "" },   // 64
  { cat: "Metal", name: "RawMetal 2", tag: "" },   // 65
  { cat: "Metal", name: "RawMetal 3", tag: "" },   // 66
  { cat: "Metal", name: "RawMetal 4", tag: "" },   // 67
  { cat: "Metal", name: "Clang 1", tag: "" },   // 68
  { cat: "Metal", name: "Clang 2", tag: "" },   // 69
  { cat: "Metal", name: "Clang 3", tag: "" },   // 70
  { cat: "Metal", name: "BitHash 1", tag: "" },   // 71
  { cat: "Metal", name: "BitHash 2", tag: "" },   // 72
  { cat: "Metal", name: "BitHash 3", tag: "" },   // 73
  { cat: "Metal", name: "BitHash 4", tag: "" },   // 74
  { cat: "Metal", name: "GrainMetal 1", tag: "" },   // 75
  { cat: "Metal", name: "GrainMetal 2", tag: "" },   // 76
  { cat: "Metal", name: "GrainMetal 3", tag: "" },   // 77
  { cat: "Ens", name: "STRINGBOX_ENS", tag: "ENS" },   // 78
  { cat: "Ens", name: "CATHEDRAL", tag: "ENS" },   // 79
  { cat: "Ens", name: "CHOIR_MORPH", tag: "ENS" },   // 80
  { cat: "Ens", name: "GLASS_CHOIR", tag: "ENS" },   // 81
  { cat: "Ens", name: "SOLINA_DREAM", tag: "ENS" },   // 82
  { cat: "Ens", name: "CELLO_SECTION", tag: "ENS" },   // 83
  { cat: "Ens", name: "THEREMIN_SWARM", tag: "ENS" },   // 84
  { cat: "Ens", name: "DARK_BREATH", tag: "ENS" },   // 85
  { cat: "Ens", name: "FM_HALO", tag: "ENS" },   // 86
  { cat: "Ens", name: "SPECTRAL_TIDE", tag: "ENS" },   // 87
  { cat: "Ens", name: "SAW_CLOUD", tag: "ENS" },   // 88
  { cat: "Ens", name: "TANNERIN_GHOST", tag: "ENS" },   // 89
  { cat: "Ens", name: "ORGAN_MASS", tag: "ENS" },   // 90
  { cat: "Ens", name: "VOICE_OF_STEEL", tag: "ENS" },   // 91
  { cat: "Ens", name: "BELL_GARDEN", tag: "ENS" },   // 92
  { cat: "Ens", name: "ACID_MIRAGE", tag: "ENS" },   // 93
  { cat: "Ens", name: "ETHEREAL_CHOIR", tag: "ENS" },   // 94
  { cat: "Ens", name: "OPERA_VIOLA", tag: "ENS" },   // 95
  { cat: "Ens", name: "CELLO_CHOIR", tag: "ENS" },   // 96
  { cat: "Ens", name: "STEEL_CHOIR", tag: "ENS" },   // 97
  { cat: "Ens", name: "CHROME_VOICES", tag: "ENS" },   // 98
  { cat: "Ens", name: "ORACLE", tag: "ENS" },   // 99
  { cat: "Ens", name: "STRING_OCTAVES", tag: "ENS" },   // 100
  { cat: "Ens", name: "GHOST_ORCHESTRA", tag: "ENS" },   // 101
  { cat: "Ens", name: "VIOLIN_SERAPH", tag: "ENS" },   // 102
  { cat: "Ens", name: "CATHEDRAL_STRINGS", tag: "ENS" },   // 103
  { cat: "Shot", name: "CHIFF", tag: "SHOT" },   // 104
  { cat: "Shot", name: "BREATH", tag: "SHOT" },   // 105
  { cat: "Shot", name: "CLICK", tag: "SHOT" },   // 106
  { cat: "Shot", name: "MALLET_TICK", tag: "SHOT" },   // 107
  { cat: "Shot", name: "NOISE_SWELL", tag: "SHOT" },   // 108
  { cat: "Shot", name: "GLASS_TICK", tag: "SHOT" },   // 109
  { cat: "Shot", name: "VOX_PLOSIVE", tag: "SHOT" },   // 110
  { cat: "Shot", name: "METAL_HIT", tag: "SHOT" },   // 111
  { cat: "Shot", name: "SUB_THUMP", tag: "SHOT" },   // 112
  { cat: "Shot", name: "TAPE_START", tag: "SHOT" },   // 113
];
