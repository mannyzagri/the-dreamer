#!/usr/bin/env bash
# migrate_env_times.sh -- one-time D1 preset migration.
# The envelope A/D/R time law changed from cubic (0.001 + 8*v^3, 1ms..8s) to
# exponential (0.001*10^(4v), 1ms..10s). Remap the stored normalized value of
# every env-time param ID so the AUDIBLE SECONDS each preset produces are
# preserved. Only IDs matching (tvf|tva|aux)_[adr]_[abcd] are touched (attack/
# decay/release of TVF/TVA/AUX, per tone); sustain (_s_) and all other params
# are left byte-identical. P-ENV time law is unchanged -> not migrated.
#   newv = log10( (0.001 + 8*old^3) / 0.001 ) / 4     (== lawv::envTimeInv)
set -euo pipefail
IN="${1:-plugin/presets.json}"
OUT="${2:-$IN}"
TMP="$(mktemp)"

awk '
function remap(v,   sec, r) {
  sec = 0.001 + 8.0*v*v*v
  r = log(sec/0.001)/log(10000.0)     # == log10(sec/0.001)/4
  if (r < 0) r = 0; if (r > 1) r = 1
  return r
}
{
  if ($0 ~ /^[ \t]*"(tvf|tva|aux)_[adr]_[abcd]":[ \t]*[-0-9.]+,?[ \t]*$/) {
    pre = $0
    sub(/[-0-9.]+,?[ \t]*$/, "", pre)          # text up to the value
    rest = substr($0, length(pre)+1)           # value (+ maybe comma)
    comma = (rest ~ /,/) ? "," : ""
    gsub(/[, \t]/, "", rest)
    nv = remap(rest + 0)
    printf "%s%.6g%s\n", pre, nv, comma
    n++
    next
  }
  print
}
END { printf "migrated %d env-time values\n", n > "/dev/stderr" }
' "$IN" > "$TMP"

mv "$TMP" "$OUT"
