# Exact-cache state format and resume protocol

`README.md` (section *Why `--exact-cache`*) explains what
`sorter --uniq P --exact-cache` does and when to reach for it. This
document is the implementer reference for the parts that do not belong
in a usage overview: the correctness model, the input preconditions,
the binary state-file format produced by `--save-state`, and the
resume protocol.

One operational note not covered by the README: the acceleration buys
speed with memory. The cache holds one E-key per covered affine chart,
not one per P-class. For `n = 27` and ~56 600 P-classes, expect ~1.5
million covered E-keys at ~702 bytes each — roughly 1.2 GB of arena
plus indexing overhead.

## Correctness model

The cache is keyed on **exact compact byte keys**, not on 64-bit
hashes. Hash collisions cannot cause spurious deduplication. The 64-bit hash
is used only as a bucket index inside the arena's lookup table; final
equality is always a byte-wise compare. This matters because the
search programs of arXiv:2607.29236 produce arrangements whose
random hashes do collide at the scale of `n = 27`.

The user-facing `eid` / `pid` identifiers printed by `-p eid pid` are
`E<n>-<base36(hash)>` / `P<n>-<base36(hash)>`, where the hash is the
FNV-1a64 of the lexicographically minimal byte buffer of the orbit (the
canonical key, per Definition D.1 of arXiv:2607.29236). The exact-cache stream and
`sym_detect` compute this identically, so their outputs agree.

## Input requirements

Exact-cache mode is restricted to the production input shape:

- `n` odd;
- `gens.size() == n * (n - 1) / 2` (a full reduced word for the
  longest permutation w₀);
- every step has `lines[gen] < lines[gen + 1]` (each adjacent
  transposition introduces a new inversion);
- the resulting `OMatrix` has no parallels and every row has length
  `n - 1`.

The first valid `n` fixes the arena layout for the entire run. Later
lines with a different valid odd `n` are dropped with a warning:

    [stream] warning skipped_invalid_input file=... line=... reason="mixed n in exact-cache stream; expected n=27 got n=29"

Lines that fail any other precondition are dropped with a similar
warning and processing continues.

Supported `n` range: `1 <= n <= 255` (key bytes are `uint8_t`).
For larger `n`, the build would need a wider key element.

## CLI flags and restrictions

Example invocations live in `README.md`. The flags `--exact-cache`
enables:

| Flag                      | Meaning                                                 |
|---------------------------|---------------------------------------------------------|
| `--exact-cache`           | Required to enable everything below                     |
| `--save-state PATH`       | Write binary `ExactCacheState` to PATH at end of run    |
| `--state-progress N`      | Also write state every N processed lines                |
| `--resume-state PATH`     | Initialize state from a binary `--save-state` file      |
| `--resume-output PATH`    | Initialize state by replaying a previous output file    |
| `--resume-skip-lines N`   | Skip N lines of input before resuming                   |

Restrictions enforced at startup:

- `--exact-cache` requires `--uniq P`.
- `--exact-cache` is incompatible with `--rotate` (would invalidate
  cache lookups for already-emitted classes).
- `--mirror` is allowed (it is applied after deduplication, only to
  the output line).
- `--save-state` / `--resume-state` / `--resume-output` /
  `--resume-skip-lines` / `--state-progress` all require
  `--exact-cache`.
- `--resume-state` and `--resume-output` are mutually exclusive.
- `--resume-skip-lines` requires either `--resume-state` or
  `--resume-output`.
- `--line` is rejected together with any resume flag (ambiguous: is
  the line number before or after the skip?).

## Progress fields

With `--progress N` (seconds) the driver prints a `[progress]` line to
stderr roughly every N seconds:

    [progress] file=27-0-part1.txt file_lines=400000 total_lines=400000 p_unique=53804 covered_e=1234560 hit_e=1234560 interval_lines=100000 rate_lps=15234.5

| Field            | Meaning                                                |
|------------------|--------------------------------------------------------|
| `file_lines`     | Lines read from the current file                       |
| `total_lines`    | Lines processed across all input files                 |
| `p_unique`       | Distinct P-classes emitted so far (`seen_p` size)      |
| `covered_e`      | E-keys cached so far (`covered_e` size)                |
| `hit_e`          | How many of `covered_e` have actually been hit         |
| `interval_lines` | Lines since the previous progress line                 |
| `rate_lps`       | Lines per second over the same interval                |

`hit_e / covered_e` approaches 1 as the stream saturates — every cached
E-key has been encountered. This is the cheapest exhaustiveness
signal available in stream mode.

## State file format

