# The Dreamer — VALIDATION (release candidate 0.1.0, 2026-07-17)

Build: Release x64, VS2022 / MSVC 14.44, JUCE 8.0.4, VST3 only.
Bundle: `The Dreamer.vst3` (identity Mnsh/Drmr) — deployed to
`C:\the-dreamer\` and `\\VBOXSVR\vagrant\` (share root).

## Checklist results

| Check | Result |
|---|---|
| Release build warnings introduced by the port | **0 new.** Sole warning: C4457 at RhinoFilter.h:300 — present verbatim in the donor (Rubber-Rhino builds with the same flags); untouchable under CLAUDE.md rule 1. |
| pluginval strictness 8 (VST3) | **SUCCESS** (pluginval 1.x, Dec 2024 Windows release; full run incl. editor, automation, fuzz params) |
| Sample-rate sweep | **SUCCESS** — pluginval re-run pinned to 44100/48000/96000 Hz × block sizes 64/256/1024 |
| Filter null test (donor vs port, all 7 types × oversample 1&2 × res 0/.5/.985) | **bitwise identical** (exceeds the −120 dB RMS bar; test_filter_port.exe) |
| FX null tests (Distortion×6, DCBlocker, Truncate16/12, StereoDelay×3, ReturnFilter×6, ModDelayFx, Phaser, SpringReverb, Instability, CompLimiter, BrickwallClip, Lfo×5) | **bitwise identical** per class (test_fx_port.exe) |
| Pitch: bank Sine at A440, 48 kHz, 1 s | **440 zero crossings exactly** (test_bank.exe; also through the full voice: 396 crossings/0.9 s in test_voice.exe) |
| 12-bit grain intact | **(sample & 0xF) == 0 across all 78 × 600 samples** (test_bank.exe) |
| No audio-thread allocation | **0 allocations** during 24-voice process() (counting global new/delete, test_voice.exe) |
| Denormals | Same discipline as the donor: `juce::ScopedNoDenormals` at the top of processBlock is the only guard, matching Rubber-Rhino exactly; test harnesses set FTZ/DAZ (`_mm_setcsr`) to mirror it |
| Cubase 15 manual pass | **PENDING (user)** — new plugin identity `Drmr`: Cubase needs a full plugin re-scan on first load; VST3s don't hot-reload |

## Extra behavioral tests (test_voice.exe)

- Oldest-note stealing verified with a discriminating scenario a per-voice
  serial (the verified bank's stamp bug) would fail.
- Depth-0 LFOs are a bit-exact no-op; key-trigged renders deterministic.
- Square pitch-LFO at depth 1 measured 495 crossings/0.9 s (expected ~495
  for the ±12 st law).
- Tremolo bounded by dry level, never silent.
- Control-rate law: exactly one cutoff update per 16 samples (spy FilterSlot).
- Pitch bend +12 st doubles rendered frequency (396 → 880 crossings).
- Demo renders through the Rhino filter: `dream_bell_rhino.wav`,
  `dream_stringpad_rhino.wav` (this directory, untracked).

## How to re-run everything

    call C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat
    cd C:\the-dreamer
    cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_bank.cpp /Fo:tests\bin\ /Fe:tests\bin\test_bank.exe
    cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I. tests\test_filter_port.cpp /Fo:tests\bin\ /Fe:tests\bin\test_filter_port.exe
    cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES /I. tests\test_fx_port.cpp /Fo:tests\bin\ /Fe:tests\bin\test_fx_port.exe
    cl /nologo /std:c++17 /O2 /EHsc /DRHINO_TEST_FEATURES=1 /D_USE_MATH_DEFINES /I. tests\test_voice.cpp /Fo:tests\bin\ /Fe:tests\bin\test_voice.exe
    tests\bin\test_bank.exe && tests\bin\test_filter_port.exe && tests\bin\test_fx_port.exe && tests\bin\test_voice.exe validation
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/the-dreamer/deps/JUCE
    cmake --build build --config Release
    pluginval --strictness-level 8 --validate "build\TheDreamer_artefacts\Release\VST3\The Dreamer.vst3"

(pluginval is not kept on the VM; download `pluginval_Windows.zip` from the
Tracktion GitHub releases via Invoke-WebRequest with TLS 1.2.)
