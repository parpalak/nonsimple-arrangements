#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""The first doubling step, n = 6 (lem:first-step), in exact arithmetic.

At n = 6 the leading terms G_1 and G_2 tie, and only the n^-6 term breaks the
tie, so floats are not enough. All data lie in Q(sqrt3): Q3 below is
p + q sqrt3 over Fraction.

Checks, at t = 1 (n_t = 12):
  * m_min = |m(L_4)| = 2/5 (lem:uv);
  * the right wedges with a new line are "eq:wedges-first-step";
  * G_1 = G_2 = -1 - sqrt3, the other G_k are larger by at least 1, the values
    of H_1, H_2, F_1 - F_2 = (3 - sqrt3)/n^6 and F_2 < 0;
  * L_4 and M_2 meet at the rightmost point of both;
  * a_3(B) = 10 + n^2, r(B + R) = 5 + n/2, a_3(B + R) = a_3(B) + r;
  * V = L_2 L_3 is the only point of A_0 below R, and its neighbors on L_2
    and L_3 stay L_2 L_6 and Q after the step (lem:r-avoids).
"""
import sys
from fractions import Fraction as F

SQRT3 = 1.7320508075688772


class Q3:
    """p + q sqrt3 with rational p, q."""
    __slots__ = ('p', 'q')

    def __init__(self, p=0, q=0):
        self.p = F(p)
        self.q = F(q)

    def __add__(s, o):
        o = q3(o)
        return Q3(s.p + o.p, s.q + o.q)
    __radd__ = __add__

    def __neg__(s):
        return Q3(-s.p, -s.q)

    def __sub__(s, o):
        return s + (-q3(o))

    def __rsub__(s, o):
        return q3(o) + (-s)

    def __mul__(s, o):
        o = q3(o)
        return Q3(s.p * o.p + 3 * s.q * o.q, s.p * o.q + s.q * o.p)
    __rmul__ = __mul__

    def inv(s):
        d = s.p * s.p - 3 * s.q * s.q
        assert d != 0
        return Q3(s.p / d, -s.q / d)

    def __truediv__(s, o):
        return s * q3(o).inv()

    def __rtruediv__(s, o):
        return q3(o) * s.inv()

    def sign(s):
        p, q = s.p, s.q
        if q == 0:
            return (p > 0) - (p < 0)
        if p == 0:
            return (q > 0) - (q < 0)
        if p > 0 and q > 0:
            return 1
        if p < 0 and q < 0:
            return -1
        c = p * p - 3 * q * q
        return ((c > 0) - (c < 0)) if p > 0 else ((c < 0) - (c > 0))

    def __eq__(s, o):
        return (s - q3(o)).sign() == 0

    def __lt__(s, o):
        return (s - q3(o)).sign() < 0

    def __hash__(s):
        return hash((s.p, s.q))

    def __repr__(s):
        return f"{float(s.p) + SQRT3 * float(s.q):.12g}"

    def __format__(s, spec):
        return format(repr(s), spec)


def q3(x):
    return x if isinstance(x, Q3) else Q3(x, 0)


S3 = Q3(0, 1)
N = 6


def seed(t):
    """eq:base-lines, eq:params; returns (A_0, R, delta)."""
    nt = 6 * 2 ** t
    eps = Q3(F(1, 4 * nt))
    eta = 3 * nt ** 6 * S3
    delta = q3(2 * nt ** 6)
    u = -4 * eta / (2 * eta - 3 + eps * S3 * (2 * eta + 1))
    v = 4 * S3 / (S3 + 9 * eps)

    def sl(m, a):                      # y = m(x - a) as m x - y = m a
        return (q3(m), q3(-1), q3(m) * q3(a))

    A0 = {
        'Y0': (q3(0), q3(1), q3(0)),
        'L1': sl(Q3(F(-1, 2)), -S3),
        'L2': sl(-eta, -S3 / 3),
        'L3': sl(eta, S3 / 3),
        'L4': sl(Q3(F(2, 5)), S3),
        'L5': sl(u, -eps),
        'L6': sl(v, eps),
    }
    return A0, sl(1, delta), delta


# beta_j = -pi/2 + (j - 1/2) pi/6: -75, -45, -15, 15, 45, 75 degrees
TANB = {1: -(2 + S3), 2: q3(-1), 3: -(2 - S3), 4: 2 - S3, 5: q3(1), 6: 2 + S3}
SIN2B = {1: Q3(F(-1, 2)), 2: q3(-1), 3: Q3(F(-1, 2)),
         4: Q3(F(1, 2)), 5: q3(1), 6: Q3(F(1, 2))}


def add_step(A0):
    """M_1..M_6 by eq:new-lines; returns (M, m_min, the line with m_min)."""
    best = None
    for k, (m, _, _) in A0.items():
        if k == 'Y0':
            continue
        am = m if m.sign() >= 0 else -m
        if best is None or am < best[0]:
            best = (am, k)
    mmin, host = best
    M = {}
    for j in range(1, N + 1):
        mu = -(mmin / N ** 10) * (SIN2B[j] + 1 / (q3(N ** 6) * TANB[j]))
        M[f'M{j}'] = (mu, q3(-1), mu * TANB[j])
    return M, mmin, host


def meet(l1, l2):
    (a1, b1, c1), (a2, b2, c2) = l1, l2
    d = a1 * b2 - a2 * b1
    if d.sign() == 0:
        return None
    return ((c1 * b2 - c2 * b1) / d, (a1 * c2 - a2 * c1) / d)


def vertices_on(arr, name):
    """Points on the line, left to right; a triple point is one record."""
    pts = []
    for k in arr:
        if k == name:
            continue
        p = meet(arr[name], arr[k])
        if p is None:
            continue
        for rec in pts:
            if rec[0] == p[0] and rec[1] == p[1]:
                rec[2].append(k)
                break
        else:
            pts.append([p[0], p[1], [k]])
    out = []
    for rec in pts:                    # insertion sort with exact comparisons
        i = 0
        while i < len(out) and out[i][0] < rec[0]:
            i += 1
        out.insert(i, rec)
    return out


def triangles(arr):
    """Triangles: on each side the other two points differ and are adjacent (lem:seed-faces)."""
    names = list(arr)
    idx = {}
    for h in names:
        d = {}
        for k, rec in enumerate(vertices_on(arr, h)):
            for nm in rec[2]:
                d[nm] = k
        idx[h] = d
    out = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            for k in range(j + 1, len(names)):
                tri = (names[i], names[j], names[k])
                ok = True
                for h in range(3):
                    d = idx[tri[h]]
                    o1, o2 = tri[(h + 1) % 3], tri[(h + 2) % 3]
                    if o1 not in d or o2 not in d or abs(d[o1] - d[o2]) != 1:
                        ok = False
                        break
                if ok:
                    out.append(tri)
    return out


def right_wedges(arr, newnames):
    rm = {nm: vertices_on(arr, nm)[-1] for nm in arr}
    out = []
    names = list(arr)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = names[i], names[j]
            if b in rm[a][2] and a in rm[b][2] and (a in newnames or b in newnames):
                out.append((a, b, rm[a][0]))
    return out


def main():
    t = 1
    A0, R, delta = seed(t)
    M, mmin, host = add_step(A0)
    B = dict(A0)
    B.update(M)
    BR = dict(B)
    BR['R'] = R
    fails = []

    def want(cond, msg):
        print(f"  [{'OK' if cond else 'FAIL'}] {msg}")
        if not cond:
            fails.append(msg)

    want(host == 'L4' and mmin == Q3(F(2, 5)), f"m_min = |m({host})| = {mmin}")

    w = right_wedges(B, set(M))
    got = sorted(tuple(sorted((a, b))) for a, b, _ in w)
    exp = sorted([('M1', 'M3'), ('L4', 'M2'), ('M4', 'M5'), ('M6', 'Y0')])
    want(got == exp, f"right wedges: {got}")

    r_L4 = vertices_on(B, 'L4')[-1]
    r_M2 = vertices_on(B, 'M2')[-1]
    want(r_L4[2] == ['M2'] and r_M2[2] == ['L4'], "L4 M2 is the rightmost point on L4 and on M2")

    a4 = S3                            # eq:M-height with a_{n-2} = a_4
    G = {j: SIN2B[j] * (a4 - TANB[j]) for j in TANB}
    H = {j: (a4 - TANB[j]) / (q3(N ** 6) * TANB[j]) for j in TANB}
    Fj = {j: G[j] + H[j] for j in TANB}
    want(G[1] == G[2] == -1 - S3 and all(not (G[k] - G[2] < 1) for k in (3, 4, 5, 6)),
         "G_1 = G_2 = -1 - sqrt3, other G_k larger by at least 1")
    want(H[1] == -2 * (S3 - 1) / N ** 6 and H[2] == -(S3 + 1) / N ** 6, "H_1, H_2")
    want(Fj[1] - Fj[2] == (3 - S3) / N ** 6 and Fj[2] < 0, "F_1 - F_2 = (3 - sqrt3)/n^6, F_2 < 0")

    a3B = len(triangles(B))
    trBR = triangles(BR)
    rBR = len([x for x in trBR if 'R' in x])
    want(a3B == 46, f"a_3(B) = {a3B} = 10 + n^2")
    want(rBR == 8, f"r(B + R) = {rBR} = 5 + n/2")
    want(len(trBR) == a3B + rBR, f"a_3(B + R) = {len(trBR)} = a_3(B) + r")

    below = []
    ks = list(A0)
    for i in range(len(ks)):
        for j in range(i + 1, len(ks)):
            p = meet(A0[ks[i]], A0[ks[j]])
            if p and (p[1] - (p[0] - delta)).sign() <= 0:
                below.append((ks[i], ks[j], p))
    want(len(below) == 1 and {below[0][0], below[0][1]} == {'L2', 'L3'},
         "V = L2 L3 is the only point of A_0 below R")

    v2 = vertices_on(B, 'L2')
    v3 = vertices_on(B, 'L3')
    want(v2[-1][2] == ['L3'] and v2[-2][2] == ['L6'],
         "on L2: V is the rightmost point, next to L2 L6")
    want(v3[0][2] == ['L2'] and sorted(v3[1][2]) == ['L1', 'L5'],
         "on L3: V is the leftmost point, next to Q = L1 L3 L5")

    print("all checks passed" if not fails else f"FAIL: {len(fails)}")
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
