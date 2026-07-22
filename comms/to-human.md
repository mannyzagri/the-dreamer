# Inbox: human

## [NEW] 2026-07-22T10:15 · from:mac-opus → human · comms channel online + triage posted
comms/ bootstrapped in the-dreamer (rubber-rhino convention). Posted the full
2.5.x regression triage to vm-opus (comms/to-vm.md): noise bisect (task 1),
GUI resize pipeline check (task 2), library-v4 integration (task 3, blocked on
the re-bake you're arranging per REBAKE-V4-INSTRUCTIONS.md). Key correction on
record: the 2.5.1 all-220 roots change was correct; the mid-loop detune is
baked into the v3 WAVs (shared_drift Brownian bridge, measured ±8–11c).
---
## [NEW] 2026-07-22T23:55 · from:vm-opus → human · VERDICT TD-001: fixed, 2.5.2 ready — your go for release/deploy
The 0 dBFS noise is root-caused and fixed (worked with you live this session).
One-past-end heap read in the ENSEMBLE mod-fx delay reader (float-rounding
edge; also latent in Dimension/Rotary) — fires deterministically on SOLINA
FIELDS @44.1k ~14 s in, reverb traps the garbage for minutes, limiter pins it
at the ceiling = the noise you heard. Fixed on branch fix/td-001-noise +
D13 NaN-blindness fix ("NaN is NaN" honored — fmax was swallowing NaN) +
complete flush/prepare resets. Validator dsp 11/11, 2× full soak clean.
AWAITING YOUR GO: release.ps1 → 2.5.2 → deploy both targets. Ear-gate after:
SOLINA FIELDS @44.1k, play + idle 60 s → must stay clean (no re-scan needed).
New shared tool from the hunt: C:\code-bank\tools\vst3-probe (long-render
soak harness for all the plugins; found what pluginval+validator missed).
> UPDATE 2026-07-23T00:15 (go given in-session): **2.5.2 RELEASED + DEPLOYED**
> both targets, pluginval 8 SUCCESS, main `3e29fe7`, src export refreshed.
> Tool verdict: RE-SCAN required (moduleinfo changed) + remove/re-add the
> instance; header must read 2.5.2. Your ear gate is the only open item.
---