> **Experimental, unverified.** Everything in this section and in
> *From a binary state file* below — the `--save-state`,
> `--state-progress`, and `--resume-state` flags — is not covered by
> any automated test, was not used to produce the published results,
> may contain bugs, and the on-disk format is not guaranteed stable
> across builds. For resuming an interrupted run, the supported path is
> the text replay via `--resume-output` (next section).

Magic / header:

    bytes  0..15   "SORTERSTATE00001"   (16 bytes, ASCII)
    bytes 16..23   u64 version = 1      (little-endian)
    bytes 24..31   u64 expected_n
    bytes 32..39   u64 total_lines      (number of input lines accounted for)

Followed by two arenas (`covered_e`, then `seen_p`), each laid out as:

    u64 n
    u64 key_len           (must equal n * (n - 1))
    u64 byte_size         (must be a multiple of key_len)
    <byte_size raw bytes> (key_len bytes per entry, packed contiguously)

Followed by the hit-bits payload for `covered_e`:

    u64 count             (must equal covered_e.size())
    <ceil(count / 8) bytes>  one bit per E-key, LSB-first

The file is written atomically: `<path>.tmp` is written, fsync is
omitted (use a filesystem that supports atomic `rename`), then
`rename(<tmp>, <path>)` replaces the previous state.

There is no backwards-compatible reader for older magic strings.
State files written by earlier builds with a different magic are not
loadable — rerun from scratch or replay the previous output via
`--resume-output`.

## Resume modes

There are two ways to continue an interrupted run.

### From a binary state file (experimental, unverified)

This path is experimental: untested, unused for the published results,
and the state format may change (see the note under *State file
format*). Prefer *From a previous output file* below.

If the previous run was launched with `--save-state PATH` (and
optionally `--state-progress N` for periodic checkpoints), the next
run loads it directly:

    sorter -i remaining.txt --uniq P --exact-cache \
           --resume-state state.bin \
           -p pid eid ')' gens

`--resume-skip-lines` is optional; if provided, it must match
`total_lines` in the state file. If omitted, it defaults to the state
file's `total_lines`.

### From a previous output file

If no state file was kept but the previous run's stdout was captured,
the same state can be reconstructed by replaying the output:

    sorter -i remaining.txt --uniq P --exact-cache \
           --resume-output prev_output.txt \
           --resume-skip-lines 500000 \
           -p pid eid ')' gens

Each line of `prev_output.txt` is parsed: anything before the first
`)` is ignored except for tokens matching `P<n>-<hash>` / `E<n>-<hash>`
(used as ID consistency checks); anything after `)` is parsed
as generators. The full projective orbit is computed once per output
line, all orbit E-keys go into `covered_e`, and the P-key goes into
`seen_p`. After replay the main input is opened, the first
`--resume-skip-lines` lines are skipped, and live processing resumes.

An ID mismatch (the recomputed `pid` differs from the
one in the output) is logged as a warning but does not stop resume —
it only flags that the resume file was likely produced by a
different build of the program. Same for `eid`.

### Multi-file input

When `--inputs file1 file2 file3 ...` is given, `--resume-skip-lines`
is the total line count across the concatenation in the order given.
If `file1` had 300 000 lines and `--resume-skip-lines 500000`, the
driver skips all of `file1`, the first 200 000 lines of `file2`, then
resumes on line 200 001 of `file2`.

`Configuration::line_number` (printed via `-p pos`) is the global
1-based index across the concatenation, so positions in resumed
output remain comparable to positions in the original run's output.

### Durability budget

The driver `fflush(stdout)` after every emitted line, so a clean
SIGTERM or sudden kill loses at most the partially-written last line
of stdout. Cache state in memory is only persisted to disk on
`--save-state` / `--state-progress` boundaries, so an unexpected
crash without `--save-state` will require replaying from the output
file via `--resume-output`.

## Tests

`test_exact_key` covers the exact-canonical functions and arenas,
including adversarial hash-collision tests (different compact keys
hashing to the same 64-bit value). The end-to-end "exact-cache output
equals naive `--uniq P` output" regression runs in CI (the
`Sorter --exact-cache matches no-cache (n=17)` step of the `pipeline`
workflow).

The text-replay resume (`--resume-output`, hence `resume.cpp`) is
covered by `test_resume`: `parse_resume_line` extraction plus
`load_exact_cache_resume_output` reconstructing the `ExactCacheState`
arenas from a previous output file (round-trip against direct orbit
insertion, deduplication, mixed-`n` rejection, malformed-line skip, missing
file).

Not tested at any level: the binary checkpoint path (`--save-state`,
`--state-progress`, `--resume-state`, hence `state_io.cpp`), which is
flagged experimental/unverified above.
