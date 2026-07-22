#!/usr/bin/env python3
# OpenCppCoverage is an open source code coverage for C++.
#
# log_codemod.py -- rewrite transitional LOG_*_STREAM << ... ; statements into
# the destination LOG_*("fmt", args) format form.
#
# For each single statement it splits the << chain at the top level: adjacent
# string literals ("..." / L"...") become literal text in the format string
# (with '{'/'}' doubled), and every other operand becomes a "{}" placeholder
# plus an argument. Statements that use stream manipulators (std::hex, setw, ...)
# are left untouched and reported, so a human converts them. Always review the
# resulting diff and run tools/log_parity.py. See docs/SPDLOG_MIGRATION.md.

import argparse
import pathlib
import re
import sys

STREAM_RE = re.compile(r"\bLOG_(TRACE|DEBUG|INFO|WARNING|ERROR)_STREAM\b")
HARD_MARKERS = (
    "std::hex", "std::dec", "std::oct", "std::setw", "std::setfill",
    "std::setprecision", "std::endl", "std::boolalpha", "std::setbase",
)
# A run of one or more adjacent string literals, optionally L-prefixed.
LITERALS_ONLY = re.compile(r'^(?:\s*L?"(?:[^"\\]|\\.)*"\s*)+$')
ONE_LITERAL = re.compile(r'L?"((?:[^"\\]|\\.)*)"')


def statement_end(text: str, start: int) -> int:
    """Index just past the ';' that ends the statement at `start`."""
    depth = 0
    in_str = None
    esc = False
    for i in range(start, len(text)):
        c = text[i]
        if in_str:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == in_str:
                in_str = None
        elif c in ('"', "'"):
            in_str = c
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == ";" and depth == 0:
            return i + 1
    return -1


def split_stream(remainder: str):
    """Split the text after LOG_*_STREAM on top-level '<<'."""
    segments, cur = [], []
    depth = 0
    in_str = None
    esc = False
    i = 0
    while i < len(remainder):
        c = remainder[i]
        if in_str:
            cur.append(c)
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == in_str:
                in_str = None
            i += 1
        elif c in ('"', "'"):
            in_str = c
            cur.append(c)
            i += 1
        elif c in "([{":
            depth += 1
            cur.append(c)
            i += 1
        elif c in ")]}":
            depth -= 1
            cur.append(c)
            i += 1
        elif depth == 0 and remainder[i:i + 2] == "<<":
            segments.append("".join(cur))
            cur = []
            i += 2
        else:
            cur.append(c)
            i += 1
    segments.append("".join(cur))
    return segments


def literal_text(segment: str):
    """If `segment` is only string literals, return their concatenated content
    with braces escaped for fmt; otherwise None."""
    if not LITERALS_ONLY.match(segment):
        return None
    content = "".join(ONE_LITERAL.findall(segment))
    return content.replace("{", "{{").replace("}", "}}")


def convert_statement(level: str, body: str):
    """body is everything between LOG_*_STREAM and ';'. Returns the new
    statement text, or None to leave it alone."""
    if any(m in body for m in HARD_MARKERS):
        return None
    segments = split_stream(body)
    # segments[0] is the text before the first '<<' (whitespace) -> drop.
    fmt_parts, args = [], []
    for seg in segments[1:]:
        seg = seg.strip()
        if not seg:
            continue
        lit = literal_text(seg)
        if lit is not None:
            fmt_parts.append(lit)
        else:
            fmt_parts.append("{}")
            args.append(seg)
    fmt = "".join(fmt_parts)
    if args:
        return f'LOG_{level}("{fmt}", {", ".join(args)});'
    return f'LOG_{level}("{fmt}");'


def process(path: pathlib.Path, apply: bool) -> tuple:
    text = path.read_text(encoding="utf-8", errors="replace")
    edits = []  # (start, end, new)
    skipped = []
    for m in STREAM_RE.finditer(text):
        level = m.group(1)
        end = statement_end(text, m.start())
        if end < 0:
            continue
        body = text[m.end():end - 1]  # between macro and ';'
        new = convert_statement(level, body)
        if new is None:
            skipped.append((m.start(), " ".join(text[m.start():end].split())[:80]))
            continue
        edits.append((m.start(), end, new))

    if apply and edits:
        for start, end, new in sorted(edits, reverse=True):
            text = text[:start] + new + text[end:]
        path.write_text(text, encoding="utf-8")
    return len(edits), skipped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=pathlib.Path,
                        help="source files to rewrite")
    parser.add_argument("--apply", action="store_true",
                        help="write changes (default: dry-run count only)")
    args = parser.parse_args()

    total, total_skipped = 0, 0
    for path in args.paths:
        converted, skipped = process(path, args.apply)
        total += converted
        total_skipped += len(skipped)
        verb = "converted" if args.apply else "would convert"
        print(f"  {converted:3} {verb}   {len(skipped):2} skipped   {path}")
        for _pos, preview in skipped:
            print(f"        SKIP (manual): {preview}")
    print(f"  ----\n  {total} sites, {total_skipped} skipped"
          f"{'' if args.apply else ' (dry run; pass --apply to write)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
