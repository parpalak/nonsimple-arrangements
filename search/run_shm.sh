#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
# One process per chunk over a state table shared in /dev/shm.
#
#   usage: ./run_shm.sh CHUNKS LOG WORKERS SHM-NAME <multenum args...>
#   e.g.   ./run_shm.sh chunks.txt run1.log 16 run1 14 55 --pruning 4G --stop-on-word
#
# CHUNKS is the chunk list IN TRAVERSAL ORDER: that gives table locality, and the
# expensive chunks come first. LOG is the journal, one line per finished chunk
# (chunk<TAB>code<TAB>nodes<TAB>words). Re-running with the same journal resumes
# the run: chunks already done are skipped, and the segment keeps its warm table,
# which stays valid across a crash because only fully traversed subtrees are ever
# recorded. The segment is not removed on exit: rm /dev/shm/NAME when done.
#
# BIN picks another binary. drvfs has no ETXTBSY, so rebuilding in place would
# swap the binary under running workers: give a new build its own name, segment
# and journal.
set -u
if [ $# -lt 5 ]; then sed -n '4,18p' "$0" >&2; exit 1; fi
chunks=$1; log=$2; procs=$3; shm=$4; shift 4
bin=${BIN:-$(cd "$(dirname "$0")" && pwd)/multenum}
case $bin in /*) ;; *) bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin") ;; esac
[ -x "$bin" ] || { echo "no $bin: g++ -O3 -march=native -std=c++17 -o multenum mult_enum.cpp" >&2; exit 1; }
todo=$(mktemp)
# Only code 0 counts as done: an interrupted worker also writes a line (code 2,
# INCOMPLETE), and skipping it would lose its subtree. Chunks that workers of
# this segment are traversing right now are skipped too: the dispatcher may have
# been killed while its workers go on.
skip=$(mktemp); trap 'rm -f "$todo" "$skip"' EXIT
if [ -s "$log" ]; then awk -F'\t' '$2 == 0 {print $1}' "$log"; fi > "$skip"
ps -eo args | sed -n "s|^$bin .*--chunk \\(.*\\) --pruning-shm $shm .*|\\1|p" > "$todo.inflight"
inflight=$(wc -l < "$todo.inflight"); cat "$todo.inflight" >> "$skip"; rm -f "$todo.inflight"
if [ -s "$skip" ]; then sort -u "$skip" | grep -Fxv -f - "$chunks" > "$todo" || true
else cp "$chunks" "$todo"; fi
echo "chunks left $(wc -l < "$todo") of $(wc -l < "$chunks"); workers $procs; segment /dev/shm/$shm${inflight:+; skipping $inflight chunk(s) already in flight}" >&2
xargs -P "$procs" -a "$todo" -d '\n' -I{} \
    "$bin" "$@" --chunk "{}" --pruning-shm "$shm" --log "$log" --quiet
code=$?
case $code in
  0)   echo "dispatch finished, every chunk traversed" >&2 ;;
  124) # xargs stopped dispatching, but running workers would finish their chunks
       pkill -f -- "--pruning-shm $shm" 2>/dev/null
       echo "STOP: a worker returned 255 - a word, or a segment-count lemma violated" >&2 ;;
  *)   echo "xargs returned $code: some chunks were not traversed, see the journal $log" >&2 ;;
esac
exit $code
