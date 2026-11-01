# addline

Adds one pseudoline to an arrangement of odd `n` lines and prints the
arrangements of `n+1` lines that reach a target number of triangles. Both
augmentation templates, direct and collapse, are described in Section 4 of the
preprint.

## Build

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

The representation (words, O-matrices, the projective closure, identifiers)
comes from `../core`.

## Usage

```bash
./build/addline -n 13 -i ../testdata/core13.txt > 14.txt
```

The inputs of Section 4 are the P-class representatives of odd arrangements
from arXiv:2607.29236; `../testdata/core13.txt` holds one of them. Larger
inputs come straight from
[`two_defect`](https://github.com/parpalak/pseudoline-algorithms/tree/e80f7a2/search/two_defect),
keeping the optimal ones:

```bash
two_defect -n 31 -X 27 -defects b4a | grep "K=298" | ./build/addline -n 31 > 32.txt
```

- `-n N` — lines in the input, odd; required.
- `-i FILE` — read FILE instead of stdin.
- `-source both|direct|collapse` — which template to apply; `both` by default.
- `-target T` — triangles wanted; by default `⌊(n+1)(n−1)/3⌋ − ⌊(n+1)/6⌋`.
- `-first` — print only the first route per input instead of all of them.
- `-ceiling` — compute the upper bound only and print no words.
- `-anchor cut|infinity` — `cut`, the default, searches only the routes that
  the template needs; `infinity` searches all of them, which is slower and
  serves as a cross-check.
- `-q` — no progress line on stderr.

Output is in the `sorter` format. A sample line:

```
#1 K=54 *int=3 *ext=0 par=0 chart=13 src=direct did=D14-... eid=E14-... pid=P14-... ) 1 9 11 12 10* 9 ...
```

`#1` is the input line, `K` the number of triangles, `*int` and `*ext` the
multiple points off and on the line at infinity, `chart` the line sent to
infinity. The identifiers are described in `../core/IDENTIFIERS.md`. Different
routes give different arrangements, so all of them are printed, each in the
affine chart where it reaches the target.

`par > 0` means the chart has parallel lines: a triple point lies on the line
at infinity. The word then has `par` fewer generators than a reduced word, and
tools that expect every pair of lines to cross should skip such lines.

## Implementation

The search runs on the projective closure. The new line is a route through
the faces of the closure that crosses every line once. It passes either
through an edge or through a simple vertex, which then becomes a triple point;
existing triple points are not passed.

First, a dynamic program over the face graph bounds the number of triangles
from above, for every affine chart at once: it keeps a vector with one entry
per line of the closure, the entry for line `l` counting triangles finite in
the chart where `l` is at infinity. Then a depth-first search looks for exact
routes, pruned by this bound.

A route defines the projective O-matrix of the extension. The word is read
back from it in a chart that reaches the target. Each printed word is rebuilt
from scratch and its triangles are recounted by code not used in the search.
