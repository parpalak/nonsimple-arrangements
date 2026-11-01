# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin

# Helpers for checks.yml; sourced from search/ with ./multenum built there.

TMP="${TMP:-$(mktemp -d)}"

eq() {  # eq <what> <expected> <actual>
  if [ "$2" = "$3" ]; then
    printf 'OK   %-46s %s\n' "$1" "$3"
  else
    printf 'FAIL %-46s expected %s, got %s\n' "$1" "$2" "$3" >&2
    exit 1
  fi
}

nodes() {  # nodes <n> <target> [options...] -> "nodes words"
  local n=$1 t=$2; shift 2
  ./multenum "$n" "$t" --quiet "$@" 2>&1 >/dev/null \
    | sed -n 's/^DONE.*nodes=\([0-9]*\) words=\([0-9]*\).*/\1 \2/p'
}

# Drops the preamble: it varies between runs.
strip() { sed 's/^[^)]*) //' | sort; }

words() {  # words <n> <target> [options...] -> sorted words
  local n=$1 t=$2; shift 2
  ./multenum "$n" "$t" --quiet "$@" 2>/dev/null | strip
}
