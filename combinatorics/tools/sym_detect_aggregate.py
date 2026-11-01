#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin

"""
Aggregate sym_detect JSONL output into a profile table.

Usage:
    sym_detect -i <input.sph.txt> | python3 sym_detect_aggregate.py

Or:
    sym_detect -i <input.sph.txt> > out.jsonl
    python3 sym_detect_aggregate.py < out.jsonl

Optional --expected N enables ETA in the header (when stdin is a stream).

Each P-class is grouped by profile (gp_type, gp_ord, multiset of
m, multiset of (reps, he_type)). Each profile prints as one main row
followed by sub-rows, one per (reps, stab) group of E-classes.
"""

import argparse
import json
import sys
import time
from collections import Counter, defaultdict


def profile_key(r):
    cuts = tuple(sorted(e["m"] for e in r["e_classes"]))
    # m(E) and |H_E| are carried, not recovered from rep(E): rep(E) is
    # 2n/|H_E| only where the odd-n parity filter applies (see "reps" in
    # sorter/README.md), and a chart with parallel lines has a shorter orbit
    # still.  reps may be null when the count is not defined.
    stabs = tuple(sorted((e["reps"] if e["reps"] is not None else 0,
                          e["he_type"], e["m"], e["he_ord"])
                         for e in r["e_classes"]))
    return (r["gp_ord"], r["gp_type"], cuts, stabs)


def words_per_p(r):
    return sum(e["reps"] or 0 for e in r["e_classes"])


def e_breakdown(stabs):
    """Return list of (count, reps, stab_type, m, he_ord), by reps then type."""
    c = Counter(stabs)
    return sorted([(count, reps, st, m, he) for (reps, st, m, he), count in c.items()],
                  key=lambda x: (x[1], x[2]))


# --- Old single-line table (kept commented for possible rollback) ---------
#
# def fmt_cuts(cuts):
#     c = Counter(cuts)
#     parts = [(f"{count}×{val}" if count > 1 else f"{val}")
#              for val, count in sorted(c.items())]
#     return "+".join(parts)
#
#
# def fmt_stabs(stabs):
#     grouped = Counter((reps, st) for reps, st in stabs)
#     parts = [(f"{count}×{reps}/{st}" if count > 1 else f"{reps}/{st}")
#              for (reps, st), count in sorted(grouped.items())]
#     return ", ".join(parts)
#
#
# def main():
#     records = []
#     for line in sys.stdin:
#         line = line.strip()
#         if not line:
#             continue
#         records.append(json.loads(line))
#
#     if not records:
#         print("(no records)", file=sys.stderr)
#         return
#
#     n = records[0]["n"]
#     profiles = defaultdict(list)
#     failed_checks = 0
#     for r in records:
#         profiles[profile_key(r)].append(r)
#         if not all(r["checks"].values()):
#             failed_checks += 1
#
#     rows = []
#     for key, recs in profiles.items():
#         p_ord, p_type, cuts, stabs = key
#         wpp = words_per_p(recs[0])
#         rows.append({
#             "p_count": len(recs),
#             "p_type": p_type,
#             "p_ord": p_ord,
#             "cuts": fmt_cuts(cuts),
#             "stabs": fmt_stabs(stabs),
#             "wpp": wpp,
#             "total": len(recs) * wpp,
#         })
#     rows.sort(key=lambda x: (-x["p_count"], -x["p_ord"]))
#
#     headers = ["P-classes", "P-sym", "|G|", "cuts", "E-classes (reps/type)",
#                "words/P", "total"]
#     table = [headers] + [
#         [str(r["p_count"]), r["p_type"], str(r["p_ord"]),
#          r["cuts"], r["stabs"], str(r["wpp"]), str(r["total"])]
#         for r in rows
#     ]
#     widths = [max(len(row[i]) for row in table) for i in range(len(headers))]
#
#     def line(row):
#         return " | ".join(c.ljust(widths[i]) for i, c in enumerate(row))
#
#     sep = "-+-".join("-" * w for w in widths)
#     print(f"n = {n}, P-classes = {len(records)}, profiles = {len(rows)}")
#     print()
#     print(line(headers))
#     print(sep)
#     for row in table[1:]:
#         print(line(row))
#     print(sep)
#
#     grand_total = sum(r["total"] for r in rows)
#     grand_p = sum(r["p_count"] for r in rows)
#     print(f"Total: {grand_p} P-classes, {grand_total} words")
#     if failed_checks:
#         print(f"WARNING: {failed_checks} record(s) had failing checks", file=sys.stderr)
# --------------------------------------------------------------------------


