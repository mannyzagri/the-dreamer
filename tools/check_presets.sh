#!/usr/bin/env bash
# check_presets.sh -- factory-preset / APVTS drift check (PRESETS.md validation).
#
# Confirms the union of every parameter id used across the 47 factory presets in
# plugin/presets.json is a SUBSET of the APVTS id set produced by
# plugin/Params.h createParameterLayout() (mirrored below). Any preset id not in
# the APVTS set is a preset/param drift bug (the processor also jasserts on it at
# construction). No JUCE build required -- pure text cross-check (Git Bash).
#
# Usage:  bash tools/check_presets.sh
set -euo pipefail
cd "$(dirname "$0")/.."

PRESETS_JSON="plugin/presets.json"

# ---- APVTS id set: mirror of plugin/Params.h createParameterLayout() ---------
# Regenerated 2026-07-23 (2.7.1): 57 per-tone stems x4 + 101 globals = 329 ids.
# The previous mirror was stale at 281 (missing the v2.2+/v2.3/v2.4 additions)
# and would FALSE-FAIL any preset using e.g. semi_a / loop_rate_b / g_octave.
pertone="wave on level oct semi fine start start_random velo pan shape shape_depth \
noise noise_color dir vint voicing dreamy_spread detune_voices detune_amount \
loop_mode hit_play hit_stretch hit_pitchtrim \
loop_rate loop_rate_sync loop_rate_beats loop_varispeed \
lfo1_rate lfo1_depth lfo1_sync lfo1_dest lfo1_shape lfo2_rate \
lfo2_depth lfo2_sync lfo2_dest lfo2_shape aux_dest aux_amt tvf_type tvf_cut \
tvf_res tvf_env tvf_kf tvf_a tvf_d tvf_s tvf_r tva_a tva_d tva_s tva_r aux_a \
aux_d aux_s aux_r"
global="master vec_phase vec_orbit_on vec_orbit_rate vec_orbit_shape \
vec_orbit_voice vec_penv_on vec_penv_start vec_penv_end vec_penv_time \
vec_penv_loop flt_route flt_bal flt1_type flt1_cut flt1_res flt1_env flt2_type \
flt2_cut flt2_res flt2_env flt2_morph lfo_rate lfo_shape lfo_sync \
lfo2_rate lfo2_shape lfo2_sync mtx1_src mtx1_dst mtx1_amt \
mtx2_src mtx2_dst mtx2_amt mtx3_src mtx3_dst mtx3_amt modfx_type modfx_rate \
modfx_depth modfx_mix modfx_on modfx_param modfx_p0 modfx_p1 modfx_p2 modfx_p3 modfx_pfocus \
dly_mode dly_time dly_fb dly_mix dly_on dly_sync dly_param dly_p0 dly_p1 dly_p2 dly_p3 \
dly_pfocus rev_type rev_size rev_damp rev_mix rev_on rev_param rev_p0 rev_p1 rev_p2 rev_p3 \
rev_pfocus lofi_on lofi_bits lofi_srate lofi_compand lofi_alias lofi_param lofi_pfocus \
width_on width width_haas width_bassmono talk_on talk_va talk_vb talk_morph talk_sens \
talk_param talk_pfocus fx_prepost \
g_env_a g_env_d g_env_s g_env_r g_cutoff g_res g_octave limiter_on \
drift interp engine"

apvts=$(mktemp); preset=$(mktemp); trap 'rm -f "$apvts" "$preset"' EXIT
for b in $pertone; do for sx in _a _b _c _d; do echo "$b$sx"; done; done  > "$apvts"
for g in $global; do echo "$g"; done                                     >> "$apvts"
sort -u "$apvts" -o "$apvts"

# ---- preset id union (skip the JSON envelope keys) ---------------------------
grep -oE '"[a-z][a-z0-9_]*": ' "$PRESETS_JSON" \
  | sed -E 's/": ?$//; s/^"//' \
  | grep -vE '^(format|version|note|count|name|category|bank|presets|params)$' \
  | sort -u > "$preset"

apvts_n=$(wc -l < "$apvts"); preset_n=$(wc -l < "$preset")
missing=$(comm -23 "$preset" "$apvts")

echo "presets:        $(grep -c '"name":' "$PRESETS_JSON")   (bank A $(grep -cE '"bank": *"A"' "$PRESETS_JSON"), bank B $(grep -cE '"bank": *"B"' "$PRESETS_JSON"))"
echo "APVTS ids:      $apvts_n"
echo "preset union:   $preset_n"
if [ -z "$missing" ]; then
  echo "unknown ids:    0  -- OK (preset union is a subset of the APVTS id set)"
  exit 0
else
  echo "unknown ids:    DRIFT -- the following preset ids are NOT in the APVTS:"
  echo "$missing" | sed 's/^/  - /'
  exit 1
fi
