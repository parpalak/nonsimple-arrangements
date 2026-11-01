# sorter

Streaming pipeline for postprocessing pseudoline-arrangement search
output: parse generator sequences, build O-matrices, deduplicate by
Euclidean or projective equivalence (`E`- and `P`-classes), and emit
canonical identifiers.

Two binaries:

| Binary       | Purpose                                                |
|--------------|--------------------------------------------------------|
| `sorter`     | Stream filter with `--uniq D` / `--uniq E` / `--uniq P` deduplication |
| `sym_detect` | Per-P-class symmetry analyzer, JSONL output            |

Both share `sorter_core`, a static library containing the O-matrix
implementation, exact compact keys, traced line-permutations, the
symmetry analyzer, and the persistence layer.

Input words may also contain multiple points: `g*` is a triple point, `g**` a
fourfold point, and so on.  Such words are read, keyed and analyzed through
`combinatorics_core` (`../core`); ordinary words keep their parsing and
identifier values.  For words with multiple points `--uniq` compares the
complete canonical byte keys; the printable 64-bit hashes are labels only.

## Build

```bash
cmake -B build
cmake --build build
```

Default is **Release**, built aggressively: `-O3 -DNDEBUG -march=native
-funroll-loops` plus link-time optimization (LTO/IPO). `-march=native` ties the
binary to the build host. For debug:

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

Do not benchmark Debug builds — they are 5–10× slower.

## Tests

```bash
cd build && ctest --output-on-failure
```

## `sorter`: streaming filter

### Modes

```bash
# Plain parsing + output formatting
sorter -i input.txt -p gens omatrix

# Filter unique by wiring diagram (commutation class of the raw word)
sorter -i input.txt --uniq D -p eid ')' gens

# Filter unique by E-class
sorter -i input.txt --uniq E -p eid ')' gens

# Multiple points: preserve stars and deduplicate by the generalized P-key
sorter -i starred.txt --uniq P -p pid eid did ')' gens

# Filter unique by P-class, with exact-cache acceleration
sorter -i input.txt --uniq P --exact-cache -p pid eid ')' gens

# Follow a growing file (like tail -f)
sorter -f -i growing.txt --uniq P --exact-cache -p pid ')' gens

# Multiple inputs; --follow applies only to the last
sorter -f -I a.txt b.txt c.txt --uniq P --exact-cache -p pid ')' gens

# Progress to stderr every 120 s (0 = off, the default)
sorter -i input.txt --uniq P --exact-cache --progress 120 -p pid ')' gens

# Checkpointing (EXPERIMENTAL, see note below): save state every
# 10 000 processed lines, also at end
sorter -i input.txt --uniq P --exact-cache \
       --save-state state.bin --state-progress 10000 \
       -p pid ')' gens

# Resume from a binary state file
sorter -i input.txt --uniq P --exact-cache --resume-state state.bin -p pid gens

# Resume from a previous output file
sorter -i input.txt --uniq P --exact-cache \
       --resume-output prev_output.txt --resume-skip-lines 500000 \
       -p pid ')' gens
```

> **Experimental, unverified.** The binary checkpoint path
> (`--save-state`, `--state-progress`, `--resume-state`) is not covered
> by any automated test, was not used to produce the published results,
> may contain bugs, and its on-disk format is not guaranteed stable.
> To resume an interrupted run, prefer replaying the previous text
> output with `--resume-output` (which needs only `--uniq P
> --exact-cache` and is independent of the binary state file).

### `--print` tokens

`-p` takes a sequence of formatting tokens. Known tokens (listed in
the `--help` output) emit specific fields; any other token is printed
literally, which is how `')'` is used as a visual separator.

| Token             | What it prints                                       |
|-------------------|------------------------------------------------------|
| `gens`            | Generator list (terminates the line)                 |
| `omatrix`         | Pretty-printed O-matrix                              |
| `n`, `k`, `A`     | Bracketed scalar fields (line count, parallels, Arnold difference) |
| `pos`             | `#<input line number>`                               |
| `did`             | D-class identifier `D<n>-<base36(hash)>`             |
| `eid`             | E-class identifier `E<n>-<base36(hash)>`             |
| `pid`             | P-class identifier `P<n>-<base36(hash)>`             |
| `perm`            | Resulting permutation on lines                       |
| `w0` / `w0_reason`| Reduced-word-for-w₀ validity status                  |
| `wire` / `wire_condensed` | ASCII wiring diagram (one row per crossing / commuting rows collapsed) |
| `lf`              | Insert a line break                                  |

`eid` and `pid` are the canonical class identifiers: the base36 FNV-1a64
hash of the lexicographically minimal byte buffer of the class orbit
(the E-class orbit for `eid`, the projective P-class orbit for `pid`),
matching Definition D.1 of arXiv:2607.29236.

For words with multiple points, plain `--uniq D`, `--uniq E`, and `--uniq P`
are supported.  `--exact-cache`, `--mirror`, and `--rotate` still use the old
simple-arrangement representation and therefore reject or skip such words
instead of silently resolving a multiple point into ordinary crossings.

