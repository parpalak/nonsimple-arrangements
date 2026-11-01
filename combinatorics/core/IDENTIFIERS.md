# Class identifiers

`addline`, `sorter` and `sym_detect` print three identifiers: `did` for the
labelled chart, `eid` for its Euclidean class and `pid` for its projective
class. They are computed in `src/identifiers.*`. Classes are compared by the
full exact key; the 64-bit hash in the identifier is only a short label.

## Exact key of a chart

Within the block of each finite multiple point the labels are sorted; the
blocks stay in their order along the line. Let `L` be the rows written one
after another, one `uint8_t` per label. For a simple arrangement

```text
K = L
```

of length `n*(n-1)`. This is the key of arXiv:2607.29236, so identifiers of
simple arrangements are the same as there.

With multiple points, `L` is followed by

```text
ff 01 C
```

where `C` marks the block boundaries: for row `i` and position `j = 1..n-2`,
bit `q = i*(n-2) + (j-1)` is set when elements `j-1` and `j` belong to one
point. Bits are packed from the lowest; unused high bits of the last byte are
zero. Two consecutive triple points give `101`, one fivefold point of the same
length gives `111`.

If the chart has parallel lines, the labels missing from a row are sorted and
appended to it as the block of its point at infinity. Then

```text
K = L || ff 02 || P || C
```

where `L` again has length `n*(n-1)`, `P` holds `n` bytes, the number of lines
parallel to each line, and `C` covers both the finite blocks and the block at
infinity.

## Canonical forms

The D-key is the exact key of the chart itself. The E-key is the
lexicographic minimum over the `4n` Euclidean relabellings with the line at
infinity fixed. To transform a chart, its multiple points are resolved into a
simple arrangement, the simple O-matrix is transformed, and the same sets of
lines are gathered back into finite or infinite blocks.

The P-key is the minimum of the E-key over all `n+1` lines of the projective
closure. A line through a multiple point may also go to infinity: the other
lines of that point become a parallel class. All admissible orders inside such
a class are tried, so the result does not depend on the order in which the
shortened affine word was written.

## Label

```text
<D|E|P><n>-<base36(FNV1a64(K))>
```

FNV-1a with offset basis `0xcbf29ce484222325` and prime `0x100000001b3`,
modulo `2^64`.
