# `data/runs/`

Runs of [`multenum`](../../search/) behind Table 2 of the preprint. A directory
`{n}-{K}` holds the runs for n pseudolines and target K. Files are named
`<scope>[.<flags>].<type>`:

| Part | Values |
|---|---|
| scope | `complete`: the whole tree; `first-fixed`: first letter at slot 0 (`--first-fixed`) |
| flags | `pruning`: `--pruning 4G` (Appendix C); `reduced`: `--reduced` (Section 3) |
| type | `words.txt`: stdout; `log`: stderr |

```sh
./multenum $n $K > complete.words.txt 2> complete.log
./multenum $n $K --first-fixed --pruning 4G --reduced \
    > first-fixed.pruning.reduced.words.txt 2> first-fixed.pruning.reduced.log
```

The last line of a log is the verdict:

```
DONE n=11 K>=33 zone=all flags=first-fixed: nodes=627129383709 words=0 time=20754s ...
```

`words=0` after `DONE` means that no arrangement reaches K. `INCOMPLETE`
instead of `DONE` marks a run stopped by a limit or a signal. Node counts of a
single process do not depend on the machine. With `--reduced` the output is
not an exhaustive enumeration, but it contains all words of maximum length.

## n = 14

32 processes with a shared state table, one subtree per prefix of length 8:

```sh
./multenum 14 55 --prefixes 8 --pruning 256M --reduced --quiet \
    | grep '^0 ' > first-fixed.reduced.chunks.txt
./run_shm.sh first-fixed.reduced.chunks.txt first-fixed.pruning.reduced.tsv 32 run 14 55 \
    --pruning 32G --reduced --stop-on-word
```

`first-fixed.pruning.reduced.tsv` lists each chunk with its exit code, node
count and word count; `first-fixed.pruning.reduced.log.zip` holds the stderr of
all workers. The node count of a parallel run is not deterministic.
