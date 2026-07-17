// Bank order is LOCKED (param choice index = array index)
const WAVES = [
  { cat: "Basic", name: "Saw" },   // 0
  { cat: "Basic", name: "Square" },   // 1
  { cat: "Basic", name: "Triangle" },   // 2
  { cat: "Basic", name: "Sine" },   // 3
  { cat: "Pad", name: "SoftSaw 1" },   // 4
  { cat: "Pad", name: "SoftSaw 2" },   // 5
  { cat: "Pad", name: "SoftSaw 3" },   // 6
  { cat: "Pad", name: "AsymSaw 1" },   // 7
  { cat: "Pad", name: "AsymSaw 2" },   // 8
  { cat: "Pad", name: "Hollow 1" },   // 9
  { cat: "Pad", name: "Hollow 2" },   // 10
  { cat: "Pad", name: "Hollow 3" },   // 11
  { cat: "Pad", name: "Hollow 4" },   // 12
  { cat: "Pad", name: "Tannerin" },   // 13
  { cat: "Pad", name: "Breath 1" },   // 14
  { cat: "Pad", name: "Breath 2" },   // 15
  { cat: "Pad", name: "Breath 3" },   // 16
  { cat: "Pad", name: "Glass Organ 1" },   // 17
  { cat: "Pad", name: "Glass Organ 2" },   // 18
  { cat: "Pad", name: "Glass Organ 3" },   // 19
  { cat: "Pad", name: "SinHarm 1" },   // 20
  { cat: "Pad", name: "SinHarm 2" },   // 21
  { cat: "String", name: "StringBox 1" },   // 22
  { cat: "String", name: "StringBox 2" },   // 23
  { cat: "String", name: "StringBox 3" },   // 24
  { cat: "String", name: "Violin 1" },   // 25
  { cat: "String", name: "Violin 2" },   // 26
  { cat: "String", name: "Cello 1" },   // 27
  { cat: "String", name: "Cello 2" },   // 28
  { cat: "String", name: "Cello 3" },   // 29
  { cat: "Vox", name: "Voice 1" },   // 30
  { cat: "Vox", name: "Voice 2" },   // 31
  { cat: "Vox", name: "Voice 3" },   // 32
  { cat: "Vox", name: "Voice 4" },   // 33
  { cat: "Vox", name: "Voice 5" },   // 34
  { cat: "Vox", name: "Voice Bright 1" },   // 35
  { cat: "Vox", name: "Voice Bright 2" },   // 36
  { cat: "Bell", name: "FM Bell 1" },   // 37
  { cat: "Bell", name: "FM Bell 2" },   // 38
  { cat: "Bell", name: "FM Bell 3" },   // 39
  { cat: "Bell", name: "FM Bell 4" },   // 40
  { cat: "Bell", name: "FM Bell 5" },   // 41
  { cat: "Bell", name: "FM Bell 6" },   // 42
  { cat: "Bell", name: "Tine Bright" },   // 43
  { cat: "Bell", name: "EP Tine 1" },   // 44
  { cat: "Bell", name: "EP Tine 2" },   // 45
  { cat: "Bell", name: "DigiBell 1" },   // 46
  { cat: "Bell", name: "DigiBell 2" },   // 47
  { cat: "Bell", name: "DigiBell 3" },   // 48
  { cat: "Bell", name: "DigiBell 4" },   // 49
  { cat: "Bell", name: "DigiBell 5" },   // 50
  { cat: "Bell", name: "Chime 1" },   // 51
  { cat: "Bell", name: "Chime 2" },   // 52
  { cat: "Bell", name: "Chime 3" },   // 53
  { cat: "Bell", name: "Hollow Bell 1" },   // 54
  { cat: "Bell", name: "Hollow Bell 2" },   // 55
  { cat: "Bell", name: "Hollow Bell 3" },   // 56
  { cat: "Metal", name: "Spectrum 1" },   // 57
  { cat: "Metal", name: "Spectrum 2" },   // 58
  { cat: "Metal", name: "Spectrum 3" },   // 59
  { cat: "Metal", name: "Spectrum 4" },   // 60
  { cat: "Metal", name: "Spectrum 5" },   // 61
  { cat: "Metal", name: "Spectrum 6" },   // 62
  { cat: "Metal", name: "Spectrum 7" },   // 63
  { cat: "Metal", name: "RawMetal 1" },   // 64
  { cat: "Metal", name: "RawMetal 2" },   // 65
  { cat: "Metal", name: "RawMetal 3" },   // 66
  { cat: "Metal", name: "RawMetal 4" },   // 67
  { cat: "Metal", name: "Clang 1" },   // 68
  { cat: "Metal", name: "Clang 2" },   // 69
  { cat: "Metal", name: "Clang 3" },   // 70
  { cat: "Metal", name: "BitHash 1" },   // 71
  { cat: "Metal", name: "BitHash 2" },   // 72
  { cat: "Metal", name: "BitHash 3" },   // 73
  { cat: "Metal", name: "BitHash 4" },   // 74
  { cat: "Metal", name: "GrainMetal 1" },   // 75
  { cat: "Metal", name: "GrainMetal 2" },   // 76
  { cat: "Metal", name: "GrainMetal 3" },   // 77
];
