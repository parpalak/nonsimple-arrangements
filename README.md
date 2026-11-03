# Non-simple Arrangements of Lines and Pseudolines with Many Triangles

Code and data for the paper.

Preprint: [arXiv:2611.03096](https://arxiv.org/abs/2611.03096) ·
[Gallery of stretched arrangements](https://ud1.github.io/kobon-solutions/gallery/)

## Repository layout

| Directory | Contents | Preprint |
|---|---|---|
| [`search/`](search/) | Exhaustive search over arrangements with multiple points (`multenum`) | Section 3, Appendix C |
| [`combinatorics/`](combinatorics/) | Deduplication and symmetry analysis (`sorter`, `sym_detect`), augmentation (`addline`) | Sections 3, 4 |
| [`data/`](data/) | Exhaustive enumerations, run logs, augmented arrangements, series | Sections 3–5 |
| [`scripts/`](scripts/) | Independent checks in Python | Sections 2–5, Appendices A, B |
| [`preprint/`](preprint/) | LaTeX source | — |

The odd arrangements augmented in Section 4 come from
[pseudoline-algorithms](https://github.com/parpalak/pseudoline-algorithms/tree/e80f7a2);
see [`data/README.md`](data/README.md).

## Building

Requires a C++17 compiler, CMake and Python 3 (standard library only).

    g++ -O3 -march=native -std=c++17 -o search/multenum search/mult_enum.cpp
    cmake -B combinatorics/sorter/build -S combinatorics/sorter
    cmake --build combinatorics/sorter/build
    cmake -B combinatorics/addline/build -S combinatorics/addline
    cmake --build combinatorics/addline/build

## Checks

[`.github/workflows/checks.yml`](.github/workflows/checks.yml) runs the unit
tests, reproduces the exhaustive enumerations for n = 6, 8, 10 and runs every script in
[`scripts/`](scripts/).
