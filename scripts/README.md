# `scripts/`

Independent checks of the preprint. They share no code with the programs they
check (`../search/`, `../combinatorics/`) and recompute everything in a
different way. Run them from this directory.

## Section 2 and Appendix A: the series N_t = 6·2^t+2

| Script | Checks |
|---|---|
| `series_construction.py` | the main theorem of Section 2: builds the arrangements of the series from the formulas in high-precision arithmetic up to N = 50 and recounts a_3 and the two triple points; at every step exactly one R-triangle, R Y_0 L_{n−2}, meets the strip (W2) |
| `series_seed.py` | Appendix A: the orders of crossings on the lines of the starting arrangement, its fifteen triangles, the constants and formulas |
| `series_first_step.py` | the first doubling step, n = 6, in exact arithmetic: the right wedges, the tie G_1 = G_2 broken by the n^-6 term, a_3 and r after the step, and the point V below R (`lem:first-step`, `lem:r-avoids`) |

## Section 3: search for small n

| Script | Checks |
|---|---|
| `enumeration_faces.py` | the number of triangles of every word: builds the arrangement from polylines, traces the faces and checks the number of bounded faces against the Euler formula. Independent of the search counter; also applies to `../data/series/` |
| `furedi_palasti.py` | the number of triangles of the Füredi–Palásti arrangement used in the perturbation lemma |

## Appendix B: arrangements

| Script | Checks |
|---|---|
| `appendix_b_lines.py` | the rational line equations for n = 20, 32, 38, 74 (`tab:n20-collapse-lines`, `tab:n32-lines`, `tab:n38-lines`, `tab:n74-lines`): the orders of crossings on every line match the series word in `../data/series/`, the triple points lie on the reference line, the triangle counts are 117, 315, 450, 1764 |
| `nonstretchable_n10.py` | non-stretchability of the two optima for n = 10 (`fig:n10-nonstretchable`): an exact check of the biquadratic final polynomials `../data/exhaustive/10.cert68.txt`, `10.cert71.txt` (lines 68 and 71 of `10.uniq-e.txt`) and two negative controls; no LP needed |

## Section 5: conjectures

`conjectures.py` recomputes a_3, u, d and t_m on every word of the complete
enumerations `../data/runs/*/complete*.words.txt` and of `../data/augmented/`,
and prints how often the inequalities of Section 5 hold and fail.

addline words with par=1 have a pair of parallel lines. `conjectures.py` and
`enumeration_faces.py` skip them unless given `--resolve-parallels`; then each
is checked as the two arrangements where the pair crosses first or last.
