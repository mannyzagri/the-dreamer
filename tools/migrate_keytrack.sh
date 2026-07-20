#!/usr/bin/env bash
# migrate_keytrack.sh -- one-time D10 preset migration.
# tvf_kf changed from unipolar 0..1 to BIPOLAR -1..+1. applyPreset feeds the
# stored value straight in as the normalized 0..1 position, so to preserve each
# preset's AUDIBLE key-follow the old value must move into the positive half:
#   new = old*0.5 + 0.5    (old [0,1] -> new [0.5,1] == actual keytrack [0,+1])
# Only tvf_kf_[abcd] is touched; everything else is byte-identical.
set -euo pipefail
IN="${1:-plugin/presets.json}"
OUT="${2:-$IN}"
TMP="$(mktemp)"
awk '
{
  if ($0 ~ /^[ \t]*"tvf_kf_[abcd]":[ \t]*[-0-9.]+,?[ \t]*$/) {
    pre = $0
    sub(/[-0-9.]+,?[ \t]*$/, "", pre)
    rest = substr($0, length(pre)+1)
    comma = (rest ~ /,/) ? "," : ""
    gsub(/[, \t]/, "", rest)
    nv = (rest + 0) * 0.5 + 0.5
    printf "%s%.6g%s\n", pre, nv, comma
    n++
    next
  }
  print
}
END { printf "migrated %d tvf_kf values\n", n > "/dev/stderr" }
' "$IN" > "$TMP"
mv "$TMP" "$OUT"
