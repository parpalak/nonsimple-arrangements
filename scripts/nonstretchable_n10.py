#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""Non-stretchability of two optima for n = 10 (fig:n10-nonstretchable).

Exact check of the biquadratic final polynomials (Bokowski, Richter 1990) in
../data/exhaustive/10.cert68.txt and 10.cert71.txt for lines 68 and 71 of
../data/exhaustive/10.uniq-e.txt.

The chirotope is read from the word: the sign of [ijk], i < j < k, is the order
of the points j and k on line i (the lines of a word are ordered by slope);
line n is the line at infinity, and a zero bracket is a triple point. Each
certificate line is a three-term Grassmann-Plücker relation

    [abc][ade] - [abd][ace] + [abe][acd] = 0

with the conclusion that the signs of its terms force:
    eq  one term is zero, the other two have opposite signs  => |P| = |M|,
    gt  the term P is the only one of its sign                => |P| > |M|.
In the variables log|[ijk]| the sum of (P - M) with the integer weights must
vanish identically while the gt lines have positive weight: then the system is
infeasible and there is no realization. No LP is needed to check this.

The negative controls apply each certificate to another word, where it must
fail.

Usage: python3 nonstretchable_n10.py [WORDS LINE N CERT]
"""
import itertools
import os
import re
import sys
from collections import Counter


def parse(line):
    line = line.split(')', 1)[1] if ')' in line else line
    out = []
    for t in line.split():
        m = re.fullmatch(r'(\d+)(\**)', t)
        out.append((int(m.group(1)), 2 + len(m.group(2))))
    return out


def local_points(word, n):
    """at[i][j]: the index of the point with line j along line i."""
    slots = list(range(n))
    at = [dict() for _ in range(n)]
    cnt = [0] * n
    for p, k in word:
        grp = slots[p:p + k]
        for a in grp:
            for b in grp:
                if a != b:
                    at[a][b] = cnt[a]
            cnt[a] += 1
        slots[p:p + k] = grp[::-1]
    return at


def check(words_path, line, n, cert_path):
    at = local_points(parse(open(words_path).readlines()[line - 1]), n)

    def chi(t):
        i, j, k = sorted(t)
        if k == n:
            return 1 if j in at[i] else 0
        for L, a, b in ((i, j, k), (j, i, k), (k, i, j)):
            if a in at[L] and b in at[L]:
                return 0 if at[L][a] == at[L][b] else (1 if at[L][b] < at[L][a] else -1)
        return 0

    def sgn(t):
        inv = sum(1 for x, y in itertools.combinations(t, 2) if x > y)
        return chi(t) * (-1) ** inv

    def key(p):
        return tuple(sorted(tuple(sorted(t)) for t in p))

    total = Counter()
    gtsum = 0
    for l in open(cert_path):
        if l.startswith('#') or not l.strip():
            continue
        head, P, M = l.split('|')
        kind, mult, *gp = head.split()
        mult = int(mult)
        a, b, c, d, e = map(int, gp)
        P = key([tuple(map(int, t.split(','))) for t in P.split()])
        M = key([tuple(map(int, t.split(','))) for t in M.split()])
        terms = [(+1, ((a, b, c), (a, d, e))), (-1, ((a, b, d), (a, c, e))), (+1, ((a, b, e), (a, c, d)))]
        s = {key(p): o * sgn(p[0]) * sgn(p[1]) for o, p in terms}
        if not (P in s and M in s and P != M):
            return 'bad line: ' + l.strip()
        nz = [v for v in s.values() if v]
        if kind == 'eq':
            if not (len(nz) == 2 and s[P] * s[M] == -1):
                return 'eq fails: ' + l.strip()
        else:
            if not (mult > 0 and len(nz) == 3 and s[P] != 0 and s[M] == -s[P]
                    and list(s.values()).count(s[P]) == 1):
                return 'gt fails: ' + l.strip()
            gtsum += mult
        for t in P:
            total[tuple(sorted(t))] += mult
        for t in M:
            total[tuple(sorted(t))] -= mult
        for t in P + M:
            if chi(t) == 0:
                return 'zero bracket: ' + l.strip()
    residue = {t: v for t, v in total.items() if v}
    if residue or gtsum <= 0:
        return f'residue {residue}, gt weight {gtsum}'
    return None


def main():
    if len(sys.argv) == 5:
        err = check(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
        print('FAIL ' + err if err else 'certificate valid: not stretchable')
        sys.exit(1 if err else 0)
    here = os.path.dirname(os.path.abspath(__file__))
    ex = os.path.join(here, '..', 'data', 'exhaustive')
    words = os.path.join(ex, '10.uniq-e.txt')
    ok = True
    for line in (68, 71):
        err = check(words, line, 10, os.path.join(ex, f'10.cert{line}.txt'))
        print(f'line {line}: ' + ('FAIL ' + err if err else 'certificate valid, not stretchable'))
        ok = ok and err is None
    for line, cert in ((70, 68), (1, 71)):
        err = check(words, line, 10, os.path.join(ex, f'10.cert{cert}.txt'))
        print(f'control: cert{cert} on line {line}: ' + ('rejected' if err else 'FAIL: accepted'))
        ok = ok and err is not None
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
