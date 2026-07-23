// test_td007_remap.cpp -- TD-007 global-filter-type remap, shared table
// (plugin/Td007Remap.h) used by BOTH decode paths: setStateInformation (DAW
// state, already remapped pre-fix) and applyParamMap (factory + user preset
// JSON, the path this fix adds). The helper is JUCE-free, so the table is
// verified standalone here; the applyParamMap call-site semantics (remap
// during the loop, neutralize cut/res AFTER the loop so stored cut/res
// entries cannot undo it) are mirrored by a synthetic param-map simulation.
//
// Sections:
//   [table]     exhaustive 0..13 against the documented TD-007 mapping,
//               plus <=3 pass-through and neutralize flag correctness.
//   [presetmap] synthetic user-preset param maps through a simulation of
//               applyParamMap's decode order:
//                 flt1_type=11            -> flt1_type==2 (Formant -> BP)
//                 flt1_type=8 (+cut/res) -> flt1_type==0, cut==1.0, res==0.0
//               including stored cut/res entries AFTER the type entry.
//
// Build (vcvars64, from C:\the-dreamer):
//   cl /nologo /std:c++17 /O2 /EHsc /D_USE_MATH_DEFINES /I.
//     tests\test_td007_remap.cpp /Fo:tests\bin\ /Fe:tests\bin\test_td007_remap.exe

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/Td007Remap.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { ++failures; std::printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

// ---- a minimal double-valued param store + applyParamMap decode simulation --
// Mirrors TheDreamerProcessor::applyParamMap for the ids this test cares
// about: choice ids (flt types) get the TD-007 remap; float ids pass through;
// neutralize overrides run AFTER the whole map has been applied.
using ParamMap = std::vector<std::pair<std::string, double>>;

static std::map<std::string, double> simulateApplyParamMap(const ParamMap& values)
{
    // defaults matching the APVTS layout (cut 1.0, res 0.0, type 0)
    std::map<std::string, double> store = {
        { "flt1_type", 0.0 }, { "flt1_cut", 1.0 }, { "flt1_res", 0.0 },
        { "flt2_type", 0.0 }, { "flt2_cut", 1.0 }, { "flt2_res", 0.0 },
    };
    bool neutralize[2] = { false, false };
    for (const auto& [id, value] : values)
    {
        if (store.find(id) == store.end()) continue;      // unknown id -> skip
        if (id == "flt1_type" || id == "flt2_type")
        {
            const int slot = (id == "flt2_type") ? 1 : 0;
            bool tr = false;
            const int idx = dreamer::td007::remapGlobalFilterType((int)value, tr);
            neutralize[slot] = neutralize[slot] || tr;
            store[id] = (double)idx;                      // choice index
        }
        else
            store[id] = value;                            // float pass-through
    }
    const char* cutIds[2] = { "flt1_cut", "flt2_cut" };
    const char* resIds[2] = { "flt1_res", "flt2_res" };
    for (int s = 0; s < 2; ++s)
        if (neutralize[s]) { store[cutIds[s]] = 1.0; store[resIds[s]] = 0.0; }
    return store;
}

int main()
{
    // ---- [table] exhaustive against the documented TD-007 mapping ----------
    std::printf("[table]\n");
    {
        // expected {newType, neutralize} for old 0..13
        const struct { int nu; bool tr; } exp[14] = {
            {0,false},{1,false},{2,false},{3,false},   // 0-3 keep
            {0,false},{0,false},{0,false},             // 4,5,6  -> LP24
            {0,true },{0,true },{0,true },             // 7,8,9  -> LP24 neutral
            {0,false},                                 // 10 N+LP -> LP24 (keep cut)
            {2,false},                                 // 11 Formant -> BP
            {0,true },                                 // 12 allpass -> LP24 neutral
            {0,false},                                 // 13 DreamPln -> LP24
        };
        bool ok = true;
        for (int old = 0; old < 14; ++old)
        {
            bool tr = false;
            const int nu = dreamer::td007::remapGlobalFilterType(old, tr);
            if (nu != exp[old].nu || tr != exp[old].tr)
            {
                ok = false;
                std::printf("  old=%d -> got {%d,%d} want {%d,%d}\n",
                            old, nu, (int)tr, exp[old].nu, (int)exp[old].tr);
            }
            if (old <= 3 && nu != old) ok = false;     // pass-through identity
        }
        std::printf("  14/14 table entries checked\n");
        CHECK(ok, "remap table matches the documented TD-007 mapping");
        // result always lands in the TD-007 choice-4 range, whatever comes in
        bool inRange = true;
        for (int old = 0; old < 64; ++old)
        {
            bool tr = false;
            const int nu = dreamer::td007::remapGlobalFilterType(old, tr);
            if (nu < 0 || nu > 3) inRange = false;
        }
        CHECK(inRange, "remapped index always in [0,3]");
    }

    // ---- [presetmap] synthetic user-preset maps through the decode order ---
    std::printf("[presetmap]\n");
    {
        // Task acceptance case 1: flt1_type=11 (Formant) -> BP (2)
        auto s1 = simulateApplyParamMap({ { "flt1_type", 11.0 } });
        std::printf("  flt1_type=11 -> type=%.0f cut=%.2f res=%.2f\n",
                    s1["flt1_type"], s1["flt1_cut"], s1["flt1_res"]);
        CHECK(s1["flt1_type"] == 2.0, "flt1_type 11 (Formant) -> 2 (BP)");
        CHECK(s1["flt1_cut"] == 1.0 && s1["flt1_res"] == 0.0,
              "Formant remap leaves cut/res at their (default) values");

        // Task acceptance case 2: flt1_type=8 (comb family) -> 0 + cut 1 res 0,
        // with the preset's OWN cut/res entries stored AFTER the type entry --
        // the neutralize override must still win (post-loop application).
        auto s2 = simulateApplyParamMap({
            { "flt1_type", 8.0 }, { "flt1_cut", 0.22 }, { "flt1_res", 0.85 } });
        std::printf("  flt1_type=8 (+cut .22/res .85) -> type=%.0f cut=%.2f res=%.2f\n",
                    s2["flt1_type"], s2["flt1_cut"], s2["flt1_res"]);
        CHECK(s2["flt1_type"] == 0.0, "flt1_type 8 -> 0 (LP24)");
        CHECK(s2["flt1_cut"] == 1.0, "neutralize forces cut -> 1.0 over stored 0.22");
        CHECK(s2["flt1_res"] == 0.0, "neutralize forces res -> 0.0 over stored 0.85");

        // Neutralize is per-slot: an exotic flt1 must not touch flt2's knobs.
        auto s3 = simulateApplyParamMap({
            { "flt1_type", 12.0 },
            { "flt2_type", 10.0 }, { "flt2_cut", 0.40 }, { "flt2_res", 0.30 } });
        CHECK(s3["flt1_type"] == 0.0 && s3["flt1_cut"] == 1.0 && s3["flt1_res"] == 0.0,
              "slot 1 allpass -> LP24 neutralized");
        CHECK(s3["flt2_type"] == 0.0, "flt2_type 10 (N+LP) -> 0 (LP24)");
        CHECK(s3["flt2_cut"] == 0.40 && s3["flt2_res"] == 0.30,
              "N+LP keeps the STORED cut/res (lowpass-ish, no neutralize)");

        // TD-007-native / factory-preset values are a strict no-op.
        auto s4 = simulateApplyParamMap({
            { "flt1_type", 3.0 }, { "flt1_cut", 0.66 }, { "flt1_res", 0.10 } });
        CHECK(s4["flt1_type"] == 3.0 && s4["flt1_cut"] == 0.66 && s4["flt1_res"] == 0.10,
              "values <= 3 pass through untouched (factory bank no-op)");
    }

    if (failures) { std::printf("%d FAILURE(S)\n", failures); return 1; }
    std::printf("ALL CHECKS PASSED\n");
    return 0;
}
