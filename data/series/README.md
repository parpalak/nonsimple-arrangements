# `data/series/`

Pseudoline series 18·2^t+2 and 30·2^t+2 for t ≤ 3 (Section 5). Each word attains
the conjectured bound. Line format: see [`../README.md`](../README.md).

| File | N | Words | Triangles | Triple points |
|---|---|---|---|---|
| `20.words.txt` | 20 | 1 | 117 | 3 |
| `38.words.txt` | 38 | 22 | 450 | 3 |
| `74.words.txt` | 74 | 57 | 1764 | 3 |
| `146.words.txt` | 146 | 129 | 6984 | 3 |
| `32.words.txt` | 32 | 1 | 315 | 4 |
| `62.words.txt` | 62 | 42 | 1230 | 4 |
| `122.words.txt` | 122 | 101 | 4860 | 4 |
| `242.words.txt` | 242 | 221 | 19320 | 4 |

The starting words are the 20- and 32-line arrangements of Appendix B; the
first is line 408 of [`../augmented/20.txt`](../augmented/20.txt). Each next
file holds the doublings of the first word of the previous file that keep the
maximum number of triangles.

```sh
python3 ../../scripts/enumeration_faces.py 38 38.words.txt
python3 ../../scripts/conjectures.py 38.words.txt
```