### Progress and durability

- `--progress N` — every N seconds, one `[progress]` line per active
  file. `N` is a `double`; `0` (default) disables progress.
- `--state-progress N` — only valid with `--save-state`. Snapshot the
  state every N processed lines; it is always also flushed at the end
  of the run.

The progress line in exact-cache mode includes `hit_e`, the count of
cached E-classes actually observed so far. The ratio `hit_e /
covered_e` approaches 1 as the stream saturates — every cached E-key
has been encountered, the cheapest exhaustiveness signal in stream mode.

### Why `--exact-cache`

The default `--uniq P` re-canonicalizes every input line through the
full projective orbit. For saturated streams (millions of lines, most
landing in already-seen P-classes), the exact-cache mode short-circuits
the orbit computation for repeat E-classes. On-disk state format and
resume semantics: **[docs/EXACT_CACHE.md](docs/EXACT_CACHE.md)**.

## `sym_detect`: per-P-class symmetry analyzer

```bash
# Default: full P-symmetry group with reflections (T = 4n)
sym_detect -i input.uniq-p.txt > out.jsonl

# Oriented / chiral part only (T = 2n) — diagnostic; the group may not
# close (the `generators_close` check can come back false)
sym_detect -i input.uniq-p.txt --no-mirror > out.jsonl

# Add extra payload fields
sym_detect -i input.uniq-p.txt --include-full-perms --include-gens > out.jsonl
```

Output is JSONL, one record per input representative. Default top-level fields:
`n`; `pid` (P-class identifier); `gp_ord`, `gp_type`, `gp_gens`,
`gp_elt_orders` (the P-symmetry group's order, isomorphism-type name,
generators as line permutations, and element-order histogram);
`chart_to_e` (the local E-class each affine chart maps to); an array
`e_classes[]`; and a `checks` block of consistency assertions. Each
entry of `e_classes[]` carries `eid`, `m` (chart multiplicity), `reps`
(how many words the class is worth, see below), and the Euclidean stabilizer `he_ord`,
`he_type`, `he_gens`. The two opt-in flags above add fields:
`--include-gens` emits the source word as `gens`; `--include-full-perms`
emits the full element lists `gp_elts` and `he_elts`.

All records in one run must have the same `n`.  Simple full words use the
fixed-layout path; generalized words may have finite multiple points or
parallel classes. Invalid lines are reported to stderr as `[analyzer] warning
skipped_invalid_input` and skipped.

Multiple-point words use the generalized analyzer.  It enumerates every
realizable order of a parallel block created by a projective chart and traces
the resulting line relabeling.  Before computing the profile it selects the
canonical affine representative that realizes the P-key; consequently two
different words of one P-class produce the same `chart_to_e`, stabilizers, and
group generators.  Simple full words retain the original fixed-buffer
analyzer and its output unchanged.

For such records, `gp_gens`, `gp_elts`, and `chart_to_e` use the line labels
of that canonical representative.  With `--include-gens`, the `gens` field is
still the original input word and is included as provenance, not as the
coordinate system of the reported permutations.

A chart whose line at infinity runs through a multiple point has parallel
lines, and then its Euclidean orbit is shorter than the usual `T = 4n`: the
`k` lines of a parallel class share one direction, so `m = n - sum (k_i - 1)`
directions give `2m` rotations and `4m` images with reflections. The rotations
in between are no chart of that arrangement at all, and `he_ord` for such an
E-class counts matches inside the shorter orbit.

### `reps`

`reps` is the number of commutation classes of words the E-class is worth:
its orbit under the group acting on the chart divided by `|H_E|`.  The orbit
is `4n` for a full chart and `4m` where parallel lines leave only `m`
directions, half of that with `--no-mirror`, whose group is the rotations
alone.

For odd `n` the count is halved once more, `2n/|H_E|` for a full chart, as in
Lemma 5.13 of arXiv:2607.29236: the search keeps one word of every mirror
pair.  At even `n` the mirror preserves every index parity, there is no such
pairing, and `reps` counts the words themselves.

For generalized configurations the kernel of the action on line labels can
be nontrivial: `rho^n` may fix an O-matrix whose rows contain symmetric point
blocks.  In that case `he_ord` remains the order of the stabilizer in
`D_{2n}`, while the additional `he_img_ord` and `he_kernel_ord` fields give
the order of its permutation image and the kernel.  These fields are omitted
when the kernel is trivial, preserving the old JSON format for simple words.

### The `checks` block

Each record carries a `checks` object: per-P-class self-consistency
assertions. They cross-validate the symmetry enumeration against the
orbit-stabilizer bookkeeping of Section 5.4 of arXiv:2607.29236; for simple
arrangements all are expected to be `true`.  When the halved orbit `2n` of an
odd-`n` arrangement is not a multiple of `|H_E|`, `reps` is `null` and the
checks carry `"representatives_applicable":false`.

