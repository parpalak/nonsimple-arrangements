#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin
"""Rational line equations of Appendix B
(tab:n20-collapse-lines, tab:n32-lines, tab:n38-lines, tab:n74-lines).

Line i is y = m_i x + b_i with terminating decimals m_i, b_i, and wire i of the
word from ../data/series/. In exact arithmetic checks:
  * on every line the order of the intersection points, with coinciding points
    merged, is that of wire i, up to direction;
  * the multiple points are the given number of triple points, all on the line
    hub;
  * the number of triangles: three lines bound a triangle if their three
    points differ and are adjacent on each of the lines.

The equations come from the decimal lines of the kobon-solutions gallery
(gallery/data/<n>/<id>.json) that the figures are drawn from. Slopes and
intercepts are rounded to d decimals (d = 4 for n = 20, d = 5 for n = 32, 38);
the x-coordinate of each triple point is the mean of its three pairwise
intersections, also to d decimals, the point is put on the hub line, and its
two partner lines are drawn through it (their intercepts to 2d decimals). The
vertices move by at most 0.04 mm from the figures at page scale, 0.003 mm in the
insets.

For n = 74 (no figure) the gallery decimals break the orders already at d = 6,
so the exact rational solution from the certificate
(gallery/certificates/74/74-179ttjdlxwev8.json, field "lines_frac") is
rounded the same way; d = 8 is the least that passes (d = 7 breaks line 25).
The lines are numbered as the wires of ../data/series/74.words.txt.
"""
import os
import sys
from fractions import Fraction as F
from itertools import combinations

