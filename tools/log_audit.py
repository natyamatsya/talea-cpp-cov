#!/usr/bin/env python3
# OpenCppCoverage is an open source code coverage for C++.
#
# log_audit.py -- inventory of the boost-log -> spdlog conversion.
#
# Lists every remaining transitional LOG_*_STREAM << ... site, classifies each
# as "simple" (mechanically convertible to the LOG_*("fmt", ...) form) or
# "hard" (uses stream manipulators or otherwise needs a human), and enforces the
# invariants that keep spdlog statically linkable. See docs/SPDLOG_MIGRATION.md.
#
# Exit code is non-zero if any invariant is violated, so this can gate CI.

import argparse
import pathlib
import re
import sys

# Directories that are not our source.
EXCLUDED_DIRS = {
    "inspiration", "vcpkg_installed", "packages", ".git", "x64", "Debug",
    "Release",
}
EXCLUDED_PARTS = ("build/third-party", "build\\third-party")

SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".inl"}

STREAM_RE = re.compile(r"\bLOG_(TRACE|DEBUG|INFO|WARNING|ERROR)_STREAM\b")

# Things that make a stream chain non-mechanical to convert.
HARD_MARKERS = (
    "std::hex", "std::dec", "std::oct", "std::setw", "std::setfill",
    "std::setprecision", "std::endl", "std::boolalpha", "std::setbase",
    "boost::",
)


def iter_sources(root: pathlib.Path):
    for path in root.rglob("*"):
        if path.suffix not in SOURCE_SUFFIXES:
            continue
        parts = set(path.relative_to(root).parts)
        if parts & EXCLUDED_DIRS:
            continue
        rel = path.relative_to(root).as_posix()
        if any(part in rel for part in EXCLUDED_PARTS):
            continue
        yield path


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def statement_from(text: str, start: int) -> str:
    """Capture from `start` up to the terminating ';' at statement level,
    skipping ';' inside strings, char literals and parentheses."""
    depth = 0
    i = start
    in_str = None  # '"' or "'"
    escaped = False
    out = []
    while i < len(text):
        c = text[i]
        out.append(c)
        if in_str:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == in_str:
                in_str = None
        else:
            if c in ('"', "'"):
                in_str = c
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c == ";" and depth == 0:
                break
        i += 1
    return "".join(out)


def classify(statement: str) -> str:
    return "hard" if any(m in statement for m in HARD_MARKERS) else "simple"


def line_of(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def check_invariants(root: pathlib.Path):
    """Return a list of violation strings."""
    violations = []
    for path in iter_sources(root):
        rel = path.relative_to(root).as_posix()
        text = read(path)
        is_log = rel in ("tools/Log.hpp", "tools/Log.cpp")
        if not is_log:
            for m in re.finditer(r"boost::log|boost/log", text):
                violations.append(
                    f"{rel}:{line_of(text, m.start())}: boost::log outside tools/Log.*")
        if rel != "tools/Log.cpp":
            for m in re.finditer(r"#\s*include\s*<spdlog/", text):
                violations.append(
                    f"{rel}:{line_of(text, m.start())}: spdlog include outside "
                    f"tools/Log.cpp (would duplicate the logger)")
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", type=pathlib.Path)
    parser.add_argument("--list", action="store_true",
                        help="print every site, not just the summary")
    args = parser.parse_args()
    root = args.root.resolve()

    per_file = {}
    total = {"simple": 0, "hard": 0}
    sites = []
    for path in iter_sources(root):
        text = read(path)
        rel = path.relative_to(root).as_posix()
        if rel in ("tools/Log.hpp",):
            continue  # macro definitions, not call sites
        for m in STREAM_RE.finditer(text):
            statement = statement_from(text, m.start())
            kind = classify(statement)
            total[kind] += 1
            counts = per_file.setdefault(rel, {"simple": 0, "hard": 0})
            counts[kind] += 1
            sites.append((rel, line_of(text, m.start()), kind,
                          " ".join(statement.split())[:100]))

    print("=== LOG_*_STREAM sites remaining (conversion backlog) ===")
    for rel in sorted(per_file):
        c = per_file[rel]
        print(f"  {c['simple']:3} simple  {c['hard']:3} hard   {rel}")
    print(f"  ----\n  {total['simple']} simple, {total['hard']} hard, "
          f"{total['simple'] + total['hard']} total in {len(per_file)} files")

    if args.list:
        print("\n=== sites ===")
        for rel, line, kind, preview in sorted(sites):
            print(f"  [{kind:6}] {rel}:{line}: {preview}")

    violations = check_invariants(root)
    print("\n=== invariants ===")
    if violations:
        for v in violations:
            print(f"  VIOLATION {v}")
        return 1
    print("  ok: no boost::log outside tools/Log.*, no stray spdlog includes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