| Key                   | Holds when                                                              |
|-----------------------|-------------------------------------------------------------------------|
| `sum_m_ok`            | the chart multiplicities `m(E)` sum to `n + 1`                          |
| `orbit_stabilizer_ok` | every E-class is consistent with orbit–stabilizer, using the line-permutation image of `H_E` |
| `representatives_ok`  | `rep(E)*|H_E|` recovers the orbit it was counted in (`4n`, `4m`, or their halves; see `reps` above) |
| `generators_close`    | the P-symmetry generators close to a group of order `gp_ord` with the matching element-order histogram |
| `e_generators_close`  | each Euclidean stabilizer's generators close likewise                   |
| `chart_to_e_ok`       | every `chart_to_e` index is in range                                    |

An internal seventh diagnostic, `kernel_collapse_free`, records whether the
rotation kernel acts freely on the O-matrix.  It is true for the simple case
covered by Lemma C.2 of arXiv:2607.29236; a false value is legitimate for generalized
point blocks and is not included in the JSON record.

### Aggregator

The JSONL stream is not directly readable. Use
[`sym_detect_aggregate.py`](../tools/sym_detect_aggregate.py) (sibling directory) to
group records by symmetry profile and print a table:

```bash
build/sym_detect -i input.uniq-p.txt 2>/dev/null \
  | python3 ../tools/sym_detect_aggregate.py

# With ETA based on input line count
build/sym_detect -i input.uniq-p.txt 2>/dev/null \
  | python3 ../tools/sym_detect_aggregate.py --expected "$(wc -l < input.uniq-p.txt)"

# Capture JSONL while watching the live table
build/sym_detect -i input.uniq-p.txt 2>/dev/null \
  | tee out.jsonl \
  | python3 ../tools/sym_detect_aggregate.py --expected "$(wc -l < input.uniq-p.txt)"
```

The aggregator runs in alternate screen buffer when stdout is a TTY;
on Ctrl+C it restores the screen and prints a final table. Postfactum
analysis on a captured JSONL file works without piping:

```bash
python3 ../tools/sym_detect_aggregate.py < out.jsonl
```

Aggregation is incremental: every record updates a small profile map
(usually ≤ 10 buckets), so the work is linear in record count even at
tens of thousands of P-classes.

## Source layout

```
main.cpp           CLI parsing, argument validation, top-level dispatch.

params.hpp         Params struct shared between CLI and stream drivers.

stream_drivers.{cpp,hpp}
                   Three streaming entry points:
                   - stream_print_file       (no deduplication)
                   - stream_unique_file      (--uniq E / P, hash-based)
                   - stream_unique_file_exact_cache  (--uniq P --exact-cache)

state_io.{cpp,hpp} ExactCacheState struct + save/load to a binary file
                   (experimental --save-state/--resume-state path; see
                   docs/EXACT_CACHE.md), plus the [progress] formatter
                   that knows about it.

resume.{cpp,hpp}   Replay a previous output file to reconstruct
                   ExactCacheState (--resume-output).

configuration.{cpp,hpp}
                   Configuration struct: input gens, cached min_o /
                   min_p_o / perm / w0 validation.

print_config.{cpp,hpp}
                   --print token dispatch; --mirror / --rotate
                   post-deduplication transformations; StreamPrintOverrides
                   for ID short-circuit.

wire_print.{cpp,hpp}
                   ASCII wiring-diagram formatters (wire / wire_condensed)
                   and the O-matrix pretty-printer.

omatrix.{cpp,hpp}  OMatrix value type, get_OMatrix() builder,
                   rotation / mirror / min_o / min_projective_o.

fixed_omatrix.{cpp,hpp}
                   Flat-buffer view of a closed OMatrix (no parallels);
                   used in the exact-canonical hot loop.

exact_key.{cpp,hpp}
                   CompactKey, exact_min_o_key, get_projective_orbit_keys,
                   FixedLenExactKeyArena.

id_labels.{cpp,hpp}
                   base36 encoder and the E<n>-<hash> / P<n>-<hash>
                   identifier formatters.

traced_transform.{cpp,hpp}
                   OMatrix paired with the LinePermutation tracking
                   how line labels migrated under a transformation.

line_permutation_group.{cpp,hpp}
                   Group closure over LinePermutations: BFS from a
                   generator set, abelianness check, group-type label.

symmetry_analyzer.{cpp,hpp}
                   Per-P-class analysis: cut points, E-classes,
                   stabilizers, the full SymmetryAnalysisResult.

json_writer.{cpp,hpp}
                   JSONL emitter for SymmetryAnalysisResult.

generator_parser.{cpp,hpp}
                   Permissive parser: anything before the first `)` is
                   ignored, remaining whitespace-separated tokens are
                   parsed as integers; words with stars go through the
                   core word parser.

line_reader.{cpp,hpp}
                   ifstream wrapper with optional follow mode (tail -f).

progress.{cpp,hpp} ProgressTimer, ProgressRate, time-throttled
                   progress emission.

sym_detect.cpp     sym_detect CLI.

docs/
  EXACT_CACHE.md   Implementer reference for --exact-cache:
                   correctness model, on-disk state format, resume protocol.

tests/             ctest-driven unit tests for sorter_core modules.

third_party/
  argparse/        header-only CLI parser (used by sorter alone)
```
