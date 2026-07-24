// test_v18_remap.cpp -- v18 mod-matrix remap tables (plugin/V18Remap.h) +
// call-site semantics simulation. The helper is JUCE-free so the tables are
// verified standalone; the applyParamMap/setStateInformation call-site rules
// (remap during the loop, amt neutralize AFTER the loop so stored amt entries
// cannot undo it, pre-v18 marker detection, ovr force-true) are mirrored by a
// synthetic param-map simulation, exactly like tests/test_td007_remap.cpp.
//
// Sections:
//   [tables]              exhaustive MSRC 0..6 and MDST 0..9 against the
//                         documented v18 mapping + range clamps.
//   [marker]              isPreV18 = presence of a "vec_phase" entry.
//   [neutralize-after-loop] a Vec Phs slot forces its amt to center-zero even
//                         when the stored amt entry comes AFTER the src/dst.
//   [ovr-force]           pre-v18 maps force all 12 *_ovr_[t] TRUE; v18-native
//                         maps leave them alone.
//
// Build (vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I.
//     tests\test_v18_remap.cpp /Fo:tests\bin\ /Fe:tests\bin\test_v18_remap.exe

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/V18Remap.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

// ---- minimal param store + the v18 decode simulation ------------------------
// Mirrors TheDreamerProcessor::applyParamMap for the ids this test cares
// about: mtx*_src/mtx*_dst choices get the v18 remap when the map is pre-v18
// (vec_phase present); floats pass through; the amt-neutralize + ovr-force
// overrides run AFTER the whole map has been applied.
using ParamMap = std::vector<std::pair<std::string, double>>;

static const char* kOvrIds[12] = {
    "amp_ovr_a", "amp_ovr_b", "amp_ovr_c", "amp_ovr_d",
    "flt_ovr_a", "flt_ovr_b", "flt_ovr_c", "flt_ovr_d",
    "aux_ovr_a", "aux_ovr_b", "aux_ovr_c", "aux_ovr_d",
};

static std::map<std::string, double> simulateApplyParamMap(const ParamMap& values)
{
    // defaults matching the v18 APVTS layout (amt center 0.0 real, ovr false)
    std::map<std::string, double> store;
    for (int i = 1; i <= 3; ++i) {
        store["mtx" + std::to_string(i) + "_src"] = 0.0;
        store["mtx" + std::to_string(i) + "_dst"] = 0.0;
        store["mtx" + std::to_string(i) + "_amt"] = 0.0;
    }
    for (const char* id : kOvrIds) store[id] = 0.0;

    // pre-v18 detection: presence of a vec_phase entry (V18Remap.h contract)
    bool preV18 = false;
    for (const auto& [id, value] : values)
        if (id == "vec_phase") { preV18 = true; break; }

    bool neutralizeAmt[3] = { false, false, false };
    for (const auto& [id, value] : values)
    {
        if (store.find(id) == store.end()) continue;    // unknown id -> skip
        if (preV18 && id.size() == 8 && id.rfind("mtx", 0) == 0)
        {
            const int slot = id[3] - '1';               // mtx1..mtx3
            if (slot >= 0 && slot < 3)
            {
                bool tr = false;
                if (id.substr(5) == "src") {
                    store[id] = (double)dreamer::v18::remapMtxSrc((int)value, tr);
                    neutralizeAmt[slot] = neutralizeAmt[slot] || tr;
                    continue;
                }
                if (id.substr(5) == "dst") {
                    store[id] = (double)dreamer::v18::remapMtxDst((int)value, tr);
                    neutralizeAmt[slot] = neutralizeAmt[slot] || tr;
                    continue;
                }
            }
        }
        store[id] = value;                              // pass-through
    }
    // AFTER the loop (Td007 idiom): amt neutralize to bipolar center-zero,
    // then the pre-v18 ovr force-true.
    for (int s = 0; s < 3; ++s)
        if (neutralizeAmt[s]) store["mtx" + std::to_string(s + 1) + "_amt"] = 0.0;
    if (preV18)
        for (const char* id : kOvrIds) store[id] = 1.0;
    return store;
}