# the tables of appendix/b-figures.tex
CONFIGS = [
    dict(n=20, gallery='20-amf7ix7zqi9a', figure='fig:n20-collapse', table='tab:n20-collapse-lines',
         hub=13, triples=3, triangles=117, word=('20.words.txt', 1),
         lines=[
        ("-0.034", "-0.8668"), ("-0.1424", "0.7687"), ("-0.3026", "-3.2348"),
        ("-0.4915", "1.1772"), ("-0.639", "0.6319"), ("-0.7707", "4.87422138"),
        ("-1.1086", "16.07732016"), ("-4.1366", "10.95505632"),
        ("-6.343", "22.67860632"), ("-13.0923", "24.1398"),
        ("12.4722", "-49.01273808"), ("5.3816", "-8.8336011"), ("3.3902", "-9.8084"),
        ("2.4386", "-0.9237"), ("1.2157", "-3.5408"), ("0.833", "-1.7987"),
        ("0.6615", "-2.1437"), ("0.4958", "-0.5564"), ("0.3773", "-2.1232"),
        ("0.034", "0.8634"),
         ]),
    dict(n=32, gallery='32-31imvjadxi50t', figure='fig:n32', table='tab:n32-lines',
         hub=5, triples=4, triangles=315, word=('32.words.txt', 1),
         lines=[
        ("-0.06476", "-0.7560626464"), ("-0.1772", "4.0055813476"),
        ("-0.19072", "1.096562072"), ("-0.62491", "0.88298"), ("-1.61302", "-8.02093"),
        ("-1.74892", "24.01463"), ("-1.82634", "6.28754"), ("-1.91825", "-10.33657"),
        ("-2.06335", "-8.8103"), ("-2.09887", "-12.3846"), ("-3.7305", "-13.36831"),
        ("-5.84564", "-35.30939"), ("-7.7291", "-25.79848"), ("-22.151", "-144.70811"),
        ("13.67108", "42.57195"), ("5.69142", "32.4434"), ("4.11657", "12.85756"),
        ("2.86853", "15.03237"), ("2.80636", "9.09436"), ("2.33795", "10.55365"),
        ("2.12481", "7.49822"), ("1.85599", "7.50283"), ("1.16509", "-2.18024"),
        ("0.59231", "0.48538"), ("0.54137", "-2.54997"), ("0.41874", "-1.8998753596"),
        ("0.39469", "-10.62239"), ("0.26104", "-0.0145623976"),
        ("0.19652", "-1.9637835856"), ("0.18978", "0.89411"), ("0.11893", "-0.9276862965"),
        ("0.06777", "0.8869491177"),
         ]),
    dict(n=38, gallery='38-1l8x1qx7on4je', figure='fig:n38', table='tab:n38-lines',
         hub=20, triples=3, triangles=450, word=('38.words.txt', 6),
         lines=[
        ("-0.04793", "0.6523"), ("-0.0852", "-1.02173"), ("-0.17455", "0.54753"),
        ("-0.21998", "-0.99642"), ("-0.29208", "0.36745"), ("-0.32849", "-0.87598"),
        ("-0.38084", "0.14532"), ("-0.4004", "-0.69737"), ("-0.42956", "-0.12128"),
        ("-0.43625", "-0.4151"), ("-0.90749", "9.22924"), ("-2.40272", "7.63847"),
        ("-2.59757", "15.581461275"), ("-2.99387", "52.766107044"),
        ("-9.7393", "11.9113976453"), ("-10.3901", "17.0924954295"),
        ("-12.70956", "5.44985"), ("-170.97458", "156.7680040263"),
        ("1406.69858", "551.8309088275"), ("39.45675", "-3.68082"),
        ("30.00493", "32.33556"), ("7.85177", "6.5065"), ("4.83331", "9.25869"),
        ("4.55319", "7.42156"), ("3.07899", "10.56713"), ("2.65403", "6.21769"),
        ("1.39238", "10.9832"), ("1.32243", "7.00528"), ("0.45188", "10.11089"),
        ("0.34272", "0.05092"), ("0.3251", "0.28583"), ("0.31861", "-0.22649"),
        ("0.27275", "0.46675"), ("0.26314", "-0.5276"), ("0.18314", "0.59326"),
        ("0.17127", "-0.76651"), ("0.07339", "0.66667"), ("0.05342", "-0.93754"),
         ]),
    dict(n=74, gallery='74-179ttjdlxwev8', figure='-', table='tab:n74-lines',
         hub=40, triples=3, triangles=1764, word=('74.words.txt', 1),
         lines=[
        ("-6.65426468", "-1091.43745452"), ("-6.64810537", "-1090.3864598"), ("-6.13520075", "-1003.89886196"),
        ("-6.13374755", "-1003.64270684"), ("-5.70667318", "-931.62880038"), ("-5.70571304", "-931.45708301"),
        ("-5.35498219", "-872.31688205"), ("-5.35409169", "-872.15825542"), ("-5.0732397", "-824.80147333"),
        ("-5.07157073", "-824.51294547"), ("-4.85294201", "-787.64847296"), ("-4.85223516", "-787.52360093"),
        ("-4.68721255", "-759.69817544"), ("-4.68657182", "-759.58585996"), ("-4.57304306", "-740.44325058"),
        ("-4.57239659", "-740.33138239"), ("-4.5057611", "-729.09571234"), ("-4.50513696", "-728.98903225"),
        ("-4.48337368", "-725.31945462"), ("-1.5910024", "-237.37186659"), ("-0.41986925", "-40.02412869"),
        ("-0.41815525", "-39.68721726"), ("-0.21753219", "-5.89835333"), ("-0.20294042", "-3.41383947"),
        ("-0.1330755", "8.34706968"), ("-0.13299121", "8.37372571"), ("-0.10862691", "12.47316892"),
        ("-0.10854949", "12.49109486"), ("0.12516097", "51.93883116"), ("0.13048166", "52.71558026"),
        ("0.13483437", "53.53791167"), ("0.13954354", "54.3160826"), ("0.14060184", "54.5227052048622872"),
        ("0.14086713", "54.6818822830280324"), ("0.14291553", "54.8884471310340848"), ("0.1430402", "54.9155142351386022"),
        ("0.14305875", "54.90438146"), ("0.14421861", "55.116043480362134"), ("0.14432907", "55.1143550503036888"),
        ("0.14480079", "55.20455821"), ("0.14497591", "55.21414649"), ("0.14739226", "55.63755173"),
        ("0.14962877", "56.00380269"), ("0.15029264", "56.12302765"), ("0.15480384", "56.85487027"),
        ("0.15584749", "57.07155497"), ("0.40556958", "98.28282147"), ("0.4076748", "99.52353415"),
        ("0.40962048", "99.85523857"), ("0.42701422", "102.7788107"), ("0.44390213", "105.63945342"),
        ("0.48516687", "112.57837511"), ("0.54937296", "123.42912769"), ("0.66546018", "142.9647525"),
        ("0.86891663", "177.32112487"), ("1.47866731", "280.03295081"), ("-13.67391141", "-2275.26702125"),
        ("-13.66831944", "-2274.3218405"), ("-13.29563964", "-2211.47504171"), ("-13.28440237", "-2209.57379786"),
        ("-12.60118553", "-2094.35988763"), ("-12.59582406", "-2093.44621711"), ("-11.69989116", "-1942.36153415"),
        ("-11.69402363", "-1941.36001817"), ("-10.71044458", "-1775.49620678"), ("-10.70593721", "-1774.72250631"),
        ("-9.72673613", "-1609.59816906"), ("-9.72381877", "-1609.09187701"), ("-8.80670122", "-1454.43810158"),
        ("-8.80451216", "-1454.05453181"), ("-7.98207725", "-1315.36845915"), ("-7.97990258", "-1314.98770823"),
        ("-7.26432733", "-1194.32251028"), ("-7.26218807", "-1193.9484859"),
         ]),
]


