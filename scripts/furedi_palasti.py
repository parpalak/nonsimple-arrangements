#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""The Füredi-Palásti arrangement used in lem:perturbation.

B_m = {L(2 pi i / m)}, where L(a) is the line through the points P(a) and
P(pi - 2a) of the unit circle, or the tangent at P(a) if they coincide
[Füredi, Palásti 1984, Example 2]. Checks

   p_3(B_m) = m(m-3)/3 + 6 - 2 eps/3

projective triangles, eps = 0 for m = 0 (mod 3) and 2 otherwise, and p_3(B_m) >= 2m.

The lines are lifted to great circles, the faces of the sphere are traced by
the rotation system, and each projective face is counted once.

Usage: python3 furedi_palasti.py [m_min m_max]   (default 6 14)
"""
import math
import sys
from collections import Counter, defaultdict


def cross(p, q):
    return (p[1] * q[2] - p[2] * q[1],
            p[2] * q[0] - p[0] * q[2],
            p[0] * q[1] - p[1] * q[0])


def dot(p, q):
    return sum(x * y for x, y in zip(p, q))


def unit(p):
    r = math.sqrt(dot(p, p))
    return tuple(x / r for x in p)


def sub(p, q):
    return tuple(x - y for x, y in zip(p, q))


def line(a):
    """Normal of the plane through the origin that cuts out L(a)."""
    b = math.pi - 2 * a
    if abs(math.remainder(a - b, 2 * math.pi)) < 1e-12:   # tangent
        return (math.cos(a), math.sin(a), -1.0)
    return cross((math.cos(a), math.sin(a), 1.0), (math.cos(b), math.sin(b), 1.0))


def faces(normals):
    normals = [unit(v) for v in normals]
    key = lambda v: tuple(round(x, 8) for x in v)
    verts, on = {}, defaultdict(set)
    for i in range(len(normals)):
        for j in range(i + 1, len(normals)):
            c = cross(normals[i], normals[j])
            assert math.sqrt(dot(c, c)) > 1e-9, "equal lines"
            c = unit(c)
            for v in (c, tuple(-x for x in c)):
                k = key(v)
                verts.setdefault(k, v)
                on[k] |= {i, j}

    # neighbors along each great circle
    around = defaultdict(list)
    for i, normal in enumerate(normals):
        mine = [k for k in on if i in on[k]]
        e1 = verts[mine[0]]
        e2 = cross(normal, e1)
        pts = sorted((math.atan2(dot(verts[k], e2), dot(verts[k], e1)), k) for k in mine)
        for t in range(len(pts)):
            u, w = pts[t][1], pts[(t + 1) % len(pts)][1]
            around[u].append(w)
            around[w].append(u)

    # cyclic order of the neighbors in the tangent plane
    order = {}
    for u, nb in around.items():
        v = verts[u]
        b1 = unit(cross(v, (0.3, 0.7, 0.1)))
        b2 = cross(v, b1)
        order[u] = sorted(nb, key=lambda w: math.atan2(dot(sub(verts[w], v), b2),
                                                       dot(sub(verts[w], v), b1)))

    seen, sizes = set(), Counter()
    for u in order:
        for w in order[u]:
            if (u, w) in seen:
                continue
            k, a, b = 0, u, w
            while (a, b) not in seen:
                seen.add((a, b))
                k += 1
                ring = order[b]
                a, b = b, ring[(ring.index(a) - 1) % len(ring)]
            sizes[k] += 1
    return {k: c // 2 for k, c in sizes.items()}


def main(argv):
    lo, hi = (int(argv[1]), int(argv[2])) if len(argv) > 2 else (6, 14)
    bad = 0
    for m in range(lo, hi + 1):
        p3 = faces([line(2 * math.pi * i / m) for i in range(m)]).get(3, 0)
        eps = 0 if m % 3 == 0 else 2
        formula = (m * (m - 3) + 18 - 2 * eps) // 3
        ok = p3 == formula and p3 >= 2 * m
        bad += not ok
        print(f"m={m:2d}  p3={p3:4d}  formula {formula:4d}  2m={2 * m:3d}  "
              f"{'OK' if ok else 'FAIL'}")
    print("all checks passed" if not bad else f"FAIL: {bad}")
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