int main()
{
    // ---- [tables] exhaustive against the documented v18 mapping ------------
    std::printf("[tables]\n");
    {
        // MSRC old(7) -> new(6): expected {newIdx, neutralize}
        const struct { int nu; bool tr; } srcExp[7] = {
            {0,false},           // 0 G-LFO 1
            {0,true },           // 1 Vec Phs (deleted)
            {1,false},           // 2 Aux
            {2,false},           // 3 Velo
            {3,false},           // 4 Wheel
            {4,false},           // 5 G-LFO 2
            {5,false},           // 6 G-Aux
        };
        bool ok = true;
        for (int old = 0; old < 7; ++old) {
            bool tr = false;
            const int nu = dreamer::v18::remapMtxSrc(old, tr);
            if (nu != srcExp[old].nu || tr != srcExp[old].tr) {
                ok = false;
                std::printf("  MSRC old=%d -> got {%d,%d} want {%d,%d}\n",
                            old, nu, (int)tr, srcExp[old].nu, (int)srcExp[old].tr);
            }
        }
        std::printf("  7/7 MSRC entries checked\n");
        CHECK(ok, "MSRC table matches the documented v18 mapping");

        // MDST old(10) -> new(9)
        const struct { int nu; bool tr; } dstExp[10] = {
            {0,false},{1,false},{2,false},{3,false},{4,false},   // Pitch..Shape keep
            {0,true },           // 5 Vec Phs (deleted)
            {5,false},           // 6 Pan
            {6,false},           // 7 Noise
            {7,false},           // 8 Fx Param
            {8,false},           // 9 Loop Rate
        };
        ok = true;
        for (int old = 0; old < 10; ++old) {
            bool tr = false;
            const int nu = dreamer::v18::remapMtxDst(old, tr);
            if (nu != dstExp[old].nu || tr != dstExp[old].tr) {
                ok = false;
                std::printf("  MDST old=%d -> got {%d,%d} want {%d,%d}\n",
                            old, nu, (int)tr, dstExp[old].nu, (int)dstExp[old].tr);
            }
        }
        std::printf("  10/10 MDST entries checked\n");
        CHECK(ok, "MDST table matches the documented v18 mapping");

        // range safety: anything (incl. corrupt) lands inside the new lists
        bool inRange = true;
        for (int old = -3; old < 64; ++old) {
            bool tr = false;
            const int s = dreamer::v18::remapMtxSrc(old, tr);
            const int d = dreamer::v18::remapMtxDst(old, tr);
            if (s < 0 || s > 5 || d < 0 || d > 8) inRange = false;
        }
        CHECK(inRange, "remapped src in [0,5], dst in [0,8] for any input");
    }

    // ---- [marker] pre-v18 detection ----------------------------------------
    std::printf("[marker]\n");
    {
        // a v18-native map (no vec_phase) must NOT remap: stored indices are
        // already in the new space.
        auto native = simulateApplyParamMap({
            { "mtx1_src", 5.0 },     // v18 G-Aux
            { "mtx1_dst", 8.0 },     // v18 Loop Rate
            { "mtx1_amt", 0.7 } });
        CHECK(native["mtx1_src"] == 5.0, "v18-native src passes through unremapped");
        CHECK(native["mtx1_dst"] == 8.0, "v18-native dst passes through unremapped");
        CHECK(native["mtx1_amt"] == 0.7, "v18-native amt untouched");

        // the same indices WITH a vec_phase marker are pre-v18 and remap.
        auto old = simulateApplyParamMap({
            { "vec_phase", 0.25 },   // marker (id itself is unknown -> skipped)
            { "mtx1_src", 5.0 },     // old G-LFO 2
            { "mtx1_dst", 8.0 },     // old Fx Param
            { "mtx1_amt", 0.7 } });
        CHECK(old["mtx1_src"] == 4.0, "pre-v18 src 5 (G-LFO 2) -> 4");
        CHECK(old["mtx1_dst"] == 7.0, "pre-v18 dst 8 (Fx Param) -> 7");
        CHECK(old["mtx1_amt"] == 0.7, "non-VecPhs slot keeps its amt");
        CHECK(old.find("vec_phase") == old.end(), "vec_phase itself is not stored");
    }

    // ---- [neutralize-after-loop] -------------------------------------------
    std::printf("[neutralize-after-loop]\n");
    {
        // Vec Phs SOURCE: slot 1 neutralized even though its amt entry comes
        // AFTER the src entry in the map.
        auto s1 = simulateApplyParamMap({
            { "vec_phase", 0.0 },
            { "mtx1_src", 1.0 },     // old Vec Phs source
            { "mtx1_dst", 6.0 },     // old Pan
            { "mtx1_amt", 0.9 } });  // stored amt AFTER -> must still lose
        std::printf("  VecPhs src slot -> src=%.0f dst=%.0f amt=%.2f\n",
                    s1["mtx1_src"], s1["mtx1_dst"], s1["mtx1_amt"]);
        CHECK(s1["mtx1_src"] == 0.0, "Vec Phs source -> 0 (G-LFO 1)");
        CHECK(s1["mtx1_dst"] == 5.0, "old Pan dest 6 -> 5");
        CHECK(s1["mtx1_amt"] == 0.0, "neutralize forces amt -> center-zero over stored 0.9");

        // Vec Phs DEST: same rule via the dst table.
        auto s2 = simulateApplyParamMap({
            { "vec_phase", 0.0 },
            { "mtx2_src", 4.0 },     // old Wheel
            { "mtx2_dst", 5.0 },     // old Vec Phs dest
            { "mtx2_amt", -0.6 } });
        CHECK(s2["mtx2_src"] == 3.0, "old Wheel src 4 -> 3");
        CHECK(s2["mtx2_dst"] == 0.0, "Vec Phs dest -> 0 (Pitch)");
        CHECK(s2["mtx2_amt"] == 0.0, "Vec Phs dest slot amt neutralized");

        // neutralize is per-slot: slot 3 untouched by slot 1's Vec Phs.
        auto s3 = simulateApplyParamMap({
            { "vec_phase", 0.0 },
            { "mtx1_src", 1.0 }, { "mtx1_amt", 0.5 },
            { "mtx3_src", 3.0 }, { "mtx3_dst", 9.0 }, { "mtx3_amt", 0.4 } });
        CHECK(s3["mtx1_amt"] == 0.0, "slot 1 (Vec Phs) neutralized");
        CHECK(s3["mtx3_src"] == 2.0 && s3["mtx3_dst"] == 8.0 && s3["mtx3_amt"] == 0.4,
              "slot 3 remaps Velo/LoopRate and keeps its amt");
    }

    // ---- [ovr-force] --------------------------------------------------------
    std::printf("[ovr-force]\n");
    {
        // pre-v18 map: all 12 *_ovr_[t] forced TRUE, even one stored false.
        auto old = simulateApplyParamMap({
            { "vec_phase", 0.1 },
            { "amp_ovr_b", 0.0 } });     // stored false must still end true
        bool allTrue = true;
        for (const char* id : kOvrIds) if (old[id] != 1.0) allTrue = false;
        CHECK(allTrue, "pre-v18 load forces all 12 *_ovr_[t] TRUE");

        // v18-native map: ovr values pass through / stay at default false.
        auto native = simulateApplyParamMap({ { "flt_ovr_c", 1.0 } });
        CHECK(native["flt_ovr_c"] == 1.0, "v18-native stored ovr true kept");
        CHECK(native["amp_ovr_a"] == 0.0, "v18-native leaves unstored ovr at default false");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
