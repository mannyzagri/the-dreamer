# GAIN_STAGING.md — headroom, summing & clip prevention (DSP/VM Claude)

Reported symptom: loading presets and/or enabling reverb causes severe
saturation/clipping. Root cause is unbounded gain accumulation at several
summing points, none of which currently reserve headroom. Fix all of the
following; each is independently testable.

## Signal-path gain budget (target: internal peak <= 0 dBFS, aim -6)

Path: tones -> voice sum -> global filters -> FX(mod->delay->reverb) ->
master -> output. Gain can GROW at every stage below.

### 1. Tone-sum normalization  [biggest fix]
Four tones at level ~0.8 sum to ~3.2 before anything else. The vector gain
law scales PER TONE but not the SUM.
- Scale the summed tone bus so a full 4-tone patch sits near unity, not 3x.
- Use active-tone-aware scaling: sum / sqrt(max(1, nActiveTones)), or a
  fixed 0.5 bus trim if simpler. Apply BEFORE the global filters.
- Must be smooth when tones toggle on/off (one-pole the scale) to avoid
  zipper on preset load.

### 2. VOICING tap-sum (per DSP_BUILD §11)
POWER/DREAMY fan one tone into 3-4 taps. Confirm taps are summed with
equal-power gain 1/sqrt(nTaps) so DREAMY is not 4x louder than SINGLE.
If not yet implemented, this alone can quadruple a tone's level.

### 3. FX wet/dry MUST be equal-power, never additive  [reverb clip cause]
For EVERY effect (mod, delay, reverb):
    out = dry*(1-mix) + wet*mix     // CORRECT
    out = dry + wet*mix             // WRONG - instant clip as mix rises
This is the most likely "adding reverb clips" smoking gun. Audit each
Rhino FX port's final mix line. Reverb/delay also ADD energy (tails,
feedback); ensure the wet path itself is gain-normalized so a max-size
hall doesn't output > unity from a unity input.
- Delay feedback: clamp fb < 1.0 (e.g. max 0.95) and scale wet so high
  feedback doesn't build to clipping.

### 4. Resonant filter make-up
Ladder/Liquid/Classic at high resonance peak +12..+18 dB at cutoff. They
were voiced for Rubber-Rhino's single-osc level, now they see a 4-tone
sum. Do NOT change the ported filter math (CLAUDE.md rule 1). Instead:
- Apply resonance-compensation gain at the filter's INPUT or OUTPUT in the
  ADAPTER (RhinoFilterSlot), not inside the ported filter: trim input by
  a function of resonance (e.g. inGain = 1 - 0.4*res) so self-oscillation
  peaks stay bounded. Tune so a res-swept loud pad does not exceed 0 dBFS.

### 5. Output soft-clip / limiter  [safety net, also authentic]
After MASTER, before the DAC, add a gentle limiter:
- Option A (authentic): tanh soft-clip — output = tanh(x * drive)/tanh(drive)
  or a simple cubic soft-clip. Rounds transients like a real output stage,
  on-theme with the 12-bit character.
- Option B (transparent): lookahead brickwall limiter at -0.3 dBFS.
Recommend A for character + a hard ceiling at -0.1 dBFS behind it so
nothing ever digitally clips. This catches residual peaks from 1-4.

### 6. Preset headroom
Some factory presets (esp. 4-tone banks) are authored hot. After fix 1 is
in, re-check: target preset peak ~ -6 dBFS on a sustained chord. If any
preset still clips with fixes 1-5, trim its tone levels in presets.json
(don't rely on the limiter as the primary control).

## Quick diagnostic (do this first)
- Load a hot preset, set MASTER to minimum.
  * Still clips -> clip is BEFORE master (filter/tone-sum: fixes 1,2,4).
  * Clean -> sum is just hot (fixes 1,5).
- Clips ONLY when reverb enabled -> it's fix 3 (additive wet/dry). Verify
  the reverb mix line directly.

## Validation
- Render every factory preset (sustained 4-note chord, velocity 100) and
  assert peak <= -0.1 dBFS at the output.
- Sweep each resonant filter type at max res under a loud pad; assert no
  sample > 0 dBFS.
- Enable reverb at mix=1.0 on a unity signal; assert output <= unity.
