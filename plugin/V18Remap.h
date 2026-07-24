#pragma once
// V18Remap.h -- v18 mod-matrix source/dest migration tables (single source of
// truth, Td007Remap.h style). The v18 renovation DELETED the Dream Vector
// (vec_phase & friends), which removes one mod-matrix SOURCE (Vec Phs, old
// stored index 1 of 7) and one DEST (Vec Phs, old stored index 5 of 10):
//
//   MSRC old(7) -> new(6):  0 G-LFO1 -> 0;  1 Vec Phs -> 0 + NEUTRALIZE;
//                           2 Aux -> 1;  3 Velo -> 2;  4 Wheel -> 3;
//                           5 G-LFO2 -> 4;  6 G-Aux -> 5
//   MDST old(10) -> new(9): 0-4 (Pitch/Cut1/Cut2/Morph/Shape) keep;
//                           5 Vec Phs -> 0 + NEUTRALIZE;  6 Pan -> 5;
//                           7 Noise -> 6;  8 Fx Param -> 7;  9 Loop Rate -> 8
//
// NEUTRALIZE = the slot routed to/from the deleted Vec Phs: the caller must
// ALSO force that slot's mtx*_amt to the bipolar CENTER (real 0.0 =
// normalized 0.5), applied AFTER the whole map/tree loop (Td007 idiom) so a
// stored amt entry later in the source cannot undo it.
//
// isPreV18 detection: the PRESENCE of a "vec_phase" entry in the source
// (DAW state tree / preset param map) marks a pre-v18 file. Going forward,
// getStateInformation stamps paramsVersion="18" on the state root and
// saveUserPreset writes "version":18 (informational belt-and-braces; the
// vec_phase marker is the decision input). Pre-v18 sources ADDITIONALLY force
// all 12 per-tone *_ovr_[t] flags TRUE on load so old patches keep their
// per-tone envelopes instead of snapping to the new global tiers.
//
// Used by BOTH decode paths: setStateInformation (DAW state) and
// applyParamMap (factory + user preset JSON). JUCE-free on purpose so the
// tables are standalone-testable (tests/test_v18_remap.cpp). C++17.

namespace dreamer {
namespace v18 {

// Returns the v18 6-entry mtx*_src choice index for a stored pre-v18 7-entry
// index. Sets neutralize=true when the old source was Vec Phs (deleted): the
// caller must force that slot's mtx*_amt to center-zero AFTER the loop.
inline int remapMtxSrc(int oldIdx, bool& neutralize) noexcept
{
    neutralize = false;
    if (oldIdx <= 0) return 0;                    // G-LFO 1 keeps slot 0
    if (oldIdx == 1) { neutralize = true; return 0; }   // Vec Phs deleted
    if (oldIdx > 6) oldIdx = 6;                   // clamp corrupt input
    return oldIdx - 1;                            // 2..6 -> 1..5 (order kept)
}

// Returns the v18 9-entry mtx*_dst choice index for a stored pre-v18 10-entry
// index. Sets neutralize=true when the old dest was Vec Phs (deleted).
inline int remapMtxDst(int oldIdx, bool& neutralize) noexcept
{
    neutralize = false;
    if (oldIdx < 0) return 0;
    if (oldIdx <= 4) return oldIdx;               // Pitch..Shape keep
    if (oldIdx == 5) { neutralize = true; return 0; }   // Vec Phs deleted
    if (oldIdx > 9) oldIdx = 9;                   // clamp corrupt input
    return oldIdx - 1;                            // 6..9 -> 5..8 (order kept)
}

} // namespace v18
} // namespace dreamer
