# `search/`

`multenum` decides whether some arrangement of n pseudolines with vertices of
any multiplicity has at least K triangles, and lists all such arrangements as
words (Section 3).

```sh
g++ -O3 -march=native -std=c++17 -o multenum mult_enum.cpp
./multenum 8 16 --quiet    # no words: K(8) < 16
./multenum 8 15 --quiet    # 32 words
```

Words go to stdout; the last line of stderr is the verdict. Both formats are
described in [`../data/README.md`](../data/README.md) and
[`../data/runs/README.md`](../data/runs/README.md).

## Options

```
usage: multenum <n> <K> [options]
  zone:     --max-mult M  --require-mult M
  mode:     --first  --quiet  --max-nodes N  --stop-on-word
  scope:    --first-fixed   first letter (0,m) only (Sec. 3, first letter)
  reduce:   --reduced   Roudneff normalization: verdict/first witness only
  speed-up: --pruning [memory]   everything beyond Sec. 3 (App. C): 1G, 256M
            --pruning-shm NAME   share that table via /dev/shm/NAME,
                                 or /path/on/hugetlbfs/NAME for huge pages
  chunking: --prefixes D  --chunk "word"  --chunks FILE  --log FILE
  checks:   --no-forced-descent --no-doom
            --no-segments --no-table --no-pair-doom --table-equal --ledger
```

Without options the program runs the algorithm of Section 3. `--pruning` adds
the prunings of Appendix C and does not change the output. With `--reduced`
the output is not an exhaustive enumeration, but it contains all words of
maximum length. The `checks` options disable single prunings; the output must stay
the same. `--ledger` adds u, 2d_3 and d_4 to each word; `lemma-violations` in
the verdict must be `0/0/0`. For n ≥ 8 targets with n(n−2) − 3K > 12 are
refused: the output would take gigabytes.

## Parallel runs

`run_shm.sh` runs one process per chunk over a state table shared through
`/dev/shm`:

```sh
./multenum 14 55 --prefixes 5 --pruning 256M --quiet > chunks.txt
./run_shm.sh chunks.txt run.log 16 run 14 55 --pruning 4G --stop-on-word
rm /dev/shm/run
```

Give the table about 256 MiB per worker. Rerunning with the same journal
resumes the run.