HEADERS = ["G_P", "|G_P|", "#E/P", "m(E)", "rep(E)", "H_E",
           "rep(P)", "#P", "#E", "#D"]
NUMERIC = {1, 2, 3, 4, 6, 7, 8, 9}

LATEX_HEADERS = [r"$G_P$", r"$|G_P|$",
                 r"\#$E/P$", r"$m(E)$", r"$\operatorname{rep}(E)$",
                 r"$H_E$", r"$\operatorname{rep}(P)$",
                 r"\#$P$", r"\#$E$", r"\#$D$"]


def latex_sym(label):
    """Translate a classifier label (e.g. 'C5', 'D3/S3', 'A4',
    'nonabelian order 24') into LaTeX math using the preprint
    convention: $D_{2n}$ has order $4n$, so the dihedral name $D_k$ is
    the preferred handle for any order-$2k$ group.  Concretely:
      Dk/Sl -> $D_k$   (drop the alternative name)
    Falls back to \\text{...} for shapes the classifier did not name."""
    if not label:
        return ""
    import re
    # Plain Ck / Dk / Sk / Ak.
    m = re.fullmatch(r"([CDSA])(\d+)", label)
    if m:
        return f"${m.group(1)}_{{{m.group(2)}}}$"
    # 'D3/S3'-style isomorphism note: keep only the D part (preprint
    # convention: dihedral name is canonical).
    parts = label.split("/")
    if len(parts) == 2:
        for p in parts:
            m = re.fullmatch(r"D(\d+)", p)
            if m:
                return f"$D_{{{m.group(1)}}}$"
    # Fallback for 'nonabelian order N', 'abelian order N', etc.
    return r"\text{" + label + "}"


