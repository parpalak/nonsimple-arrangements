#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""The series N_t = 6*2^t + 2 of Section 2, built from the formulas.

For t = 0..tmax builds the starting arrangement (eq:base-lines, eq:params),
applies t doubling steps (eq:new-lines) and checks:
  * every step adds n^2 triangles (lem:doubling);
  * every step keeps exactly one R-triangle meeting the strip "eq:strip", and it
    is R Y_0 L_{n-2} (W2);
  * all new intersection points lie on one side of R;
  * C_t has two triple points, A_t = n_t^2/3 - 2, r_t = n_t/2 + 2 and a_3 as in
    eq:a3-series.

Decimal arithmetic with 500 digits: the slopes shrink by n^10 per step, so
floats fail already at t = 2. A sign counts as zero below REL_ZERO; min|rel|,
the smallest nonzero relative residual, must stay far above it.

Usage: python3 series_construction.py [--tmax 3]   (t = 3, N = 50: ~5 s)
"""

import argparse
import sys
from decimal import Decimal as D, getcontext
from itertools import combinations

PREC = 500
REL_ZERO = D(10) ** (-150)
_EPS = None                  # series cutoff, set in set_precision()
PI = None


def set_precision(prec):
    global PREC, _EPS, PI
    PREC = prec
    getcontext().prec = prec
    _EPS = D(10) ** (-(prec - 30))
    PI = 16 * _atan_inv(5) - 4 * _atan_inv(239)


def _atan_inv(k):
    x = D(1) / D(k)
    x2, term, total, i = x * x, x, D(0), 0
    while abs(term) > _EPS:
        total += (term / (2 * i + 1)) * (1 if i % 2 == 0 else -1)
        term *= x2
        i += 1
    return total


def dsin(x):
    t, s, k, sign = x, D(0), 1, 1
    while abs(t) > _EPS:
        s += sign * t
        sign = -sign
        t = t * x * x / D((k + 1) * (k + 2))
        k += 2
    return s


def dcos(x):
    t, s, k, sign = D(1), D(0), 0, 1
    while abs(t) > _EPS:
        s += sign * t
        sign = -sign
        t = t * x * x / D((k + 1) * (k + 2))
        k += 2
    return s


def dtan(x):
    return dsin(x) / dcos(x)


# A line (m, a) is y = m (x - a); the construction has no vertical lines.

def inter(L1, L2):
    m1, a1 = L1
    m2, a2 = L2
    x = (m1 * a1 - m2 * a2) / (m1 - m2)
    return x, m1 * (x - a1)


def side(L, P):
    """Sign of L at P and the relative residual."""
    m, a = L
    x, y = P
    val = y - m * (x - a)
    scale = abs(y) + abs(m) * (abs(x) + abs(a)) + D(10) ** (-(PREC - 100))
    rel = abs(val) / scale
    if rel < REL_ZERO:
        return 0, rel
    return (1 if val > 0 else -1), rel


def analyse(lines):
    """Vertices, multiple points and triangles.

    Three lines bound a triangle if their three intersection points differ and
    no other line separates them. Unlike adjacency along the lines, this
    criterion needs no care at triple points.
    """
    N = len(lines)
    V = {(i, j): inter(lines[i], lines[j]) for i, j in combinations(range(N), 2)}
    sg, minrel = {}, D(1)
    for (i, j), P in V.items():
        for d in range(N):
            if d in (i, j):
                continue
            s, rel = side(lines[d], P)
            sg[(d, i, j)] = s
            if s != 0:
                minrel = min(minrel, rel)
    multi = sorted({tuple(sorted(k)) for k, v in sg.items() if v == 0})
    faces = []
    for tri in combinations(range(N), 3):
        if tri in multi:
            continue
        i, j, k = tri
        ok = True
        for d in range(N):
            if d in tri:
                continue
            ss = [sg[(d,) + p] for p in ((i, j), (i, k), (j, k))]
            if max(ss) > 0 and min(ss) < 0:
                ok = False
                break
        if ok:
            faces.append(tri)
    return {"V": V, "faces": faces, "multi": multi, "minrel": minrel}


def seed(t):
    """A_0 = {Y_0, L_1..L_6} and the line R (eq:base-lines, eq:params)."""
    nt = 6 * 2 ** t
    s3 = D(3).sqrt()
    eps = D(1) / (4 * nt)
    eta = 3 * s3 * D(nt) ** 6
    delta = 2 * D(nt) ** 6
    u = -4 * eta / (2 * eta - 3 + eps * s3 * (2 * eta + 1))
    v = 4 * s3 / (s3 + 9 * eps)
    A0 = [(D(0), D(0)),            # Y_0
          (-D(1) / 2, -s3),        # L_1
          (-eta, -1 / s3),         # L_2
          (eta, 1 / s3),           # L_3
          (D(2) / 5, s3),          # L_4
          (u, -eps),               # L_5
          (v, eps)]                # L_6
    names = ["Y0", "L1", "L2", "L3", "L4", "L5", "L6"]
    R = (D(1), delta)
    return A0, names, R, nt


def double(A, names, n, step):
    """Adds M_1..M_n (eq:new-lines). A = [Y_0, L_1, ..., L_n]."""
    m_min = min(abs(m) for m, _ in A[1:])
    new, new_names = [], []
    for j in range(1, n + 1):
        beta = -PI / 2 + (D(j) - D(1) / 2) * PI / n
        b = dtan(beta)
        mu = -(m_min / D(n) ** 10) * (dsin(2 * beta) + 1 / (D(n) ** 6 * b))
        new.append((mu, b))
        new_names.append("M%d^%d" % (j, step))
    return A + new, names + new_names


def run(t):
    A, names, R, nt = seed(t)
    print("\n=== t = %d (n_t = %d, N_t = %d) ===" % (t, nt, nt + 2))
    n, a3_prev = 6, None
    for s in range(t + 1):
        if s:
            A, names = double(A, names, n, s)
            n *= 2
        res = analyse(A)
        a3 = len(res["faces"])
        print("  A_%d (n=%d): a_3 = %d, min|rel| = %.2e" % (s, n, a3, res["minrel"]))
        if s:
            assert a3 - a3_prev == (n // 2) ** 2, "a_3 grew by %d" % (a3 - a3_prev)
        a3_prev = a3

        band = min(abs(m) for m, _ in A[1:]) / n
        resR = analyse(A + [R])
        all_names = names + ["R"]
        meets = []
        for f in resR["faces"]:
            if len(A) in f:
                ys = [resR["V"][p][1] for p in combinations(f, 2)]
                if min(ys) < band and max(ys) > -band:
                    meets.append("".join(all_names[q] for q in f))
        # L_{n-2} is L4 in A_0 and M_{n/2} of the previous step afterwards
        hub = "L4" if s == 0 else "M%d^%d" % (n // 2, s)
        print("    R-triangles meeting the strip: %s" % ", ".join(meets))
        assert meets == ["Y0" + hub + "R"], "expected only Y0 %s R" % hub
        signs = {side(R, resR["V"][(i, j)])[0]
                 for i, j in combinations(range(len(A)), 2)
                 if "M" in names[i] or "M" in names[j]}
        assert len(signs) <= 1, "new points on both sides of R"

    res = analyse(A + [R])
    N = n + 2
    a3 = len(res["faces"])
    r = sum(1 for f in res["faces"] if len(A) in f)
    print("  C_%d (N=%d): A_t = %d, r_t = %d, a_3 = %d" % (t, N, a3_prev, r, a3))
    assert sorted(len(p) for p in res["multi"]) == [3, 3], "not two triple points"
    assert a3_prev == n * n // 3 - 2 and r == n // 2 + 2, "A_t or r_t"
    assert 3 * a3 == N * N - 5 * N // 2 + 1, "eq:a3-series"


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tmax", type=int, default=3)
    ap.add_argument("--prec", type=int, default=500)
    args = ap.parse_args()
    set_precision(args.prec)
    try:
        for t in range(args.tmax + 1):
            run(t)
    except AssertionError as e:
        sys.exit("FAIL: %s" % e)
    print("\nall checks passed")