def word_rows(word, n):
    """For each wire, its crossings in order, each as the set of the other wires."""
    slots = list(range(n))
    rows = [[] for _ in range(n)]
    met = set()
    for tok in word.split():
        g, m = int(tok.rstrip('*')), 2 + tok.count('*')
        block = slots[g:g + m]
        assert block == sorted(block), f'{tok}: wires {block} have already crossed'
        for a, b in combinations(block, 2):
            assert (a, b) not in met, f'{tok}: pair {a},{b} crosses twice'
            met.add((a, b))
        for w in block:
            rows[w].append(frozenset(block) - {w})
        slots[g:g + m] = block[::-1]
    assert len(met) == n * (n - 1) // 2, 'incomplete word'
    return rows


def line_rows(lines):
    """The same from the equations, ordered by x."""
    n = len(lines)
    by_x = [{} for _ in range(n)]
    for i, j in combinations(range(n), 2):
        (mi, bi), (mj, bj) = lines[i], lines[j]
        assert mi != mj, f'lines {i} and {j} are parallel'
        x = (bj - bi) / (mi - mj)
        by_x[i].setdefault(x, set()).add(j)
        by_x[j].setdefault(x, set()).add(i)
    return [[frozenset(d[x]) for x in sorted(d)] for d in by_x]


def check(c):
    n = c['n']
    lines = [(F(m), F(b)) for m, b in c['lines']]
    assert len(lines) == n
    name, k = c['word']
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'series', name)
    word = open(path).read().splitlines()[k - 1].split(') ', 1)[1]
    expected, actual = word_rows(word, n), line_rows(lines)
    for i, (e, a) in enumerate(zip(expected, actual)):
        assert a in (e, e[::-1]), f'n={n}, line {i}: the order of crossings differs from the word'

    points = {frozenset(ev | {i}) for i, row in enumerate(actual) for ev in row}
    multiple = sorted(sorted(p) for p in points if len(p) > 2)
    assert all(len(p) == 3 and c['hub'] in p for p in multiple) and len(multiple) == c['triples'], multiple

    pos = [{w: k for k, ev in enumerate(row) for w in ev} for row in actual]
    tri = 0
    for i, j, k in combinations(range(n), 3):
        if pos[i][j] == pos[i][k]:
            continue                      # a triple point, not a triangle
        if (abs(pos[i][j] - pos[i][k]) == 1 and abs(pos[j][i] - pos[j][k]) == 1
                and abs(pos[k][i] - pos[k][j]) == 1):
            tri += 1
    assert tri == c['triangles'], f'n={n}: {tri} triangles, expected {c["triangles"]}'
    print(f'OK: {n} lines ({c["gallery"]}), triple points {multiple}, {tri} triangles')


def main():
    for c in CONFIGS:
        check(c)


if __name__ == '__main__':
    try:
        main()
    except AssertionError as e:
        sys.exit(f'FAIL: {e}')
