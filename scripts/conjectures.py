#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""The inequalities of Section 5 on the enumerations and augmentations.

For every word, independently of the search code, computes a3, the unused and
double bounded segments u and d, the number t_m of points of each multiplicity
and the waste w = n(n-2) - 3 a3 (eq:waste), and checks:

  sanity (must always hold; a failure is a counting error):
    K               a3 equals K from the preamble
    balance         w = u - d + sum_v m(v)(m(v)-2)  (eq:non-simple-segm)
    double          a double segment has an end of multiplicity >= 3
                    (lem:double-segment)
  Section 5 (even n; the last two only with multiplicities <= 3):
    conj-waste      w >= n/2 - 1  (eq:conj-waste)
    compensation    d <= 3 t3  (eq:compensation)
    compensation-1  d <= 3 t3 - 1 for t3 > 0

A letter (g, m) reverses the block of slots g..g+m-1. The face between slots
c-1 and c is the face of wall c; walls 0 and n are the unbounded faces above
and below. A wire segment in slot p lies between the faces of walls p and p+1,
which do not change until the next crossing of the wire.

The word is taken after the last ')' of a line. Words with par>0 (addline
charts with parallel lines) are skipped; with --resolve-parallels each is
checked as the two words where the parallel pair crosses first or last.
Without files: the complete enumerations data/runs/*/complete*.words.txt.
The exit code reports only the sanity checks.

Usage: python3 conjectures.py [--resolve-parallels] [file ...]
"""
import glob
import os
import sys
from collections import Counter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def parse(line):
    s = line.split(')')[-1].split()
    word = []
    for tok in s:
        m = 2 + tok.count('*')
        word.append((int(tok.rstrip('*')), m))
    return word


def resolve_parallels(word, n):
    """The two words where the one non-crossing pair crosses first or last.

    An addline word with par=1 has a triple point at infinity, so one pair of
    lines is parallel; parallel lines are adjacent at both ends.
    """
    slots = list(range(n))
    met = set()
    for g, m in word:
        blk = slots[g:g + m]
        met.update((min(a, b), max(a, b)) for i, a in enumerate(blk) for b in blk[i + 1:])
        slots[g:g + m] = blk[::-1]
    missing = [(a, b) for a in range(n) for b in range(a + 1, n) if (a, b) not in met]
    assert len(missing) == 1, "expected one parallel pair, got %d" % len(missing)
    a, b = missing[0]
    pa, pb = slots.index(a), slots.index(b)
    assert b - a == 1 and abs(pa - pb) == 1, "parallel lines not adjacent"
    return [[(a, 2)] + word, word + [(min(pa, pb), 2)]]


def analyse(word):
    n = max(g + m for g, m in word)
    perm = list(range(n))
    # faces: [opened by a letter, closed, inner vertices, wall]
    faces = [[False, False, 0, c] for c in range(n + 1)]
    cur = list(range(n + 1))              # the face of each wall
    seg_start = [None] * n                # the vertex where the wire segment starts
    vmult = []
    segments = []                         # (face, face, multiplicities of the ends)
    for g, m in word:
        blk = perm[g:g + m]
        if any(blk[i] > blk[i + 1] for i in range(m - 1)):
            raise ValueError('a pair crosses twice')
        v = len(vmult)
        vmult.append(m)
        for p in range(g, g + m):
            w = perm[p]
            if seg_start[w] is not None:
                segments.append((cur[p], cur[p + 1], vmult[seg_start[w]], m))
            seg_start[w] = v
        for c in range(g + 1, g + m):
            faces[cur[c]][1] = True
            faces.append([True, False, 0, c])
            cur[c] = len(faces) - 1
        faces[cur[g]][2] += 1
        faces[cur[g + m]][2] += 1
        perm[g:g + m] = blk[::-1]
    if perm != list(range(n))[::-1] or sum(m * (m - 1) // 2 for m in vmult) != n * (n - 1) // 2:
        raise ValueError('incomplete word')

    def tri(f):
        o, cl, k, _ = faces[f]
        return o and cl and k == 1

    a3 = sum(1 for f in range(len(faces)) if tri(f))
    u = d = 0
    double_bad = 0
    for f1, f2, m1, m2 in segments:
        use = tri(f1) + tri(f2)
        if use == 0:
            u += 1
        elif use == 2:
            d += 1
            if m1 < 3 and m2 < 3:
                double_bad += 1
    t = Counter(vmult)
    return dict(n=n, a3=a3, u=u, d=d, t=t, w=n * (n - 2) - 3 * a3,
                double_bad=double_bad)


def checks(r, K=None):
    """Name -> True/False/None (None: outside the scope of the claim)."""
    n, u, d, w, t = r['n'], r['u'], r['d'], r['w'], r['t']
    t3 = t[3]
    even = n % 2 == 0
    only3 = all(m <= 3 for m in t)
    star = sum(m * (m - 2) * k for m, k in t.items())
    return {
        'K': r['a3'] == K if K is not None else None,
        'balance': w == u - d + star,
        'double': r['double_bad'] == 0,
        'conj-waste': 2 * w >= n - 2 if even else None,
        'compensation': d <= 3 * t3 if only3 else None,
        'compensation-1': d <= 3 * t3 - 1 if only3 and t3 > 0 else None,
    }


SANITY = ('K', 'balance', 'double')


def main(argv):
    resolve = '--resolve-parallels' in argv
    paths = [a for a in argv if a != '--resolve-parallels']
    if not paths:
        paths = sorted(glob.glob(os.path.join(ROOT, 'data/runs/*/complete*.words.txt')))
        # complete.pruning only where the run without pruning is missing
        paths = [p for p in paths if not p.endswith('complete.pruning.words.txt')
                 or not os.path.exists(p.replace('.pruning', ''))]
    total = {}
    first_bad = {}
    for path in paths:
        stat = {}
        words = skipped = 0
        with open(path, encoding='utf-8') as fh:
            for line in fh:
                word = parse(line)
                if not word:
                    continue
                head = line.rpartition(')')[0]
                claim = dict(kv.split('=', 1) for kv in head.split() if '=' in kv)
                K = int(claim['K']) if 'K' in claim else None
                if claim.get('par', '0') != '0':
                    if not resolve:
                        skipped += 1
                        continue
                    variants = resolve_parallels(word, max(g + m for g, m in word))
                else:
                    variants = [word]
                for w in variants:
                    words += 1
                    for name, ok in checks(analyse(w), K).items():
                        s = stat.setdefault(name, [0, 0, 0])
                        s[0 if ok else 1 if ok is False else 2] += 1
                        if ok is False and name not in first_bad:
                            first_bad[name] = (os.path.relpath(path, ROOT), line.strip())
        rel = os.path.relpath(path, ROOT)
        print(f'{rel}: {words} words' + (f', skipped with par>0: {skipped}' if skipped else ''))
        for name, s in stat.items():
            t = total.setdefault(name, [0, 0, 0])
            for i in range(3):
                t[i] += s[i]
    print('\ntotal:')
    for name, s in total.items():
        mark = ' (sanity)' if name in SANITY else ''
        print(f'  {name:14s} hold {s[0]:7d}  fail {s[1]:6d}  out of scope {s[2]:6d}{mark}')
    for name, (path, line) in first_bad.items():
        print(f'\nfirst failure of {name}: {path}\n  {line[:300]}')
    return 1 if any(total.get(name, [0, 0])[1] for name in SANITY) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
