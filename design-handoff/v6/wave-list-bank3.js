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
  { cat: "Pad", name: "PAD_01", tag: "ENS" },   // 78
  { cat: "Pad", name: "PAD_02", tag: "ENS" },   // 79
  { cat: "Pad", name: "PAD_03", tag: "ENS" },   // 80
  { cat: "Pad", name: "PAD_04", tag: "ENS" },   // 81
  { cat: "Pad", name: "PAD_05", tag: "ENS" },   // 82
  { cat: "Pad", name: "PAD_06", tag: "ENS" },   // 83
  { cat: "Pad", name: "PAD_07", tag: "ENS" },   // 84
  { cat: "Pad", name: "PAD_08", tag: "ENS" },   // 85
  { cat: "Pad", name: "PAD_09", tag: "ENS" },   // 86
  { cat: "Pad", name: "PAD_10", tag: "ENS" },   // 87
  { cat: "Pad", name: "PAD_11", tag: "ENS" },   // 88
  { cat: "Pad", name: "PAD_12", tag: "ENS" },   // 89
  { cat: "Pad", name: "PAD_13", tag: "ENS" },   // 90
  { cat: "Pad", name: "PAD_14", tag: "ENS" },   // 91
  { cat: "Pad", name: "PAD_15", tag: "ENS" },   // 92
  { cat: "Pad", name: "PAD_16", tag: "ENS" },   // 93
  { cat: "Pad", name: "PAD_17", tag: "ENS" },   // 94
  { cat: "Pad", name: "PAD_18", tag: "ENS" },   // 95
  { cat: "Pad", name: "PAD_19", tag: "ENS" },   // 96
  { cat: "Pad", name: "PAD_20", tag: "ENS" },   // 97
  { cat: "Pad", name: "PAD_21", tag: "ENS" },   // 98
  { cat: "Pad", name: "PAD_22", tag: "ENS" },   // 99
  { cat: "Pad", name: "PAD_23", tag: "ENS" },   // 100
  { cat: "Pad", name: "PAD_24", tag: "ENS" },   // 101
  { cat: "Pad", name: "PAD_25", tag: "ENS" },   // 102
  { cat: "Pad", name: "PAD_26", tag: "ENS" },   // 103
  { cat: "Pad", name: "PAD_27", tag: "ENS" },   // 104
  { cat: "Pad", name: "PAD_28", tag: "ENS" },   // 105
  { cat: "Airy", name: "AIRY_01", tag: "ENS" },   // 106
  { cat: "Airy", name: "AIRY_02", tag: "ENS" },   // 107
  { cat: "Airy", name: "AIRY_03", tag: "ENS" },   // 108
  { cat: "Airy", name: "AIRY_04", tag: "ENS" },   // 109
  { cat: "Airy", name: "AIRY_05", tag: "ENS" },   // 110
  { cat: "Airy", name: "AIRY_06", tag: "ENS" },   // 111
  { cat: "Airy", name: "AIRY_07", tag: "ENS" },   // 112
  { cat: "Airy", name: "AIRY_08", tag: "ENS" },   // 113
  { cat: "Airy", name: "AIRY_09", tag: "ENS" },   // 114
  { cat: "Airy", name: "AIRY_10", tag: "ENS" },   // 115
  { cat: "Airy", name: "AIRY_11", tag: "ENS" },   // 116
  { cat: "Airy", name: "AIRY_12", tag: "ENS" },   // 117
  { cat: "Airy", name: "AIRY_13", tag: "ENS" },   // 118
  { cat: "Airy", name: "AIRY_14", tag: "ENS" },   // 119
  { cat: "Airy", name: "AIRY_15", tag: "ENS" },   // 120
  { cat: "Airy", name: "AIRY_16", tag: "ENS" },   // 121
  { cat: "Airy", name: "AIRY_17", tag: "ENS" },   // 122
  { cat: "Airy", name: "AIRY_18", tag: "ENS" },   // 123
  { cat: "Vox", name: "VOX_01", tag: "ENS" },   // 124
  { cat: "Vox", name: "VOX_02", tag: "ENS" },   // 125
  { cat: "Vox", name: "VOX_03", tag: "ENS" },   // 126
  { cat: "Vox", name: "VOX_04", tag: "ENS" },   // 127
  { cat: "Vox", name: "VOX_05", tag: "ENS" },   // 128
  { cat: "Vox", name: "VOX_06", tag: "ENS" },   // 129
  { cat: "Vox", name: "VOX_07", tag: "ENS" },   // 130
  { cat: "Vox", name: "VOX_08", tag: "ENS" },   // 131
  { cat: "Vox", name: "VOX_09", tag: "ENS" },   // 132
  { cat: "Vox", name: "VOX_10", tag: "ENS" },   // 133
  { cat: "Vox", name: "VOX_11", tag: "ENS" },   // 134
  { cat: "Vox", name: "VOX_12", tag: "ENS" },   // 135
  { cat: "Vox", name: "VOX_13", tag: "ENS" },   // 136
  { cat: "Vox", name: "VOX_14", tag: "ENS" },   // 137
  { cat: "Vox", name: "VOX_15", tag: "ENS" },   // 138
  { cat: "Vox", name: "VOX_16", tag: "ENS" },   // 139
  { cat: "Ether", name: "ETHER_01", tag: "ENS" },   // 140
  { cat: "Ether", name: "ETHER_02", tag: "ENS" },   // 141
  { cat: "Ether", name: "ETHER_03", tag: "ENS" },   // 142
  { cat: "Ether", name: "ETHER_04", tag: "ENS" },   // 143
  { cat: "Ether", name: "ETHER_05", tag: "ENS" },   // 144
  { cat: "Ether", name: "ETHER_06", tag: "ENS" },   // 145
  { cat: "Ether", name: "ETHER_07", tag: "ENS" },   // 146
  { cat: "Ether", name: "ETHER_08", tag: "ENS" },   // 147
  { cat: "Ether", name: "ETHER_09", tag: "ENS" },   // 148
  { cat: "Ether", name: "ETHER_10", tag: "ENS" },   // 149
  { cat: "Ether", name: "ETHER_11", tag: "ENS" },   // 150
  { cat: "Ether", name: "ETHER_12", tag: "ENS" },   // 151
  { cat: "Ether", name: "ETHER_13", tag: "ENS" },   // 152
  { cat: "Ether", name: "ETHER_14", tag: "ENS" },   // 153
  { cat: "Ether", name: "ETHER_15", tag: "ENS" },   // 154
  { cat: "Ether", name: "ETHER_16", tag: "ENS" },   // 155
  { cat: "FM", name: "FM_01", tag: "ENS" },   // 156
  { cat: "FM", name: "FM_02", tag: "ENS" },   // 157
  { cat: "FM", name: "FM_03", tag: "ENS" },   // 158
  { cat: "FM", name: "FM_04", tag: "ENS" },   // 159
  { cat: "FM", name: "FM_05", tag: "ENS" },   // 160
  { cat: "FM", name: "FM_06", tag: "ENS" },   // 161
  { cat: "FM", name: "FM_07", tag: "ENS" },   // 162
  { cat: "FM", name: "FM_08", tag: "ENS" },   // 163
  { cat: "FM", name: "FM_09", tag: "ENS" },   // 164
  { cat: "FM", name: "FM_10", tag: "ENS" },   // 165
  { cat: "FM", name: "FM_11", tag: "ENS" },   // 166
  { cat: "FM", name: "FM_12", tag: "ENS" },   // 167
  { cat: "FM", name: "FM_13", tag: "ENS" },   // 168
  { cat: "FM", name: "FM_14", tag: "ENS" },   // 169
  { cat: "Wind", name: "WIND_01", tag: "ENS" },   // 170
  { cat: "Wind", name: "WIND_02", tag: "ENS" },   // 171
  { cat: "Wind", name: "WIND_03", tag: "ENS" },   // 172
  { cat: "Wind", name: "WIND_04", tag: "ENS" },   // 173
  { cat: "Wind", name: "WIND_05", tag: "ENS" },   // 174
  { cat: "Wind", name: "WIND_06", tag: "ENS" },   // 175
  { cat: "Wind", name: "WIND_07", tag: "ENS" },   // 176
  { cat: "Wind", name: "WIND_08", tag: "ENS" },   // 177
  { cat: "Wind", name: "WIND_09", tag: "ENS" },   // 178
  { cat: "Wind", name: "WIND_10", tag: "ENS" },   // 179
  { cat: "Wind", name: "WIND_11", tag: "ENS" },   // 180
  { cat: "Wind", name: "WIND_12", tag: "ENS" },   // 181
  { cat: "Metal", name: "METAL_01", tag: "ENS" },   // 182
  { cat: "Metal", name: "METAL_02", tag: "ENS" },   // 183
  { cat: "Metal", name: "METAL_03", tag: "ENS" },   // 184
  { cat: "Metal", name: "METAL_04", tag: "ENS" },   // 185
  { cat: "Metal", name: "METAL_05", tag: "ENS" },   // 186
  { cat: "Metal", name: "METAL_06", tag: "ENS" },   // 187
  { cat: "Metal", name: "METAL_07", tag: "ENS" },   // 188
  { cat: "Metal", name: "METAL_08", tag: "ENS" },   // 189
  { cat: "Metal", name: "METAL_09", tag: "ENS" },   // 190
  { cat: "Metal", name: "METAL_10", tag: "ENS" },   // 191
  { cat: "Metal", name: "METAL_11", tag: "ENS" },   // 192
  { cat: "Metal", name: "METAL_12", tag: "ENS" },   // 193
  { cat: "Morph", name: "MORPH_PADAIR", tag: "ENS" },   // 194
  { cat: "Morph", name: "MORPH_VOXETHER", tag: "ENS" },   // 195
  { cat: "Morph", name: "MORPH_STRWIND", tag: "ENS" },   // 196
  { cat: "Morph", name: "MORPH_ETHERFM", tag: "ENS" },   // 197
  { cat: "Morph", name: "MORPH_VOXMETAL", tag: "ENS" },   // 198
  { cat: "Morph", name: "MORPH_AIRGLASS", tag: "ENS" },   // 199
  { cat: "Morph", name: "MORPH_FMBELL", tag: "ENS" },   // 200
  { cat: "Morph", name: "MORPH_PADVOX", tag: "ENS" },   // 201
  { cat: "Morph", name: "MORPH_WINDVOX", tag: "ENS" },   // 202
  { cat: "Morph", name: "MORPH_METALAIR", tag: "ENS" },   // 203
  { cat: "Morph", name: "MORPH_PADPAD", tag: "ENS" },   // 204
  { cat: "Morph", name: "MORPH_VOXVOX", tag: "ENS" },   // 205
  { cat: "Morph", name: "MORPH_ETHMETAL", tag: "ENS" },   // 206
  { cat: "Morph", name: "MORPH_FMPAD", tag: "ENS" },   // 207
  { cat: "Hit", name: "HIT_AIR_SWELL", tag: "SHOT" },   // 208
  { cat: "Hit", name: "HIT_BELL_PING", tag: "SHOT" },   // 209
  { cat: "Hit", name: "HIT_BREATH", tag: "SHOT" },   // 210
  { cat: "Hit", name: "HIT_CHIFF", tag: "SHOT" },   // 211
  { cat: "Hit", name: "HIT_CLICK", tag: "SHOT" },   // 212
  { cat: "Hit", name: "HIT_MALLET", tag: "SHOT" },   // 213
  { cat: "Hit", name: "HIT_NOISE_HIT", tag: "SHOT" },   // 214
  { cat: "Hit", name: "HIT_PLUCK", tag: "SHOT" },   // 215
  { cat: "Hit", name: "HIT_THUMP", tag: "SHOT" },   // 216
  { cat: "Hit", name: "HIT_TINE_TICK", tag: "SHOT" },   // 217
];
