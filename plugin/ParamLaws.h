// ParamLaws.h -- pure (JUCE-free) normalized<->real-unit parameter laws that
// must be SHARED between the APVTS layout (Params.h, for valueToText display)
// and the processor (PluginProcessor.cpp, for the DSP value). Keeping the law
// in one header guarantees the LCD string and the audible time always agree.
//
// D1 (UX_DSP_TASKS): envelope A/D/R times are now LOGARITHMIC and displayed in
// real units. The pre-D1 map was cubic `0.001 + 8*v^3` (1 ms .. 8 s); it is
// replaced by the exponential map below (1 ms .. 10 s). Stored preset values
// for the env-time IDs are migrated to preserve their authored SECONDS
// (tools/migrate_env_times.sh) -- IDs/ranges/count are unchanged, so no host
// re-scan, just reload.
//
// C++17, header-only, no dependencies beyond <cmath>.
#pragma once
#include <cmath>

namespace dreamer::lawv {

// Envelope attack/decay/release: normalized 0..1 -> seconds, exponential.
// v=0 -> 0.001 s (1 ms), v=1 -> 10 s. Perceptually even across the range.
inline double envTimeSec(double v) noexcept {
    if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
    return 0.001 * std::pow(10000.0, v);            // 0.001 * 10^(4v)
}
inline double envTimeInv(double sec) noexcept {     // seconds -> normalized
    if (sec < 0.001) sec = 0.001; else if (sec > 10.0) sec = 10.0;
    return std::log10(sec / 0.001) / 4.0;
}

// P-ENV (pitch-envelope) time: 0.02 .. 10 s exponential. The LAW is unchanged
// from pre-D1 (was `penvSec`); D1 only adds the real-unit display, so P-ENV
// preset values need NO migration.
inline double penvTimeSec(double v) noexcept {
    if (v < 0.0) v = 0.0; else if (v > 1.0) v = 1.0;
    return 0.02 * std::pow(500.0, v);
}
inline double penvTimeInv(double sec) noexcept {
    if (sec < 0.02) sec = 0.02; else if (sec > 10.0) sec = 10.0;
    return std::log(sec / 0.02) / std::log(500.0);
}

} // namespace dreamer::lawv
