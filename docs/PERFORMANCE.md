# Performance and collection engines

How this fork collects coverage, how fast it is, and why. Every engine and mode
below produces the **same coverage** — they are different *collection
mechanisms*, not different measurements — so you can pick the fastest one your
situation allows without changing your reports.

> This document is the reference for the performance work, which is complete —
> it supersedes the planning roadmap that drove it. The parked binary/RE-tool and
> fuzzer direction lives separately in
> [RE_TOOLS_ROADMAP.md](RE_TOOLS_ROADMAP.md).

## The three costs

A coverage run's time is governed by three levers:

1. **Startup cost** — parsing debug info and planting probes before the program
   runs.
2. **Number of first-hits** — how many probes must fire at least once.
3. **Cost per first-hit** — what one probe costs to service.

The optimizations below each attack one or more of these.

## Choosing an engine

| Engine / mode | Flag | Best for | Recompile? |
|---|---|---|---|
| Debugger + run merging | *(default)* | general use; child processes, branch coverage | no |
| In-process VEH | `--experimental_veh` | hit-heavy runs (many distinct executed probes) | no |
| Compiler-instrumentation fast mode | `--input_fastcov` | lowest overhead, CI you own the build of | yes |

All three are byte-for-byte validated against the debugger.

## Measured results

Worst cases are 100k-line programs; Release build, warm PDB cache unless noted.

| Scenario | Baseline | Now | How |
|---|--:|--:|---|
| 5,000-header startup ("skip") | 4,731 ms | ~240 ms | single-pass DIA + PDB cache + run merging |
| 100k lines full run, 1 file | 4,990 ms | 290 ms | run merging |
| 100k lines full run, 5,000 files | 5,850 ms | 880 ms | run merging |
| 100k *distinct executed* probes | 5,500 ms | 1,060 ms | VEH (per-hit ~50 µs → ~µs) |
| any of the above, if you can recompile | — | near-native | fast mode |
| 30-shard `.cov` merge (5,000 files each) | 1,360 ms | 655 ms | columnar `.cov` v2 + bitmap-OR merge |
| cold startup, 8 import DLLs | ~130 ms | ~115 ms | `--preparse_imports` |

## Startup (lever 1)

- **Single-pass debug-info enumeration.** The dominant startup cost used to be a
  DIA `findLines` query issued *per selected source file* (O(files) COM calls).
  It is now one `findLinesByRVA` query over the whole image, bucketed by source
  file name. **5,000 headers: 4,731 → 745 ms.**
- **PDB parse cache** (`--no_pdb_cache` to disable). Parsed line tables are
  cached in memory and on disk under `%LOCALAPPDATA%\OpenCppCoverage\PdbCache`,
  keyed by the PDB's `(GUID, age)` — the linker's exact identity, read from the
  PE debug directory without DIA. A relink changes the key, so the cache can
  never return stale data. The cached table holds *raw* PDB names and *all*
  files, so source selection and `--substitute_pdb_source_path` apply at replay
  time and one table is valid under any filter. **Warm startup 745 → ~240 ms.**
- **DIA-free raw PDB reader** (`--no_raw_pdb` to force DIA). The default parse
  path memory-maps the PDB and scans the CodeView streams directly, instead of
  paying DIA's ≥6 COM calls per line record. Because those streams are lock-free
  immutable memory, it parses each compiland **in parallel**. It produces the
  same line table as DIA (validated byte-for-byte); DIA remains the fallback for
  `/DEBUG:FASTLINK`, a missing/mismatched PDB, and DIA-only data (optimized-build
  symbol indexes, fast-mode demangled names). This attacks the *cold* parse the
  cache only hides on a warm run — the first run of every CI job. **Cold parse:
  5,000 headers 434 → 244 ms; the parse component itself ~4×.**
