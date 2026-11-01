#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""Independent triangle count for words with points of any multiplicity.

The word is drawn as a wiring diagram with integer polyline wires; faces of
the resulting plane graph are traced by the rotation system. This shares
nothing with the incremental counters of search/mult_enum.cpp. The sides of
a face are counted by edges of the arrangement; a face is bounded if no ray
lies on its boundary.

Input: one word per line, optionally after a preamble ending with ')'; then K
from the preamble is compared with the count. Every word is also checked to
be complete and to have C(n-1,2) - sum_v C(m(v)-1,2) bounded faces
(eq:euler-F), which tests the drawing.

Words with par>0 in the preamble (addline charts with parallel lines) are
skipped; with --resolve-parallels each is checked as the two words where the
parallel pair crosses first or last.

Usage: python3 enumeration_faces.py <n> [file|-] [--resolve-parallels]
"""
import sys
from collections import defaultdict
from math import atan2


def parse_word(s):
    """Word -> list of (gap, multiplicity)."""
    out = []
    for tok in s.split():
        m = 2
        while tok.endswith('*'):
            tok = tok[:-1]
            m += 1
        out.append((int(tok), m))
    return out


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


def wire_paths(word, n):
    """Wire polylines with integer vertices and the crossings of each wire."""
    slots = list(range(n))
    pts = {w: [(-4, 2 * w)] for w in range(n)}
    events = {w: [] for w in range(n)}
    centers = []
    for idx, (p, m) in enumerate(word):
        assert 0 <= p and p + m <= n, ("bad gap", p, m, n)
        group = slots[p:p + m]
        yc = 2 * p + (m - 1)
        centers.append((2 * idx, yc))
        for j, w in enumerate(group):
            pts[w].append((2 * idx - 1, 2 * (p + j)))
            pts[w].append((2 * idx, yc))
            pts[w].append((2 * idx + 1, 2 * (p + m - 1 - j)))
            events[w].append(idx)
        slots[p:p + m] = group[::-1]
    for w in range(n):
        assert slots[n - 1 - w] == w, ("incomplete word", slots)
    K = len(word)
    for w in range(n):
        pts[w].append((2 * K + 2, 2 * (n - 1 - w)))
    return pts, events, centers


def build_planar(word, n):
    """Straight pieces (a, b, edge_id); edge_id is ('seg', wire, k) or ('ray', wire, side)."""
    pts, events, centers = wire_paths(word, n)
    prim = []

    def add_chain(chain, eid):
        clean = [chain[0]]
        for q in chain[1:]:
            if q != clean[-1]:
                clean.append(q)
        for a, b in zip(clean, clean[1:]):
            prim.append((a, b, eid))

    for w in range(n):
        path = pts[w]
        evs = events[w]
        assert evs, "wire without crossings"
        cut = [path.index(centers[e]) for e in evs]
        add_chain(path[:cut[0] + 1], ('ray', w, 'L'))
        for i in range(len(evs) - 1):
            add_chain(path[cut[i]:cut[i + 1] + 1], ('seg', w, i))
        add_chain(path[cut[-1]:], ('ray', w, 'R'))
    return prim


def trace_faces(prim):
    """Faces of the plane graph, each as the list of edge_ids along it."""
    adj = defaultdict(list)
    for a, b, eid in prim:
        adj[a].append((b, eid))
        adj[b].append((a, eid))
    rot = {}
    for v, nbrs in adj.items():
        rot[v] = sorted(nbrs, key=lambda t: atan2(t[0][1] - v[1],
                                                  t[0][0] - v[0]))
        angs = [atan2(u[1] - v[1], u[0] - v[0]) for u, _ in rot[v]]
        assert len(set(angs)) == len(angs), ("equal directions", v)
    idx_of = {v: {u: i for i, (u, _e) in enumerate(rot[v])} for v in rot}

    visited = set()
    faces = []
    for a, b, _eid in prim:
        for (u, v) in ((a, b), (b, a)):
            if (u, v) in visited:
                continue
            face = []
            cu, cv = u, v
            while (cu, cv) not in visited:
                visited.add((cu, cv))
                i = idx_of[cv][cu]
                face.append(rot[cv][i][1])
                j = (i - 1) % len(rot[cv])
                cu, cv = cv, rot[cv][j][0]
            faces.append(face)
    return faces


def analyze(word, n):
    prim = build_planar(word, n)
    faces = trace_faces(prim)
    sizes = []
    for f in faces:
        ids = set(f)
        if any(e[0] == 'ray' for e in ids):
            continue          # unbounded
        sizes.append(len(ids))
    return len(sizes), sum(1 for s in sizes if s == 3)


def main():
    args = [a for a in sys.argv[1:] if a != '--resolve-parallels']
    resolve = len(args) < len(sys.argv) - 1
    if not args:
        print(__doc__)
        return 1
    n = int(args[0])
    src = args[1] if len(args) > 1 else '-'
    lines = (sys.stdin if src == '-' else open(src)).read().splitlines()
    tot = bad = skipped = 0
    for ln in lines:
        if not ln.strip():
            continue
        head, _, tail = ln.rpartition(')')
        claim = dict(kv.split('=', 1) for kv in head.split() if '=' in kv)
        word = parse_word(tail)
        if claim.get('par', '0') != '0':
            if not resolve:
                skipped += 1
                continue
            words = resolve_parallels(word, n)
        else:
            words = [word]
        for word in words:
            tot += 1
            assert sum(m * (m - 1) // 2 for _g, m in word) == n * (n - 1) // 2, "not all pairs cross"
            F, a3 = analyze(word, n)
            euler = (n - 1) * (n - 2) // 2 - sum((m - 1) * (m - 2) // 2 for _g, m in word)
            errs = []
            if F != euler:
                errs.append("F=%d, eq:euler-F gives %d" % (F, euler))
            if 'K' in claim and int(claim['K']) != a3:
                errs.append("a3=%d, K=%s" % (a3, claim['K']))
            if errs:
                bad += 1
                print("ERROR %s: %s" % (tail.strip(), "; ".join(errs)))
    print("words: %d, mismatches: %d" % (tot, bad) + (", skipped with par>0: %d" % skipped if skipped else ""))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
