# Rompler ROM Bank v1 -- curated AKWF single-cycle waveforms

78 waveforms baked from AKWF-FREE (CC0), selected by spectral analysis for
90s ROMpler character (E-MU Orbit / Roland JD-990 territory).

Categories: Basic (4), Pad (18), String (8), Vox (7), Bell (20), Metal (21).

## Conditioning applied at bake time
- DC removal, peak normalize to 0.9995
- Requantized to 12-bit signed (ROM-authentic), stored left-justified in
  int16 (low nibble zero) so the quantization grain survives playback math

## Files
- RomplerBankData.h  -- constexpr sample arrays (600 x int16 each)
- RomplerBank.h      -- index: {category, name, samples}
- PcmOscillator.h    -- era-authentic playback: 16.16 fixed-point phase
                        accumulator, DropSample or integer Linear interp,
                        NO band-limiting (aliases on transposition by design),
                        sample-start offset support (E-MU trick)
- bake_bank.py       -- regenerate/extend the bank from an AKWF-FREE checkout
- test_bank.cpp      -- verification (pitch accuracy, 12-bit grain, render)

## Usage
    rompler::PcmOscillator osc;
    osc.setSampleRate(fs);
    osc.setWaveform(waveIndex);      // index into bank::kWaveforms
    osc.setFrequency(440.0 * std::pow(2.0, (note - 69) / 12.0));
    osc.reset(startOffset);          // per-partial phase offset, 0..599
    float s = osc.process();         // feed into Rubber-Rhino filter chain

Mode 1 (Rubber-Rhino topology): one PcmOscillator replaces the VA oscillator.
Mode 2 (ROMpler topology): two PcmOscillator partials per voice, independent
waveform/detune/level/envelope, per-partial TVF (JD-990 style).

## Mode 2 (added)
- Mode2Voice.h -- JD-990-style dual-partial voice: per-partial waveform,
  coarse/fine detune, level, velocity sens, sample-start offset, TVF
  (cutoff/res/env amount/key follow) + independent TVF/TVA ADSR envelopes,
  control-rate (16-sample) cutoff updates as the hardware did.
  FilterSlot seam: built-in SVF placeholder; adapt Rubber-Rhino filters to
  the 3-method interface and inject via Partial::setFilter().
  Mode2Synth<N> = poly manager with oldest-voice stealing.
- test_mode2.cpp -- verification + renders demo_bell.wav / demo_stringpad.wav