- **Import-closure pre-parse** (`--preparse_imports`, opt-in). Before the target
  starts, the executable and its recursive static-import-closure DLLs (the app's
  own DLLs, read from the PE import tables) are parsed **in parallel** into the
  line-table cache, so each module then loads with a cache hit instead of a
  per-module parse stalling the single-threaded debug loop. It parallelizes work
  that is otherwise serial *across* modules. **Cold startup ~11 % on an 8-DLL
  target;** the win scales with the number and size of import DLLs. Opt-in
  because the payoff is target-dependent and it warms PDBs a filtered run might
  not need.

## Fewer, cheaper traps (levers 2 & 3, binary-only)

- **Run merging — block-leader probes** (default; `--no_line_merging` opts out).
  A breakpoint on the first instruction of a straight-line run proves execution
  of every line the run touches, so the number of debug-event round trips — the
  dominant cost of a run — drops by up to an order of magnitude. It is a *sound*
  transform, not an approximation: runs end at every branch, return, interrupt
  **and call** (a callee can throw, keeping exception paths exact), never span
  functions, and functions with an indirect jump fall back to per-line, so output
  is exactly identical to per-line breakpoints. Disabled automatically under
  `--optimized_build`. **Full run 4,990 → 290 ms (1 file, 17×); 5,850 → 880 ms
  (5,000 files, 6.6×).**
- **In-process VEH servicing** (`--experimental_veh`). Instead of a debugger, the
  target is launched suspended, a tiny kernel32-only helper DLL is injected, and
  a Vectored Exception Handler services each breakpoint *inside* the process — a
  usermode exception dispatch (~µs) instead of the cross-process debug-event
  round trip (~50 µs per first hit). All probe placement is computed offline from
  the PDB and module file (reusing the same enumeration, filters, run merging and
  probe table), so it is a different collection mechanism, not a different
  measurement — **byte-identical to the debugger**. **Hit-heavy 100k probes:
  4,650 → 810 ms (1 file, 5.8×); 5,500 → 1,060 ms (5,000 files, 5.2×);** the win
  scales with the number of distinct executed probes and composes with run
  merging. The VEH engine covers the main executable, filter-matching DLLs
  (patched as they load via `LdrRegisterDllNotification`), **branch coverage**
  (in-process trap-flag single-step re-arm), and **child processes** (an
  in-process `CreateProcess` hook force-suspends each child so the host can
  inject the helper; recursive across the whole process tree). 64-bit only.

## Coverage-as-data — compiler-instrumentation fast mode (recompile)

For the lowest possible overhead, build the target with SanitizerCoverage
`inline-8bit-counters`: each basic block becomes one `inc byte [counter]` — no
trap, no kernel, no per-hit round trip — and a freestanding runtime dumps the
counters at exit, imported with `--input_fastcov`. It is a *mode*, not the
default (it needs the recompile), and is architecture-agnostic (the same flags
target x64 and ARM64 — the portable answer to what Intel PT would have given,
without the vendor lock-in). See
[CoverageRuntime/README.md](../CoverageRuntime/README.md) for build/run steps.

- **Line-exact via control-flow reconstruction.** SanitizerCoverage uses
  *minimal* instrumentation — a block whose execution is implied by another
  carries no counter. The importer rebuilds each function's CFG from the
  disassembly and propagates coverage from the instrumented subset (entry
  dominates all; a covered block's single successor and single predecessor also
  ran). Sound for normal flow, never over-reports. **Verified line-identical to
  the debugger** on simple and complex control flow at `/Od` and `/O2`.
- **Both toolchains.** clang-cl (link with **lld**) and MSVC `cl.exe` are
  supported; they feed the same CFG reconstruction and line mapping.
- **Multi-module.** The runtime dumps every loaded module carrying instrumentation
  (the executable and instrumented DLLs), not just the main executable.
- **Template instantiation axis.** Each block is attributed to its containing
  function (the instantiation) via the demangled PDB name.
  `--fastcov_template_report` emits, per template, each instantiation's line
  coverage **and the lines covered in some but not all instantiations** — a gap a
  single boolean per line hides (a line run for `foo<int>` but never for
  `foo<double>`).

## Coverage aggregation — columnar `.cov` v2

