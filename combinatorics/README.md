# Combinatorics tools

Three CMake projects, each built in its own `build/`:

- `core` — the representation: words with multiple points (`g*`, `g**`, ...),
  O-matrices, the projective closure, exact class identifiers. A library only.
- `sorter` — `sorter` and `sym_detect` from arXiv:2607.29236, extended to
  words with multiple points through `core`.
- `addline` — draws one more line through an odd seed (Section 4).

`sorter` and `addline` pull `core` in as a subdirectory; fixtures shared by
the tests are in `testdata`.

```bash
cmake -B addline/build -S addline && cmake --build addline/build
cmake -B sorter/build  -S sorter  && cmake --build sorter/build
ctest --test-dir addline/build --output-on-failure
ctest --test-dir sorter/build  --output-on-failure
```

The tests of `core` itself: `cmake -B core/build -S core && cmake --build
core/build && ctest --test-dir core/build`.