def fmt_duration(secs):
    secs = max(0, int(secs))
    h, rem = divmod(secs, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"


class State:
    """Incremental aggregator. Each record is processed exactly once on
    arrival; render() iterates the (small) profiles dict instead of all
    records, so render is O(profiles) and total work stays O(N).
    """

    __slots__ = ("n", "profiles", "grand_p", "failed_checks", "started_at")

    def __init__(self):
        self.n = None
        # key -> {"p_count", "p_type", "p_ord", "ebreak", "wpp"}.
        # Profile-invariants (ebreak, wpp) are computed once when the
        # profile is first seen - the profile key already pins them.
        self.profiles = {}
        self.grand_p = 0
        self.failed_checks = 0
        self.started_at = None

    def add(self, r):
        if self.n is None:
            self.n = r["n"]
            self.started_at = time.monotonic()
        self.grand_p += 1
        if not all(r.get("checks", {}).values()):
            self.failed_checks += 1

        key = profile_key(r)
        prof = self.profiles.get(key)
        if prof is None:
            p_ord, p_type, _cuts, stabs = key
            self.profiles[key] = {
                "p_count": 1,
                "p_type": p_type,
                "p_ord": p_ord,
                "ebreak": e_breakdown(stabs),
                "wpp": words_per_p(r),
            }
        else:
            prof["p_count"] += 1


def render(state, expected=None):
    if state.n is None:
        return "(no records)\n"
    n = state.n

    rows = []
    for prof in state.profiles.values():
        rows.append({
            "p_count": prof["p_count"],
            "p_type": prof["p_type"],
            "p_ord": prof["p_ord"],
            "ebreak": prof["ebreak"],
            "wpp": prof["wpp"],
            "total": prof["p_count"] * prof["wpp"],
        })
    rows.sort(key=lambda x: (-x["p_count"], -x["p_ord"]))

    table = [HEADERS]
    for r in rows:
        prof_e = r["p_count"] * sum(cnt for cnt, *_ in r["ebreak"])
        for i, (cnt, reps, stab, cut_count, _he) in enumerate(r["ebreak"]):
            if i == 0:
                table.append([r["p_type"], str(r["p_ord"]),
                              str(cnt), str(cut_count), str(reps), stab,
                              str(r["wpp"]), str(r["p_count"]),
                              str(prof_e), str(r["total"])])
            else:
                table.append(["", "",
                              str(cnt), str(cut_count), str(reps), stab,
                              "", "", "", ""])

    grand_total = sum(r["total"] for r in rows)
    grand_p = state.grand_p
    # Total E-classes is now a column-sum: each profile contributes
    # #P × Σ #E/P, already accumulated in the per-profile #E column.
    grand_e = sum(r["p_count"] * sum(cnt for cnt, *_ in r["ebreak"])
                  for r in rows)
    # With a single profile the totals row would byte-copy the
    # profile's #P/#E/#D cells; suppress it.
    show_totals = len(rows) > 1
    summary_row = ["", "", "", "", "", "", "", str(grand_p),
                   str(grand_e), str(grand_total)]
    if show_totals:
        table.append(summary_row)

    widths = [max(len(row[i]) for row in table) for i in range(len(HEADERS))]

    def fmt_line(row):
        cells = []
        for i, c in enumerate(row):
            if i in NUMERIC:
                cells.append(c.rjust(widths[i]))
            else:
                cells.append(c.ljust(widths[i]))
        return " | ".join(cells)

    sep = "-+-".join("-" * w for w in widths)
    out = []
    header = f"n = {n}, P-classes = {grand_p}"
    if expected is not None:
        header += f" / {expected}"
        if state.started_at is not None and grand_p > 0:
            elapsed = time.monotonic() - state.started_at
            if grand_p < expected and elapsed > 0:
                rate = grand_p / elapsed
                eta = (expected - grand_p) / rate if rate > 0 else None
                if eta is not None:
                    header += (f", elapsed {fmt_duration(elapsed)}, "
                               f"ETA {fmt_duration(eta)} "
                               f"({rate:.1f} rec/s)")
            elif grand_p >= expected:
                header += f", done in {fmt_duration(elapsed)}"
    header += f", profiles = {len(rows)}"
    out.append(header)
    out.append("")
    out.append(fmt_line(HEADERS))
    out.append(sep)
    body = table[1:-1] if show_totals else table[1:]
    for row in body:
        out.append(fmt_line(row))
    out.append(sep)
    if show_totals:
        out.append(fmt_line(summary_row))
    if state.failed_checks:
        out.append(f"(checks failed in {state.failed_checks} record(s))")
    return "\n".join(out) + "\n"


def fmt_int(val):
    """Group thousands with a LaTeX thin space (\\,) for tabular
    readability; non-integer strings pass through unchanged."""
    s = str(val)
    digits = s[1:] if s.startswith("-") else s
    if not digits.isdigit():
        return s
    groups = []
    while len(digits) > 3:
        groups.insert(0, digits[-3:])
        digits = digits[:-3]
    groups.insert(0, digits)
    return ("-" if s.startswith("-") else "") + r"\,".join(groups)


def latex_body_rows(state, total_label=None):
    """Build the 10-column LaTeX body of one n's profile table.

    Returns ``(rows, rule_after)`` where ``rows`` is a list of cell-lists
    (each of length ``len(LATEX_HEADERS)``) covering every profile sub-row
    plus the optional totals row, and ``rule_after`` is the set of row
    indices after which a separating horizontal rule is drawn (between
    profile blocks and before the totals row).  The single-table and the
    combined renderers share this builder so they stay byte-for-byte
    consistent; they differ only in how a rule is *drawn* (full
    ``\\midrule`` vs. partial ``\\cmidrule`` that spares the leading
    ``n`` column).
    """
    n = state.n
    profiles = []
    for prof in state.profiles.values():
        profiles.append({
            "p_count": prof["p_count"],
            "p_type": prof["p_type"],
            "p_ord": prof["p_ord"],
            "ebreak": prof["ebreak"],
            "wpp": prof["wpp"],
            "total": prof["p_count"] * prof["wpp"],
        })
    profiles.sort(key=lambda x: (-x["p_count"], -x["p_ord"]))

    rows = []
    rule_after = set()
    for r in profiles:
        nsub = len(r["ebreak"])
        prof_e = r["p_count"] * sum(cnt for cnt, *_ in r["ebreak"])

        # Vertically center the per-profile cells across sub-rows
        # (columns: G_P, |G_P|, rep(P), #P, #E, #D).  Use multirow only
        # when the profile actually has multiple sub-rows.
        def span(val, nsub=nsub):
            if nsub > 1:
                return r"\multirow{" + str(nsub) + r"}{*}{" + val + "}"
            return val
        for i, (cnt, reps, stab, cut_count, _he) in enumerate(r["ebreak"]):
            # Column order: ..., m(E), rep(E), H_E, rep(P), ...
            if i == 0:
                cells = [span(latex_sym(r["p_type"])),
                         span(fmt_int(r["p_ord"])),
                         fmt_int(cnt), fmt_int(cut_count),
                         fmt_int(reps), latex_sym(stab),
                         span(fmt_int(r["wpp"])),
                         span(fmt_int(r["p_count"])),
                         span(fmt_int(prof_e)),
                         span(fmt_int(r["total"]))]
            else:
                cells = ["", "", fmt_int(cnt), fmt_int(cut_count),
                         fmt_int(reps), latex_sym(stab), "", "", "", ""]
            rows.append(cells)
        # Mark a rule after each profile block.  With a single profile
        # there is no totals row, so the trailing rule is dropped below.
        rule_after.add(len(rows) - 1)

    # With a single profile the totals row would byte-copy the profile's
    # #P/#E/#D cells; suppress it (and the lone trailing rule with it).
    if len(profiles) > 1:
        grand_total = sum(r["total"] for r in profiles)
        grand_p = state.grand_p
        grand_e = sum(r["p_count"] * sum(cnt for cnt, *_ in r["ebreak"])
                      for r in profiles)
        if total_label is not None:
            # Span the seven left columns (G_P..rep(P)) with a caption that
            # starts at the G_P cell; the three count columns stay separate.
            # The leading @{} suppresses the default \tabcolsep that a bare
            # `l` multicolumn would insert, so the caption lines up exactly
            # with the G_P entries (which inherit the preamble's n|G_P gap)
            # instead of being pushed right by ~\tabcolsep.
            summary = [r"\multicolumn{7}{@{}l}{" + total_label + r"}",
                       fmt_int(grand_p), fmt_int(grand_e),
                       fmt_int(grand_total)]
        else:
            summary = ["", "", "", "", "", "", "",
                       fmt_int(grand_p), fmt_int(grand_e),
                       fmt_int(grand_total)]
        rows.append(summary)
    else:
        rule_after.clear()

    return rows, rule_after


def render_latex(state):
    """Emit a booktabs LaTeX tabular block for a single n."""
    if state.n is None:
        return "% (no records)\n"

    rows, rule_after = latex_body_rows(state)

    lines = []
    lines.append(r"\begin{tabular*}{\linewidth}"
                 r"{@{}l@{\extracolsep{\fill}}rrrrlrrrr@{}}")
    lines.append(r"\toprule")
    lines.append(" & ".join(LATEX_HEADERS) + r" \\")
    lines.append(r"\midrule")
    for i, cells in enumerate(rows):
        lines.append(" & ".join(cells) + r" \\")
        if i in rule_after:
            lines.append(r"\midrule")
    lines.append(r"\bottomrule")
    lines.append(r"\end{tabular*}")
    if state.failed_checks:
        lines.append(f"% WARNING: checks failed in {state.failed_checks} "
                     f"record(s)")
    return "\n".join(lines) + "\n"


def render_latex_combined(entries, italic_ns=frozenset()):
    """Emit the stacked body of the combined table: data rows, intra-n
    partial \\cmidrule separators, Totals rows, the \\midrule separators
    between different n's, and the closing \\bottomrule.

    The header chrome stays in the .tex that \\input's this file: the column
    spec, \\toprule, the two-row group header, and the header/body \\midrule
    (together with the caption, label, and float wrapper).  Keeping the
    header and column spec there -- not here -- means a header tweak is never
    clobbered by a regeneration, and they stay side by side (the
    \\multicolumn spans must match the colspec).  This file is stable across
    added n's, which only append rows.

    The \\bottomrule rides with the body (not the .tex) on purpose: a row
    terminator \\\\ as the very last token of an \\input'd file, followed by a
    \\noalign rule across the file boundary, triggers "Misplaced \\noalign"
    (the \\\\ optional-argument lookahead crosses the file's EOF).  Emitting
    \\bottomrule here keeps that last \\\\ and the rule in the same file.

    ``entries`` is a list of ``(label, state)`` pairs already in display
    order; ``label`` is the math-mode content for the ``n`` cell (e.g.
    ``"7"`` or ``r"25^{\\dagger}"``).  Profile blocks within one n are
    separated by a partial \\cmidrule that spares the n column; different
    n's are separated by a full \\midrule.

    ``italic_ns`` is a set of n whose every cell is set in italics, to
    flag a qualitatively different row (the single-pentagon defect).
    """
    ncol = len(LATEX_HEADERS) + 1  # leading n column

    def italicize(cell):
        # Wrap a non-empty cell in \textit; \multicolumn/\multirow cells are
        # left alone (italic n rows here are single, simple-celled).
        if not cell.strip() or cell.startswith("\\multi"):
            return cell
        return r"\textit{" + cell + "}"

    lines = []
    failed = 0
    last = len(entries) - 1
    for ei, (label, state) in enumerate(entries):
        if state.n is None:
            continue
        failed += state.failed_checks
        # Caption the totals row (present only for multi-profile n) with the
        # value of n, starting at the G_P cell.
        rows, rule_after = latex_body_rows(
            state, total_label=r"Totals for $n=" + str(state.n) + r"$")
        nspan = len(rows)
        ital = state.n in italic_ns
        for i, cells in enumerate(rows):
            if i == 0:
                # The n cell is math-mode, where \textit leaves digits upright;
                # italicize it with \mathit inside the math instead.
                inner = (r"\mathit{" + label + "}") if ital else label
                ncell = (r"\multirow{" + str(nspan) + r"}{*}{$" + inner + r"$}"
                         if nspan > 1 else r"$" + inner + r"$")
            else:
                ncell = ""
            if ital:
                # Only the text-mode data cells need the \textit wrapper.
                cells = [italicize(c) for c in cells]
            lines.append(ncell + " & " + " & ".join(cells) + r" \\")
            if i in rule_after:
                # Partial rule over cols 2..ncol: spares the n column's
                # multirow span but reaches the left edge of the G_P
                # column (no (l) trim, which would start it mid-cell).
                lines.append(r"\cmidrule{2-" + str(ncol) + r"}")
        if ei != last:
            lines.append(r"\midrule")
    lines.append(r"\bottomrule")            # see docstring: keep it with the body
    if failed:
        lines.append(f"% WARNING: checks failed in {failed} record(s)")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--expected", type=int, default=None,
                        help="expected number of P-class records (enables ETA)")
    parser.add_argument("--format", choices=("ascii", "latex", "latex-combined"),
                        default="ascii",
                        help="output format (default: ascii)")
    parser.add_argument("inputs", nargs="*",
                        help="latex-combined only: LABEL=PATH specs, one "
                             "per n, in display order; LABEL is the math "
                             "content of the n cell (e.g. 7 or '25^{\\dagger}')")
    parser.add_argument("--italic-n", default="",
                        help="latex-combined only: comma-separated list of n "
                             "whose rows are set in italics (e.g. 25)")
    args = parser.parse_args()

    if args.format == "latex-combined":
        italic_ns = frozenset(int(x) for x in args.italic_n.split(",")
                              if x.strip())
        entries = []
        for spec in args.inputs:
            label, sep, path = spec.partition("=")
            if not sep:
                print(f"error: bad spec {spec!r}, expected LABEL=PATH",
                      file=sys.stderr)
                sys.exit(2)
            st = State()
            with open(path) as fh:
                for line in fh:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        st.add(json.loads(line))
                    except json.JSONDecodeError:
                        continue
            entries.append((label, st))
        sys.stdout.write(render_latex_combined(entries, italic_ns=italic_ns))
        sys.stdout.flush()
        return

    # Live TTY rendering is only meaningful for ascii; latex is one-shot.
    is_tty = sys.stdout.isatty() and args.format == "ascii"
    state = State()
    last_draw = 0.0
    THROTTLE_S = 0.2

    # Enter alternate screen buffer on tty so live frames don't pollute
    # scrollback. We always switch back in `finally` (incl. on Ctrl+C).
    if is_tty:
        sys.stdout.write("\033[?1049h\033[H")
        sys.stdout.flush()

    interrupted = False
    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except json.JSONDecodeError:
                continue
            state.add(r)

            if is_tty:
                now = time.monotonic()
                if now - last_draw >= THROTTLE_S:
                    sys.stdout.write("\033[H\033[J")
                    sys.stdout.write(render(state, args.expected))
                    sys.stdout.flush()
                    last_draw = now
    except KeyboardInterrupt:
        interrupted = True
    finally:
        if is_tty:
            sys.stdout.write("\033[?1049l")
            sys.stdout.flush()

    if args.format == "latex":
        sys.stdout.write(render_latex(state))
    else:
        sys.stdout.write(render(state, args.expected))
        if interrupted:
            sys.stdout.write("(interrupted)\n")
    sys.stdout.flush()
    if state.failed_checks:
        print(f"WARNING: {state.failed_checks} record(s) had failing checks",
              file=sys.stderr)


if __name__ == "__main__":
    main()