The binary `.cov` format (`--export_type binary`, `--input_coverage`) is
columnar: per (module, file) it stores line numbers as delta-varints and a packed
`executed` bitmap behind an interned path table, plus a sparse branch side-table.
Merging shards runs as a **bitmap-OR reduce** — same-build shards share an
identical line array, so the merge is a word-wise OR of the bitmaps — instead of
rebuilding map-of-map hierarchies. Legacy protobuf `.cov` files are still read
(detected by a magic byte). **30 shards of 5,000 files: merge 1,360 → 655 ms
(~2×), file size −35 %.**

## Parallelism — what paid off, and what didn't

Four places in the pipeline were measured for parallelism. The rule that emerged
saved effort repeatedly: **parallelism pays only where the work is CPU-bound —
not where it is bound by memory allocation or I/O.** Under the MSVC heap lock,
spreading allocations across threads just moves the contention.

**Wins (CPU-bound):**
- **PDB compiland parsing** (raw_pdb) — CPU-bound CodeView decode, parsed across
  compilands with the parallel STL. The one unqualified win.
- **Import-closure pre-parse** (`--preparse_imports`) — parallelizes the
  otherwise-serial parse *across* modules. Modest, target-dependent.

**No-ops (allocation/I/O-bound), tried and reverted:**
- **HTML export** — a wash-to-regression even on 32 cores: bound by string
  allocation (page buffers on the heap lock) and disk writes, not CPU. The kept
  win there is a thread-local `ctemplate` template cache (avoids ctemplate's
  global mutex per file), unrelated to parallelism.
- **`.cov` shard merge** — a wash-to-regression (30 shards, 671 vs 655 ms
  serial): building each shard's per-file data is thousands of small allocations
  on the heap lock. The serial columnar merge shipped instead.

**Corollaries:**
- The `WaitForDebugEvent` loop is single-threaded by OS contract — parallelize
  the work *around* it (offline probe computation, the pre-parse), never the loop.
- When parallelism *is* warranted, use the parallel STL
  (`std::for_each(std::execution::par, …)`). `stdexec` was assessed and rejected
  — overkill for fork-join, and MSVC is its weakest-supported compiler.

## Deliberately not pursued

- **Hardware debug registers (DR0–DR3):** only 4 per thread; useless for
  thousands of not-yet-hit addresses.
- **Guard pages / PAGE_GUARD:** 4 KB granularity — too coarse for line/branch.
- **BTS (Branch Trace Store):** ~40× slowdown, no Windows usermode API.
- **LBR (Last Branch Record):** sampled; can confirm "ran at least once", never
  prove "never ran". A hot-path hint, not a coverage source.
- **Intel Processor Trace:** attractive on paper (2–20 % overhead) but
  **Intel-only** — AMD has no equivalent, ARM is a plausible future target.
  Dropped to keep the tool architecture-agnostic; compiler-instrumentation fast
  mode delivers the portable version of the same idea.
- **TinyInst-style binary rewriting:** does not speed up boolean coverage (the
  VEH engine already retires probes to zero steady-state cost); its only payoff
  is binary-only per-block hit counts for fuzzers — see
  [RE_TOOLS_ROADMAP.md](RE_TOOLS_ROADMAP.md).

## Remaining gap

- **Fast-mode ARM64 validation** — the flags and CFG reconstruction are
  arch-agnostic, but the disassembly (Zydis, x86) and the MSVC `inc byte
  [rip+disp]` recognition are x64-specific. Needs ARM64 Windows hardware to
  validate; not otherwise blocked.

## References

- raw_pdb (DIA-free PDB reader): https://github.com/MolecularMatters/raw_pdb
- SanitizerCoverage: https://clang.llvm.org/docs/SanitizerCoverage.html ·
  MSVC `/fsanitize-coverage`: https://learn.microsoft.com/en-us/cpp/build/reference/fsanitize-coverage
- Zydis disassembler: https://github.com/zyantific/zydis
- CodeView `DEBUG_S_LINES` / C13 format: https://github.com/PascalBeyer/PDB-Documentation
