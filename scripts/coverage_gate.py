#!/usr/bin/env python3
"""Fail closed on real production source + CLI line coverage.

Reads LLVM-cov export, not test self-reported counts. Every production C++
translation unit in src/ and apps/ must be present; tests, generated code,
examples and system headers are excluded by their source roles, not by their
coverage scores. New sources enter the denominator automatically.
"""
import argparse
import json
import pathlib
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, required=True)
    parser.add_argument("--min-line", type=float, default=95.0)
    parser.add_argument("--input", type=argparse.FileType("r"), default=sys.stdin)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    report = json.load(args.input)
    if len(report.get("data", [])) != 1:
        parser.error("LLVM-cov export must contain exactly one instrumentation dataset")
    files = report["data"][0]["files"]
    expected = set()
    for directory in ("src", "apps"):
        expected.update((root / directory).glob("*.cpp"))
        expected.update((root / directory).glob("*.c"))
    expected = {path.resolve() for path in expected}
    if not expected:
        parser.error("no production source files found: refusing empty coverage denominator")

    covered = total = branches_covered = branches_total = 0
    visited = set()
    for entry in files:
        filename = pathlib.Path(entry["filename"]).resolve()
        if filename not in expected:
            continue
        if filename in visited:
            parser.error(f"duplicate coverage entry: {filename}")
        visited.add(filename)
        lines = entry["summary"]["lines"]
        branches = entry["summary"]["branches"]
        if lines["count"] <= 0:
            parser.error(f"zero executable line count for {filename}")
        covered += lines["covered"]
        total += lines["count"]
        branches_covered += branches["covered"]
        branches_total += branches["count"]
        percentage = 100.0 * lines["covered"] / lines["count"]
        print(f"{filename.relative_to(root)}: {lines['covered']}/{lines['count']} lines ({percentage:.2f}%)")
    missing = sorted(expected - visited)
    if missing:
        print("MISSING IN INSTRUMENTATION:", *(str(p.relative_to(root)) for p in missing), sep="\n  ", file=sys.stderr)
        return 2
    line_pct = 100.0 * covered / total
    branch_pct = 100.0 * branches_covered / branches_total if branches_total else 0.0
    print(f"PRODUCTION TOTAL: {covered}/{total} executable lines = {line_pct:.2f}%")
    print(f"BRANCH TOTAL (reported, not gated): {branches_covered}/{branches_total} = {branch_pct:.2f}%")
    if line_pct + 1e-12 < args.min_line:
        print(f"FAIL: {line_pct:.2f}% below minimum {args.min_line:.2f}%", file=sys.stderr)
        return 1
    print(f"PASS: production line coverage meets >= {args.min_line:.2f}%")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
