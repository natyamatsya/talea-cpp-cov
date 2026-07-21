# High-Performance Code Coverage — Roadmap & Status

*Making OpenCppCoverage fast on large translation units, with a data-oriented
design (DOD) throughline. Private, experimental fork. This is the single
canonical roadmap; it consolidates the earlier tiered drafts and the shipped
work into one place.*

> **Status (July 2026).** The data-oriented core, the startup path, and **three
> collection engines** (debugger + run-merging, in-process VEH, compiler
> instrumentation) are **shipped and measured**. Intel PT was dropped (hardware
> lock-in); the DIA-free PDB reader and the columnar `.cov` v2 / bitmap-OR merge
> are now shipped too. What remains is mostly feature extensions and a
> portability proof — see [§4](#4-open-work-prioritized).

---

## 1. Where the time goes — the three levers

Measured on synthetic TUs, Release x64. "skip" = instrumented but exits
immediately (startup only); "full" = every line executed once. This is the
*baseline* (debugger engine, before any of the work below):

| Lines | native | startup only | all lines hit |
|------:|-------:|-------------:|--------------:|
| 10k   |  98 ms |       102 ms |        626 ms |
| 100k  | 184 ms |       471 ms |      5,040 ms |
| 200k  | 303 ms |       922 ms |     10,312 ms |

Two dominant costs, two findings:

- **Startup is dominated by the number of selected _source files_, not lines.**
  100k lines: 471 ms in one file, **4,731 ms across 5,000 headers** — the
  templated / unity-build worst case, repeated per module and per child process.
  The cost was O(files) DIA `findLines` calls.
- **Runtime was ~50 µs per first-executed line, perfectly linear** — one
  `WaitForDebugEvent → GetThreadContext → WriteProcessMemory → ContinueDebugEvent`
  cross-process round trip per hit. Planting the `0xCC` bytes is free by
  comparison (millions/sec); **the round trip is everything.**

So there are exactly three levers, and everything maps to one:

1. **Startup** — stop doing O(files × compilands) DIA work per module per process.
2. **Number of first-hits** — instrument fewer probes (block leaders, not every line).
3. **Cost per first-hit** — service a hit cheaper than a ~100 µs debug event, or
   stop trapping entirely (coverage-as-data).

**The DOD lens.** *CONTROL* mechanisms pay a trap per event (a debug event per
line); *DATA* mechanisms let execution write into a buffer/counter array at
near-native speed and decode later. *Binary-only* keeps OpenCppCoverage's core
value (instrument what you didn't specially build); *recompile* is the
throughput ceiling but forfeits it. The internal work (levers 1 + the data
model) is DOD applied to our own structures; the engine work moves from CONTROL
toward DATA.

---

## 2. Status at a glance

| Workstream | Lever | Status | Headline result |
|---|---|---|---|
| Tier 0 mechanical fixes | 1 | ✅ shipped | export 6.2×, debug-heap gone |
| Single-pass DIA enumeration | 1 | ✅ shipped | 5,000-header startup 4,731 → 745 ms |
| PDB parse cache (mem + disk) | 1 | ✅ shipped | warm startup 745 → ~240 ms |
| Columnar probe table + interning | core | ✅ shipped | structural; enables merge/parallel |
| Run merging (block-leader probes) | 2 | ✅ shipped | full run 4,990 → 290 ms (1 file) |
| In-process VEH servicing | 3 | ✅ shipped | hit-heavy 5,500 → 1,060 ms; DLLs |
| Compiler-instrumentation fast mode | 2+3 | ✅ shipped | near-native; clang-cl **+ MSVC** |
| DIA-free raw PDB reader (raw_pdb) | 1 | ✅ shipped | cold parse ~4×; parses compilands in parallel |
| Parallelism (HTML export) | 1 | ⏳ tried | I/O+alloc bound — no win; thread-local template cache kept |
| Columnar `.cov` v2 + bitmap-OR merge | — | ✅ shipped | 30-shard merge 1360 → 655 ms (~2×); `.cov` −35% |
| Parallelism (shard parse/merge) | — | ⏳ tried | alloc-bound — no win; serial columnar kept |
| VEH: child processes / branch coverage | 3 | ⬜ open | feature parity with debugger |
| Fast mode: multi-module / ARM64 validation | 2+3 | ⬜ open | breadth + portability proof |
| TinyInst-style binary rewriting | 3 | ⬜ open | binary-only "data" mode (~15–20%) |
| Intel PT | 4 | ❌ dropped | Intel-only; portability lock-in |

**Cumulative effect on the 100k-line worst cases** (Release, warm cache):

| Scenario | Baseline | Now | How |
|---|--:|--:|---|
| 5,000-header startup ("skip") | 4,731 ms | ~240 ms | single-pass DIA + PDB cache + run merging |
| 100k lines full run, 1 file | 4,990 ms | 290 ms | run merging |
| 100k lines full run, 5,000 files | 5,850 ms | 880 ms | run merging |
| 100k *distinct executed* probes | 5,500 ms | 1,060 ms | VEH (per-hit ~50 µs → ~µs) |
| any of the above, if you can recompile | — | near-native | fast mode |

---

## 3. What shipped

### 3.1 Startup (lever 1)

**Tier 0 mechanical fixes.** Skip `findSymbolByVA` per line record unless
`--optimized_build`; `LineFilter` empty-regex early-out; `_NO_DEBUG_HEAP=1` in
the debuggee; a streaming Cobertura writer replacing `boost::property_tree`
(**~140 → 22.6 ms, 6.2×**, byte-identical, and one fewer boost dependency);
reuse the debug-event `wostringstream`; `CONTEXT_CONTROL` not `CONTEXT_ALL`;
drop the no-op `FlushInstructionCache`.

**Single-pass DIA enumeration.** The dominant startup cost was
`get_compilands` + `findLines(compiland, file)` issued **per selected source
file** (O(files) COM calls). Replaced by one `findLinesByRVA(0, 0xFFFFFFFF)`
query over the whole image, bucketing records by the remapped source-file name
(DIA's `get_uniqueId` is inconsistent across enumeration paths; the name is the
stable key). **5,000 headers 4,731 → 745 ms.**

**PDB parse cache** (`PdbSignature`, `PdbLineTableCache`). Keyed by the PDB
`(GUID, age)` read from the PE debug directory (RSDS) *without* DIA — the
linker's exact identity, so a relink changes the key and stale entries are
unreachable by construction. Two layers: in-memory (child processes, DLL
reloads) and a disk layer under `%LOCALAPPDATA%\OpenCppCoverage\PdbCache`
(atomic temp+rename, bounds-checked mmap load, LRU-pruned at 256). The cached
table stores *raw* PDB names and *all* files, so source selection and
`--substitute_pdb_source_path` apply at replay time and one table is valid under
any filter. **Warm startup 745 → ~240 ms; `--no_pdb_cache` disables it.**

**DIA-free raw PDB reader** (`RawPdbReader`, on the vendored **raw_pdb**). The
default parse path now memory-maps the PDB and scans the columnar CodeView
streams directly instead of paying DIA's ≥6 COM calls per line record — and,
because raw_pdb streams are lock-free immutable memory, it parses each
compiland's line stream **in parallel** (`std::execution::par`; this is why
raw_pdb over COM-bound DIA, and where parallel STL beats a sender/receiver
graph). It produces the same `ModuleLineTable` as DIA (raw names, RVA + code
length, sorted function ranges — validated byte-for-byte by `RawPdbReaderTest`),
so the cache and all downstream are unchanged; DIA remains the fallback for
`/DEBUG:FASTLINK`, a missing/mismatched PDB, and the DIA-only data
(`--optimized_build` symbol indexes, fast-mode demangled names). This attacks
the *cold* parse the cache only hides on a warm run — the first run of every CI
job. **Cold parse (no cache): 5,000 headers 434 → 244 ms, 100k lines/1 file
275 → 146 ms whole-run; the parse component itself ~4×. `--no_raw_pdb` forces
DIA.**

### 3.2 The data-oriented core

`ExecutedAddressManager` rewritten to a structure-of-arrays **columnar probe
table** (parallel `moduleId`/`fileId`/`lineNumber`/`executed` columns indexed by
a dense probe id), interned file paths, a contiguous byte hit-bitset, and an
address fan-out of probe *ids* (indices stable across growth) instead of `bool*`
aliasing container nodes. `std::map` address indexes → `unordered_map` with an
`Address` hash. `FileCoverage::GetLineCollection()` gives zero-copy line
iteration (merge ~35% faster). Behavior and output unchanged — this is the clean
model the merge/parallel work hangs off, not itself a speedup (the debug round
trip dominated).

### 3.3 Fewer, cheaper traps (levers 2 & 3, binary-only)

**Run merging — block-leader probes** (`LineRunScanner`, on by default,
`--no_line_merging` opts out). A breakpoint on the first instruction of a
straight-line run proves execution of every line the run touches. Shipped as a
*sound* variant — output is **exactly identical** to per-line breakpoints, not
an approximation — and entirely as a registration-time transform (the probe
table already supports many (file, line) per address). Runs end at every branch,
return, interrupt **and call** (a callee can throw, keeping exception paths
exact); functions with an indirect jump fall back to per-line; runs never span
functions (ranges from the PDB `SymTagFunction` symbols, cached). **Full run
4,990 → 290 ms (1 file, 17×), 5,850 → 880 ms (5,000 files, 6.6×);** startup also
improves (~5,000 breakpoints planted instead of 100,000).

**In-process VEH servicing** (`--experimental_veh`; `VehCoverageEngine` +
self-contained `VehCoverageHelper.dll`). Instead of debugging, the target is
launched suspended, a tiny helper (kernel32-only) is injected, and a Vectored
Exception Handler services each `int3` in-process — a usermode dispatch instead
of the cross-process debug round trip. All probe placement is computed **offline**
from the PDB and module file (reusing the same enumeration, filters, run
merging, and probe table), so it is a new *collection mechanism*, not a new
measurement: **byte-identical to the debugger** (asserted for basic/thread/
exception runs and a loaded DLL). Covers the main exe plus filter-matching DLLs
under its directory (patched via `LdrRegisterDllNotification` as they load).
**Hit-heavy 100k probes: 4,650 → 810 ms (1 file, 5.8×), 5,500 → 1,060 ms (5,000
files, 5.2×);** the win scales with the number of distinct executed probes, and
composes with run merging. *Limits:* 64-bit only, no child processes, no branch
coverage, DLLs must be under the exe directory.

### 3.4 Coverage-as-data — compiler-instrumentation fast mode (recompile)

`--input_fastcov`; a freestanding runtime + `FastCoverageImporter`. The target
is built with SanitizerCoverage `inline-8bit-counters` — each basic block is one
`inc byte [counter]`, **no trap, no kernel, no per-hit round trip** — and a
tiny runtime dumps the counters at exit. Portable: the same flags target x64 and
ARM64, so nothing is CPU-vendor-locked (this is the answer to what Intel PT would
have given us, without the lock-in).

- **Line-exact via control-flow reconstruction.** SanitizerCoverage uses
  *minimal* instrumentation — a block whose execution is implied by another
  (entry, merge, return) carries no counter. The importer rebuilds each
  function's CFG from the disassembly and propagates coverage from the
  instrumented subset (entry dominates all → covered if any block ran; a covered
  block's single successor and single predecessor also ran). Sound for normal
  flow, never over-reports. **Verified line-identical to the debugger** on simple
  and complex control flow (loops, switch, nested branches, early returns) at
  `/Od` and `/O2`.
- **Both toolchains.** clang-cl emits a PC table (link with **lld** — the MSVC
  linker mis-resolves the sancov boundary symbols); MSVC (`cl.exe`) has no PC
  table and its own `.sancov` scheme, so the importer recovers block addresses by
  disassembling for the `inc byte [rip+disp]` increments. Both then feed the
  *same* CFG reconstruction, line mapping, and template axis; two runtime source
  files are the only toolchain-specific part.
- **Template instantiation axis.** Each block is attributed to its containing
  function (the instantiation) via the demangled PDB name.
  `--fastcov_template_report` emits, per template, each instantiation's line
  coverage **and the lines covered in some but not all instantiations** — the gap
  a single boolean per line hides (a line run for `foo<int>` but never for
  `foo<double>`). *Limit:* `/OPT:ICF`-folded and fully-inlined instantiations
  can't be told apart (inherent).

---

## 4. Open work (prioritized)

### A. Parallelism — the rest of the immutable pipeline

PDB parsing already goes parallel per-compiland inside the raw_pdb reader
(§3.1) — the one part that is genuinely CPU-bound and scaled. A thread-local
`ctemplate` template cache was also added (each HTML worker resolves the
immutable template once instead of taking ctemplate's global mutex per file).

**HTML generation was tried and reverted:** parallelizing the per-file loop was
a wash to a slight regression even on 32 cores with large files, because HTML
generation is bound by string allocation (the page buffers contending on the
heap lock) and disk writes, not CPU. Lesson: parallelism only pays where the
work is CPU/data-bound.

The `.cov` **merge as a bitmap-OR reduce** shipped (§B) and is the third
measured parallelism negative result: parsing shards concurrently
(`std::execution::par`) was a wash-to-slight-regression (30 shards 671 vs 655 ms
serial), because building each shard's per-file `FileData` is thousands of small
vector/map allocations contending on the heap lock — allocation-bound, like HTML
export. The serial columnar merge is what shipped. **Filter scans** over the
frozen table are already fast. Pre-parsing the exe's import closure *before*
`CreateProcess` is a further raw_pdb win. (The `WaitForDebugEvent` loop itself is
single-threaded by OS contract; don't fight it.) Standing lesson across three
tries (HTML, merge parse, and the one win — PDB compiland parse): parallelism
pays **only where the work is CPU-bound, not allocation- or I/O-bound**; when it
is warranted, use the parallel STL, not stdexec.

### B. Columnar `.cov` v2 + merge-as-bitmap-OR — ✅ shipped

Shipped: the binary format is now columnar (`OCC2` magic + version for
back-compat; legacy protobuf v1 still read via magic sniffing). Per (module,file)
it stores the line numbers as delta-varints and a packed `executed` bitmap, plus
a sparse branch side-table, behind an interned path table. `--input_coverage`
merges through `ColumnarCoverageMerger`, which reads each v2 shard straight into
an accumulator: same-build shards share an identical line array, so the merge is
a word-wise OR of the bitmaps; different builds fall back to a sorted line union.
Output stays sorted by module/file/line, so exports are unchanged. Measured on 30
shards of 5,000 files: merge **1360 → 655 ms (~2×)**, `.cov` **1436 → 934 KB
(−35%)**. Remaining option: emit the **drcov v2** format so BB output is
consumable by Lighthouse (IDA / Binary Ninja / Ghidra).

### C. Feature extensions to the shipped engines

- **VEH child processes** — the debugger engine covers children; VEH does not.
  Needs the helper to propagate into `CreateProcess` descendants (inject on
  child creation, or a shared registrar).
- **VEH branch coverage** — the debugger engine has it (Zydis-based Jcc
  discovery + trap-flag re-arm); VEH would need the in-process handler to do the
  single-step re-arm.
- **Fast mode: broader module discovery** — currently the main executable; walk
  the import closure / instrument DLLs, like the VEH engine does.
- **Fast mode: ARM64 validation** — the flags and the CFG reconstruction are
  arch-agnostic by construction, but the disassembly (Zydis, x86-only) and the
  MSVC `inc byte [rip+disp]` recognition are x64-specific. An ARM64 backend needs
  a fixed-width decoder (much simpler than x86) and the ARM64 counter-increment
  form. Cannot be validated without ARM64 Windows hardware.

### D. TinyInst-style selective binary rewriting — the binary-only "data" mode

Mark the target module non-executable, fault on first entry, JIT-rewrite basic
blocks into a nearby region with inline coverage-recording instructions; only
target modules are instrumented. Coverage becomes a **bitmap** (DATA, not
CONTROL), ~15–20% overhead, pure usermode. This is the "fast mode" that stays
*binary-only* (no recompile) — the one gap fast mode (§3.4) leaves. Biggest
engineering lift here; consider wrapping TinyInst rather than writing a rewriter.

---

## 5. What *not* to pursue

- **Hardware debug registers (DR0–DR3):** only 4 per thread; useless for
  thousands of not-yet-hit addresses.
- **Guard pages / PAGE_GUARD:** 4 KB granularity — too coarse for line/branch.
- **BTS (Branch Trace Store):** ~40× slowdown, no Windows usermode API.
- **LBR (Last Branch Record):** sampled (16/32 records); can confirm "ran at
  least once", can **never** prove "never ran". A hot-path hint, not a coverage
  source.
- **Intel Processor Trace:** the headline data-driven path on paper (2–20%
  overhead, `ipt.sys` + winipt on Windows, libxdc decode), but **Intel-only** —
  AMD has no equivalent, and ARM is a plausible future target. Dropped to keep
  the tool architecture-agnostic; compiler instrumentation (§3.4) delivers the
  portable version of the same idea.

---

## Appendix — key references

- mesos (millions of one-shot breakpoints): https://github.com/gamozolabs/mesos
- TinyInst / LiteCov (selective rewriting): https://github.com/googleprojectzero/TinyInst
- SanitizerCoverage: https://clang.llvm.org/docs/SanitizerCoverage.html ·
  MSVC `/fsanitize-coverage`: https://learn.microsoft.com/en-us/cpp/build/reference/fsanitize-coverage
- raw_pdb (fast DIA-free PDB reader): https://github.com/MolecularMatters/raw_pdb
- CodeView `DEBUG_S_LINES` / C13 format: https://github.com/PascalBeyer/PDB-Documentation
- drcov format + Lighthouse: https://www.ayrx.me/drcov-file-format/ ·
  https://github.com/gaasedelen/lighthouse
- Vectored Exception Handling: https://learn.microsoft.com/en-us/windows/win32/debug/vectored-exception-handling
- "Full-speed Fuzzing" overhead numbers (S&P 2019): https://arxiv.org/abs/1812.11875
