#!/usr/bin/env python3
# Fold an xperf "-a dumper -stacktimeshifting [-symbols]" text stream (on stdin)
# into a flame-graph SVG for one process, print the top self-time hotspots, and
# write the universal "folded stacks" file (drop into speedscope.app or
# flamegraph.pl). Streaming and constant-memory, so it scales to huge traces.
#
#   xperf -i trace.etl -symbols -a dumper -stacktimeshifting \
#       | python fold-flamegraph.py <Process.exe> <out.svg>
#
# See README.md for the full capture recipe.
import sys, html, colorsys, time

if len(sys.argv) < 3:
    sys.stderr.write("usage: fold-flamegraph.py <process.exe> <out.svg>  (xperf dump on stdin)\n")
    sys.exit(2)
PROC, SVG = sys.argv[1], sys.argv[2]
t_wall0, t_cpu0, lines_read = time.perf_counter(), time.process_time(), 0

folded = {}        # "root;...;leaf" -> samples
exclusive = {}     # leaf -> self samples
cur_occ = False
cur_frames = []    # [leaf, ..., root]

def flush():
    global cur_frames
    if cur_occ and cur_frames:
        folded[";".join(reversed(cur_frames))] = folded.get(";".join(reversed(cur_frames)), 0) + 1
        exclusive[cur_frames[0]] = exclusive.get(cur_frames[0], 0) + 1
    cur_frames = []

for line in sys.stdin:
    lines_read += 1
    if line.startswith("         SampledProfile,") or line.lstrip().startswith("SampledProfile,"):
        flush()
        f = line.split(",", 3)
        cur_occ = len(f) > 2 and PROC in f[2]
        cur_frames = []
    elif "Stack," in line[:24] and cur_occ:
        f = line.split(",", 5)
        if len(f) >= 6 and f[5].strip():
            cur_frames.append(f[5].strip())
flush()

total = sum(folded.values())
if not total:
    sys.stderr.write("No samples for process %r\n" % PROC)
    sys.exit(1)

with open(SVG + ".folded", "w", encoding="utf-8") as fh:
    for stack, cnt in sorted(folded.items(), key=lambda x: -x[1]):
        fh.write("%s %d\n" % (stack, cnt))

sys.stderr.write("Total %s samples: %d\n\nTop 25 by self (exclusive) samples:\n" % (PROC, total))
for fn, c in sorted(exclusive.items(), key=lambda x: -x[1])[:25]:
    sys.stderr.write("  %6.2f%%  %7d  %s\n" % (100.0 * c / total, c, fn))

# --- tree + SVG ---
class N:
    __slots__ = ("c", "v")
    def __init__(self): self.c = {}; self.v = 0
root = N()
for stack, cnt in folded.items():
    n = root; n.v += cnt
    for fr in stack.split(";"):
        n = n.c.setdefault(fr, N()); n.v += cnt

WIDTH, XPAD, FH, FONT = 1800, 10, 16, 12
def depth(n): return 1 + max((depth(c) for c in n.c.values()), default=0)
height = depth(root) * FH + 40
def is_kernel(s): return s[:8].lower().startswith(("ntoskrnl", "ntdll", "win32", "hal!", "wow64"))
def color(name):
    h = (hash(name) & 0xffff) / 0xffff
    if is_kernel(name):
        r, g, b = colorsys.hsv_to_rgb(0.57, 0.30, 0.55 + 0.15 * h)
    else:
        r, g, b = colorsys.hsv_to_rgb(0.06 + 0.06 * h, 0.55 + 0.15 * h, 0.95)
    return "#%02x%02x%02x" % (int(r*255), int(g*255), int(b*255))
plot = WIDTH - 2 * XPAD
rects = []
def emit(n, x, level, name):
    w = plot * n.v / total
    if w < 0.4: return
    if name is not None:
        y = height - (level + 1) * FH - 20
        mc = int(w / (FONT * 0.6))
        txt = (name if len(name) <= mc else name[:mc-2] + "..") if mc >= 3 else ""
        rects.append((x, y, w, name, n.v, txt))
    cx = x
    for cn, c in sorted(n.c.items(), key=lambda kv: -kv[1].v):
        emit(c, cx, level + 1 if name is not None else level, cn)
        cx += plot * c.v / total
emit(root, XPAD, -1, None)

out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="Consolas,monospace" font-size="%d">' % (WIDTH, height, FONT),
       '<rect width="100%%" height="100%%" fill="#f8f8f8"/>',
       '<text x="%d" y="18" font-size="14" font-weight="bold">%s CPU flame graph - %d samples</text>' % (XPAD, html.escape(PROC), total)]
for x, y, w, name, cnt, txt in rects:
    out.append('<g><title>%s (%.2f%%, %d samples)</title><rect x="%.2f" y="%d" width="%.2f" height="%d" fill="%s" stroke="#fff" stroke-width="0.3"/>%s</g>'
               % (html.escape(name), 100.0*cnt/total, cnt, x, y, max(w-0.5, 0.2), FH-1, color(name),
                  ('<text x="%.2f" y="%d">%s</text>' % (x+2, y+FH-4, html.escape(txt))) if txt else ''))
out.append('</svg>')
open(SVG, "w", encoding="utf-8").write("\n".join(out))

wall, cpu = time.perf_counter() - t_wall0, time.process_time() - t_cpu0
sys.stderr.write("\nWrote %s (%d frames) and %s.folded\n" % (SVG, len(rects), SVG))
sys.stderr.write("fold timing: %.2fs wall, %.2fs CPU, %d input lines (%.0f k lines/s CPU)\n"
                 % (wall, cpu, lines_read, (lines_read / cpu / 1000.0) if cpu else 0.0))
