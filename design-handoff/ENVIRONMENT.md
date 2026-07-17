# ENVIRONMENT.md — The Dreamer: VM & toolchain requirements

Read together with CLAUDE.md (rules & phases), FEATURES.md (synthesis
spec), DESIGN.md (visual). This file describes the Windows 11 Vagrant VM
environment that Claude Code sessions run in, what must exist before
phase work starts, and environment rules the agent must follow.

If any REQUIRED item below is missing when a phase begins, STOP and
report it — do not improvise substitutes or skip validation gates.

---

## 1. Toolchain (REQUIRED)

- **MSVC** — Visual Studio 2022 Build Tools, C++ workload (full VS not
  needed). x64 native tools must be reachable from the shell.
- **CMake ≥ 3.22** and **Ninja**. All builds go through CMake presets
  (see §4); Ninja is the generator. Never invoke MSBuild directly.
- **git** — on PATH, non-interactive (no credential prompts mid-session).
- **Python 3.10+** with **numpy** and **scipy** — required for bake
  scripts (bake_bank.py, bake_shapers.py, bake_frames.py) and all
  null-test / validation scripts.
- **pluginval** (Tracktion) — CLI binary on PATH. The CLAUDE.md
  checklist requires `pluginval --strictness-level 8` on the VST3.
  If pluginval is absent, the validation gate CANNOT be marked passed.
- *Optional but recommended:* **sccache** (or ccache for MSVC) — agentic
  workflows rebuild often; if present, presets enable it.

## 2. Source layout (REQUIRED)

    C:\src\rubber-rhino\      # donor codebase — READ-ONLY. Never edit,
                              # never build here. Port by copying files
                              # into the dreamer repo per CLAUDE.md.
    C:\src\the-dreamer\       # this project (git repo root)
      CLAUDE.md  FEATURES.md  DESIGN.md  ENVIRONMENT.md
      CMakeLists.txt  CMakePresets.json
      dsp\bank\               # rompler-bank v2 headers — READ-ONLY
      dsp\ported\  dsp\glue\  plugin\  tests\  validation\
    C:\src\JUCE\              # JUCE, pinned to the SAME TAG Rubber-Rhino
                              # uses (avoid API drift mid-port). JUCE
                              # bundles the VST3 SDK — no separate
                              # Steinberg download.

## 3. Build & test targets (create in phase 1 if absent)

- `dreamer_vst3` — the JUCE plugin target.
- `dreamer_render` — console exe: loads the engine headlessly, renders
  WAV files from patch + note-list arguments. This is the primary audio
  verification tool; the VM has no trustworthy audio device. ALL sonic
  acceptance in the VM is offline render + numeric comparison. Cubase 15
  on bare metal is the human pass only.
- `dreamer_tests` — unit tests (filter adapters, envelopes, bank
  invariants, allocation guard), run via ctest.

## 4. CMake presets (REQUIRED conventions)

CMakePresets.json defines `windows-debug` and `windows-release`
(Ninja, x64). The agent uses exactly:

    cmake --preset windows-release
    cmake --build --preset windows-release
    ctest --preset windows-release

Preset MUST set:
- `JUCE_COPY_PLUGIN_AFTER_BUILD=OFF` (or `JUCE_VST3_COPY_DIR` to a
  user-writable folder). The default copy step targets
  `C:\Program Files\Common Files\VST3` and FAILS without admin in the
  VM. This is a known environment gotcha — if a build ends with a
  VST3-copy permission error, this is the cause; do not "debug" it.
- C++17 (`CMAKE_CXX_STANDARD 17`, required, no extensions).
- Warnings: `/W4`; new warnings introduced by ported code = phase fail.

## 5. Validation assets & rules

- `validation\ref\` — reference renders (e.g. white noise through
  original Rubber-Rhino filters/FX, generated ONCE on a machine with the
  original plugin). Committed to git. **Immutable**: no model may
  regenerate, edit, or replace reference files to make a test pass.
- `validation\` scripts (Python) compute residuals; thresholds live in
  the scripts and in CLAUDE.md — never loosen them to pass.
- Build outputs (`build\`, `*.vst3`, rendered test WAVs) are
  git-ignored. References are committed; outputs never.

## 6. Claude Code session rules

- `.claude\settings.json` pre-approves: cmake, ninja, ctest, python,
  git, pluginval, and the repo's own build/test scripts. If a needed
  command is not pre-approved, ask once — do not shell around the
  permission system.
- Branch per phase (`port/<phase>`), never commit to main (CLAUDE.md).
- Non-interactive discipline: no commands that prompt (git push with
  auth prompts, interactive installers). If interaction is required,
  stop and tell the user.
- Long builds: prefer incremental Ninja builds; do not delete `build\`
  to "fix" issues unless a configure-level change genuinely requires it.
- Model policy per CLAUDE.md: Sonnet 4.6 default, escalate to
  Opus 4.8 / Fable 5 with failing-test evidence attached.

## 7. Pre-flight check (run at the start of the FIRST session)

Verify and report each item before starting phase 1:

    cmake --version          (>= 3.22)
    ninja --version
    cl                       (MSVC reachable)
    python -c "import numpy, scipy; print('py ok')"
    pluginval --version
    git -C C:\src\rubber-rhino log -1 --oneline   (donor present)
    dir C:\src\the-dreamer\dsp\bank\RomplerBank.h (bank present)
    dir C:\src\JUCE\CMakeLists.txt                (JUCE present)

Report the results as a table, flag anything missing, and STOP if a
REQUIRED item fails. The user will fix the environment; do not attempt
to install system-level tooling yourself unless explicitly asked.
