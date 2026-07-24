#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors
#
# log_parity.py -- regression gate for the boost-log -> spdlog conversion.
#
# `capture` runs talea-cpp-cov over a fixed target at --verbose, then
# normalizes the volatile parts of LastCoverageResults.log (timestamps, pids,
# addresses, durations) and writes the result to a snapshot file. Convert a
# batch of LOG_* sites between two captures and `compare` them: the destination
# format form must produce a byte-identical normalized log. See
# docs/SPDLOG_MIGRATION.md.

import argparse
import difflib
import pathlib
import re
import subprocess
import sys
import tempfile

NORMALIZERS = [
    (re.compile(r"\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\]"), "[TIME]"),
    (re.compile(r"(Process:)\d+"), r"\1PID"),
    (re.compile(r"(Thread:)\d+"), r"\1TID"),
    (re.compile(r"0x[0-9a-fA-F]+"), "0xADDR"),
    (re.compile(r"\b\d+ ms\b"), "N ms"),
]


def normalize(text: str) -> str:
    out = []
    for line in text.splitlines():
        for rx, repl in NORMALIZERS:
            line = rx.sub(repl, line)
        out.append(line)
    return "\n".join(out) + "\n"


def capture(exe: pathlib.Path, target: pathlib.Path, out: pathlib.Path,
            sort: bool) -> int:
    workdir = pathlib.Path(tempfile.mkdtemp(prefix="talea_parity_"))
    cobertura = workdir / "cov.xml"
    cmd = [str(exe), "--verbose", "--export_type",
           f"cobertura:{cobertura}", "--", str(target)]
    subprocess.run(cmd, cwd=str(workdir), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)

    log = workdir / "LastCoverageResults.log"
    text = log.read_text(encoding="utf-8", errors="replace") \
        if log.exists() else ""
    # The randomly-named work directory leaks into the log (the export path);
    # replace it so two runs are comparable.
    for sep in (str(workdir), str(workdir).replace("\\", "/")):
        text = text.replace(sep, "TMPDIR")
    normalized = normalize(text)
    if sort:
        # Guards against parallel-preparse line reordering: compare the log as
        # a multiset of lines rather than a sequence.
        normalized = "\n".join(sorted(normalized.splitlines())) + "\n"
    out.write_text(normalized, encoding="utf-8")
    print(f"captured {len(text.splitlines())} log lines -> {out}")
    return 0


def compare(a: pathlib.Path, b: pathlib.Path) -> int:
    ta = a.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    tb = b.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    diff = list(difflib.unified_diff(ta, tb, fromfile=str(a), tofile=str(b)))
    if diff:
        sys.stdout.writelines(diff)
        print(f"\nPARITY FAILED: {a} and {b} differ")
        return 1
    print(f"PARITY OK: {a.name} == {b.name}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    cap = sub.add_parser("capture")
    cap.add_argument("--exe", required=True, type=pathlib.Path)
    cap.add_argument("--target", required=True, type=pathlib.Path)
    cap.add_argument("--out", required=True, type=pathlib.Path)
    cap.add_argument("--sort", action="store_true",
                     help="compare as a multiset of lines (ignore ordering)")

    cmp = sub.add_parser("compare")
    cmp.add_argument("before", type=pathlib.Path)
    cmp.add_argument("after", type=pathlib.Path)

    args = parser.parse_args()
    if args.command == "capture":
        return capture(args.exe, args.target, args.out, args.sort)
    return compare(args.before, args.after)


if __name__ == "__main__":
    sys.exit(main())
