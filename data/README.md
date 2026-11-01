# `data/`

```
exhaustive/   exhaustive enumerations of optimal arrangements, their classes and symmetries (Section 3)
runs/         search runs: commands, logs, words (see its README)
augmented/    output of addline (Section 4)
series/       the series 18·2^t+2 and 30·2^t+2 in pseudolines (Section 5, see its README)
```

## Word format

```
[21.0s] K=25 *int=0 i=147071091) 0 1 0 2 3 2 1 0 2 4 5 4 3 2 4 6 …
```

Everything up to the first `)` is a preamble for reading and grep: `K` is the
number of triangles, `*int` and `**int` the numbers of triple and fourfold
points, `i` the search node count at output. The word follows: each letter is
a crossing at the given slot, with multiplicity marked by asterisks (`3`
simple, `3*` triple, `3**` fourfold).

## `exhaustive/`

| File | Contents |
|---|---|
| `{n}.words.txt` | all words with the maximum number of triangles, n = 6, 8, 10, 12; a copy of `runs/{n}-{K}/complete.words.txt` (for n = 12, `complete.pruning.words.txt`) |
| `11.words.txt` | empty: no arrangement of 11 pseudolines has 33 triangles (`runs/11-33/first-fixed.log`) |
| `{n}.uniq-p.txt`, `{n}.uniq-e.txt` | one word per P-class and per E-class |
| `{n}.uniq-p-sym.jsonl` | symmetries of each P-class |
| `10.cert68.txt`, `10.cert71.txt` | biquadratic final polynomials of lines 68 and 71 of `10.uniq-e.txt`, the two non-stretchable optima (Appendix B) |

Classes and symmetries:

```sh
B=../combinatorics/sorter/build
for n in 6 8 10 12; do
  $B/sorter -i exhaustive/$n.words.txt --uniq P -p pid eid ')' gens > exhaustive/$n.uniq-p.txt
  $B/sorter -i exhaustive/$n.words.txt --uniq E -p pid eid ')' gens > exhaustive/$n.uniq-e.txt
  $B/sym_detect -i exhaustive/$n.uniq-p.txt > exhaustive/$n.uniq-p-sym.jsonl
done
```

The field `reps` of an E-class counts $4n/|H_E|$ words, not $2n/|H_E|$ as in
[pseudoline-algorithms](https://github.com/parpalak/pseudoline-algorithms): for
even n the arrangement has no checkerboard coloring to halve it.

## `augmented/`

Output of [`addline`](../combinatorics/addline/); the line format is described
there. The inputs are P-class representatives from
[pseudoline-algorithms](https://github.com/parpalak/pseudoline-algorithms/tree/e80f7a2/data/exhaustive):

| File | Input |
|---|---|
| `{8,14,20}.txt` | 2-defective: `{7,13,19}.skip5.uniq-p.txt` |
| `{6,10,16,18}.txt`, `22.zip` | perfect: `{5,9,15,17,21}.uniq-p.txt` |
| `32-from-E31-2fyik3y6tmkib.txt` | `E31-2fyik3y6tmkib.txt`: the 32-line arrangement of Appendix B without line 5 |

```sh
P=https://raw.githubusercontent.com/parpalak/pseudoline-algorithms/e80f7a2b44e4bcdac3fafc78fbbfa3f43d2f1846/data/exhaustive
A=../combinatorics/addline/build/addline
curl -sO $P/19.skip5.uniq-p.txt
$A -n 19 -i 19.skip5.uniq-p.txt -q > augmented/20.txt
```

`{26,32,38,44}.txt` are examples obtained from
`two_defect` output; their inputs are not kept. Removing the added line gives
a perfect or 2-defective arrangement.

Augmentations that reach no target give no file:

```sh
$A -n 25 -i 25-b5.skip5.uniq-p.txt -q                            # pentagonal defect
$A -n 11 -i runs/11-32/complete.pruning.words.txt -target 38 -q  # optima for n=11
for p in "5 8" "9 26" "15 73" "17 94" "21 144" "23 173" "27 239"; do set -- $p
  $A -n $1 -i $1.uniq-p.txt -target $2 -first -q                 # perfect, bound + 1
done
```

`27.uniq-p.txt` is stored as `27.uniq-p.txt.zst`; unpack it with
`zstd -d --long=30`.
