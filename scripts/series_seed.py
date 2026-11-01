#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""Appendix A: the starting arrangement of the series N_t = 6*2^t + 2.

Checks the intersection orders eq:seed-orders, the fifteen triangles of
lem:seed (five of them with R) and the constants and formulas of the
subsections "Parameters" and "Coordinates of the intersection points".

sqrt(3) is a rational approximation to about 120 digits, so the triple points
P and Q split by ~1e-120; points closer than 1e-50 are merged. The real gaps
are 1/eta ~ 1e-5 or larger.
"""

import itertools
import sys
from decimal import Decimal, getcontext
from fractions import Fraction as F

getcontext().prec = 120
S3 = F(Decimal(3).sqrt())
TOL = F(1, 10) ** 50

NAMES = ['R', 'Y0', 'L1', 'L2', 'L3', 'L4', 'L5', 'L6']

TABLE = {
    'R':  ['L6', 'L2', 'L3', 'L5', 'L1', 'Y0', 'L4'],
    'Y0': ['L1', 'L2', 'L5', 'L6', 'L3', 'L4', 'R'],
    'L1': ['Y0', 'L2', 'L4/L6', 'L3/L5', 'R'],
    'L2': ['L5', 'Y0', 'L1', 'L4', 'L6', 'R', 'L3'],
    'L3': ['L2', 'R', 'L1/L5', 'L4', 'Y0', 'L6'],
    'L4': ['L2', 'L1/L6', 'L5', 'L3', 'Y0', 'R'],
    'L5': ['L2', 'Y0', 'L6', 'L4', 'L1/L3', 'R'],
    'L6': ['R', 'L2', 'L1/L4', 'L5', 'Y0', 'L3'],
}

TRIANGLES = sorted([
    'RY0L4', 'RL1L5', 'RL2L3', 'RL2L6', 'RL3L5',
    'Y0L1L2', 'Y0L2L5', 'Y0L3L4', 'Y0L3L6', 'Y0L5L6',
    'L1L2L4', 'L1L4L5', 'L2L4L6', 'L3L4L5', 'L4L5L6',
])

fails = []


def ok(name, cond):
    print(('OK   ' if cond else 'FAIL ') + name)
    if not cond:
        fails.append(name)


def build(nt):
    """eq:base-lines with eq:params and eq:star-slopes."""
    nt = F(nt)
    eps = 1 / (4 * nt)
    eta = 3 * S3 * nt ** 6
    dlt = 2 * nt ** 6
    u = -4 * eta / (2 * eta - 3 + eps * S3 * (2 * eta + 1))
    v = 4 * S3 / (S3 + 9 * eps)
    lines = {                      # (m, a): y = m (x - a)
        'R':  (F(1), dlt),   'Y0': (F(0), F(0)),
        'L1': (F(-1, 2), -S3), 'L2': (-eta, -1 / S3),
        'L3': (eta, 1 / S3),  'L4': (F(2, 5), S3),
        'L5': (u, -eps),      'L6': (v, eps),
    }
    return lines, eps, eta, dlt, u, v


def xs(a, b):
    """x-coordinate of the intersection point, eq:pair-point."""
    (m1, a1), (m2, a2) = a, b
    return (m1 * a1 - m2 * a2) / (m1 - m2)


def rows(lines):
    """Points on each line sorted by x; multiple points merged."""
    out = {}
    for d in NAMES:
        pts = sorted(((xs(lines[d], lines[e]), e) for e in NAMES if e != d),
                     key=lambda t: t[0])
        merged = []
        for x, e in pts:
            if merged and abs(x - merged[-1][0]) < TOL:
                merged[-1][1].append(e)
            else:
                merged.append([x, [e]])
        out[d] = merged
    return out


def check_table(nt):
    lines, *_ = build(nt)
    rw = rows(lines)
    good = True
    for d in NAMES:
        got = ['/'.join(sorted(g)) for _, g in rw[d]]
        exp = ['/'.join(sorted(s.split('/'))) for s in TABLE[d]]
        if got != exp:
            good = False
            print(f'     row {d}: got {got}')
            print(f'          expected {exp}')
    ok(f' eq:seed-orders (n_t={nt})', good)


def check_triangles(nt):
    """Three lines bound a triangle iff their points are adjacent on all three (lem:seed-faces)."""
    lines, *_ = build(nt)
    rw = rows(lines)
    pos = {}
    for d in NAMES:
        for i, (_, g) in enumerate(rw[d]):
            for e in g:
                pos[(d, e)] = i
    tri = []
    for t1, t2, t3 in itertools.combinations(NAMES, 3):
        p12, p13, p23 = (xs(lines[t1], lines[t2]), xs(lines[t1], lines[t3]),
                         xs(lines[t2], lines[t3]))
        if abs(p12 - p13) < TOL and abs(p12 - p23) < TOL:
            continue                      # a triple point
        if all(abs(pos[(a, b)] - pos[(a, c)]) == 1
               for a, b, c in [(t1, t2, t3), (t2, t1, t3), (t3, t1, t2)]):
            tri.append(t1 + t2 + t3)
    ok(f' fifteen triangles, five with R (n_t={nt})',
       sorted(tri) == TRIANGLES and len([t for t in tri if t.startswith('R')]) == 5)


def check_constants():
    print('--- constants independent of n_t')
    ok(' 2/(1+s3/24) > 9/5           (uv)',   2 / (1 + S3 / 24) > F(9, 5))
    ok(' 4s3/(s3+3/8) > 3            (uv)',   4 * S3 / (S3 + F(3, 8)) > 3)
    ok(' (2s3/5-1/12)/(12/5) > 1/4   (i)',    (2 * S3 / 5 - F(1, 12)) / F(12, 5) > F(1, 4))
    ok(' 2s3/11 < 1/3                (i)',    2 * S3 / 11 < F(1, 3))
    ok(' 9/5*(1/s3-1/24) > 24/25     (ii)',   F(9, 5) * (1 / S3 - F(1, 24)) > F(24, 25))
    ok(' 4*(1/s3+1/24) < 5/2         (ii)',   4 * (1 / S3 + F(1, 24)) < F(5, 2))
    ok(' 8/(5s3) < s3                (ii)',   8 / (5 * S3) < S3)
    ok(' s3/5 > 1/3                  (ii)',   S3 / 5 > F(1, 3))
    ok(' 2/s3 < 5/2                  (iii)',  2 / S3 < F(5, 2))
    ok(' 3*(1/s3-1/24) > 3/2         (iii)',  3 * (1 / S3 - F(1, 24)) > F(3, 2))
    ok(' 4/s3 < 5/2                  (iii)',  4 / S3 < F(5, 2))
    ok(' 4/(5s3) > 1/3               (iii)',  4 / (5 * S3) > F(1, 3))
    ok(' -1/s3+1e-4 < -s3/9 < 0      orders', -1 / S3 + F(1, 10000) < -S3 / 9 < 0)
    ok(' 1/3 < 1/s3-1e-4             orders', F(1, 3) < 1 / S3 - F(1, 10000))
    ok(' 3s3 > 5                     (v)',    3 * S3 > 5)


def check_formulas(nt):
    lines, eps, eta, dlt, u, v = build(nt)
    print(f'--- formulas and estimates (n_t={nt})')
    ok(' eta/s3 = 3/2 delta',              eta / S3 == F(3, 2) * dlt)
    ok(' eps<=1/24, eta>2e5, delta>9e4  (eq:A-num)',
       eps <= F(1, 24) and eta > 200000 and dlt > 90000)
    ok(' 10/(eta(eta-4)) < 1e-4/eta    (ii)',
       10 / (eta * (eta - 4)) < F(1, 10000) / eta)
    ok(' (5/2)/(eta-4) < 1e-4          (ii)', F(5, 2) / (eta - 4) < F(1, 10000))
    ok(' (3s3-5)nt^6 > 1               (v)', (3 * S3 - 5) * F(nt) ** 6 > 1)
    ok(' 13delta > 14s3                (v)', 13 * dlt > 14 * S3)

    ok(' P=(-s3/9,-4s3/9) lies on L6',
       xs(lines['L1'], lines['L4']) == -S3 / 9
       and abs(v * (-S3 / 9 - eps) + 4 * S3 / 9) < TOL)
    xq = (eta / S3 - S3 / 2) / (eta + F(1, 2))
    yq = -2 * eta / (S3 * (eta + F(1, 2)))
    ok(' Q=((eta/s3-s3/2)/(eta+1/2), -2eta/(s3(eta+1/2))) lies on L5',
       abs(xs(lines['L1'], lines['L3']) - xq) < TOL
       and abs(u * (xq + eps) - yq) < TOL)

    ok(' x(L4^L5) formula, 1/4 < x < 1/3',
       xs(lines['L4'], lines['L5']) == (2 * S3 / 5 + u * eps) / (F(2, 5) - u)
       and F(1, 4) < xs(lines['L4'], lines['L5']) < F(1, 3))
    ok(' x(L5^L6)=eps(u+v)/(v-u), 0 < x < eps',
       xs(lines['L5'], lines['L6']) == eps * (u + v) / (v - u)
       and 0 < xs(lines['L5'], lines['L6']) < eps)

    good = True
    for d in ['Y0', 'L1', 'L4', 'L5', 'L6']:
        m, a = lines[d]
        g = m * (a + 1 / S3)                 # g_D: minus the height of D at x = -1/s3
        h = m * (1 / S3 - a)                 # h_D: the height of D at x = 1/s3
        x2, x3 = xs(lines['L2'], lines[d]), xs(lines['L3'], lines[d])
        if x2 != -1 / S3 + g / (m + eta) or x3 != 1 / S3 + h / (eta - m):
            good = False
    ok(' x on L2 and L3 via g_D, h_D', good)
    ok(' both clusters within 1e-4 of -+1/s3',
       all(abs(xs(lines['L2'], lines[d]) + 1 / S3) < F(1, 10000)
           and abs(xs(lines['L3'], lines[d]) - 1 / S3) < F(1, 10000)
           for d in ['Y0', 'L1', 'L4', 'L5', 'L6']))
    ok(' V=(0,-3nt^6)                  (iv)',
       xs(lines['L2'], lines['L3']) == 0 and -eta / S3 == -3 * F(nt) ** 6)

    nu = -u
    exp = {
        'Y0': dlt,
        'L1': (2 * dlt - S3) / 3,
        'L2': -dlt / (2 * (1 + eta)),
        'L3': dlt / (2 * (eta - 1)),
        'L4': (5 * dlt - 2 * S3) / 3,
        'L5': (dlt - nu * eps) / (1 + nu),
        'L6': -(dlt - v * eps) / (v - 1),
    }
    ok(' x on R                        (v)',
       all(xs(lines['R'], lines[d]) == exp[d] for d in exp))
    ok(' y(R^D) < -delta/3, five lines  (y-neg)',
       all(xs(lines['R'], lines[d]) - dlt < -dlt / 3
           for d in ['L1', 'L2', 'L3', 'L5', 'L6']))
    # the five chains of (v) that order the points on R
    ok(' chain 1: x(R^L6) < -(delta-1/6)/3 < -delta/4 < -s3/9',
       -(dlt - v * eps) / (v - 1) < -(dlt - F(1, 6)) / 3 < -dlt / 4 < -S3 / 9
       and v * eps < F(1, 6) and v - 1 < 3)
    ok(' chain 2: -s3/9 < x(R^L2) < 0',
       -S3 / 9 < -dlt / (2 * (1 + eta)) < 0)
    ok(' chain 3: 0 < x(R^L3) < 1/5 < delta/4',
       0 < dlt / (2 * (eta - 1)) < F(1, 5) < dlt / 4 and 2 * (eta - 1) > 5 * dlt)
    ok(' chain 4: delta/4 < (delta-1/12)/3 < x(R^L5) < 5delta/14',
       dlt / 4 < (dlt - F(1, 12)) / 3 < (dlt - nu * eps) / (1 + nu) <= dlt / (1 + F(9, 5))
       and nu * eps < F(1, 12))
    ok(' chain 5: 5delta/14 < x(R^L1) < delta < x(R^L4)',
       5 * dlt / 14 < (2 * dlt - S3) / 3 < dlt < (5 * dlt - 2 * S3) / 3)
    ok(' s3 < delta/4', S3 < dlt / 4)

    pts = []
    for a, b in itertools.combinations(['Y0', 'L1', 'L2', 'L3', 'L4', 'L5', 'L6'], 2):
        if {a, b} == {'L2', 'L3'}:
            continue                         # V is outside the box
        x = xs(lines[a], lines[b])
        m, aa = lines[a] if abs(lines[a][0]) <= 4 else lines[b]
        pts.append((x, m * (x - aa)))
    ok(' |x|<=s3, |y|<=8s3 except V   (lem:seed-box)',
       all(abs(x) <= S3 and abs(y) <= 8 * S3 for x, y in pts))
    ok(' y(L2^L6) < -3/2',
       v * (xs(lines['L2'], lines['L6']) - eps) < F(-3, 2))
    ok(' y(Q) < -1', yq < -1)


if __name__ == '__main__':
    print('--- orders and triangles')
    for nt in [6, 12, 24, 48, 96, 192, 6 * 2 ** 10, 6 * 2 ** 20]:
        check_table(nt)
    for nt in [6, 96]:
        check_triangles(nt)
    check_constants()
    for nt in [6, 96, 6 * 2 ** 10]:
        check_formulas(nt)
    print()
    print('all checks passed' if not fails else 'FAIL: ' + '; '.join(fails))
    sys.exit(1 if fails else 0)
