#pragma once
// Td007Remap.h -- TD-007 global-filter-type migration table (single source of
// truth). flt1_type/flt2_type shrank choice-14 -> choice-4, so any STORED
// index > 3 can ONLY come from a pre-TD-007 file (DAW state or preset JSON) --
// the remap is safe unconditionally, no version tag needed. Mapping:
//   0-3            keep                      (same first four types)
//   4,5,6,13       -> 0 LP24                 (Rhino LPs / DreamPln)
//   10 N+LP        -> 0 LP24                 (lowpass-ish, keep cut)
//   11 Formant     -> 2 BP                   (bandpass-ish)
//   7,8,9,12       -> 0 LP24 + cut=1.0 res=0 (notch/comb/allpass:
//                     near-transparent; LP24 at the STORED cutoff
//                     would gut the top end)
// Used by BOTH decode paths: setStateInformation (DAW state) and
// applyParamMap (factory + user preset JSON). JUCE-free on purpose so the
// table is standalone-testable (tests/test_td007_remap.cpp). C++17.

namespace dreamer {
namespace td007 {

// Returns the TD-007 choice-4 index for a stored global filter type index.
// Sets neutralize=true when the old type (notch/comb/allpass family) has no
// tonal analogue: the caller must ALSO force that slot's cut -> 1.0 and
// res -> 0.0 (near-transparent LP24), overriding any stored cut/res.
inline int remapGlobalFilterType(int oldType, bool& neutralize) noexcept
{
    neutralize = false;
    if (oldType <= 3)
        return oldType;                       // TD-007-native: keep
    switch (oldType) {
        case 11:                              // Formant -> BP (bandpass-ish)
            return 2;
        case 7: case 8: case 9: case 12:      // notch/comb/allpass family
            neutralize = true;
            return 0;                         // LP24, neutralized cut/res
        default:                              // 4,5,6,10,13 -> LP24
            return 0;
    }
}

} // namespace td007
} // namespace dreamer
