# Performance roadmap

Further optimization opportunities, prioritized by impact / effort, grounded in
the current architecture and the in-tree micro-benchmarks (`Benchmarks.exe`).
It complements [PERFORMANCE.md](PERFORMANCE.md), which documents the engines and
formats already in place.

## Baseline (msvc-release, 24-core / 4.7 GHz, warm file cache)

| Benchmark | Time | Notes |
|---|---|---|
| `ParsePdbRawPdb` | 2.3 ms | cold raw_pdb parse of CppCoverage.dll (161 source files) |
| `MergeCoverageData` | 3.1 ms | 3 modules x 20 files x 500 lines = 30k lines |
| `MergeColumnarShards/30` | 15.5 ms | ~0.5 ms per same-build shard |
| `SerializeAndDeserializeCoverageData` | 3.5 ms | columnar `.cov` round-trip |
| `ExportCobertura` | 18.4 ms | full document, the fixture dataset |
| `CoberturaLine_{Legacy,Fmt}` | 42 / 10 ms | 100k `<line/>` elements, baseline vs fmt |
| `FindConditionalBranches/100000` | 14.4 ms | ~70 MiB/s Zydis decode |

Where wall-clock actually goes on a real run: **cold start** is dominated by
per-module PDB parsing; **steady state** by breakpoint servicing (debug-event
round trips, already cut by run merging and the VEH engine); **finish** by
export formatting. The items below target each in turn.

## Tier 1 — high impact, contained effort

### 1. Store line coverage in a sorted vector, not `std::map`
`Plugin::FileCoverage` keeps lines in `std::map<unsigned int, LineCoverage>`
(`FileCoverage.hpp:52`) — a red-black tree with a heap node per line and
pointer-chasing on every insert/lookup. Coverage is built once and read in
order, the ideal case for a sorted `std::vector` (or `boost::container::
flat_map`, already a dependency). Expect a solid cut to `MergeCoverageData`
and every export (all of which iterate lines), plus lower memory and better
cache behaviour on large modules.
- **Effort:** medium (touches the plugin ABI — coordinate with the version bump).
- **Measure:** `MergeCoverageData`, `ExportCobertura`.

### 2. Format the Cobertura document without `wostringstream`
The exporter writes structure through a `std::wostringstream` with an imbued
locale (`CoberturaExporter.cpp:77-78`) and only formats the hot integer
attributes with `fmt::format_to`. The benchmark shows a full `fmt` line path is
~4x the legacy stream path; extending that to the whole element (one
`fmt::format_to` into a reused `std::wstring`, no per-attribute stream
insertions, no `Escape` allocation for values that can't contain markup) should
move `ExportCobertura` toward the `CoberturaLine_Fmt` envelope.
- **Effort:** low–medium.
- **Measure:** `ExportCobertura`, `CoberturaLine_*`.

### 3. Emit reports as UTF-8 directly
The Cobertura path round-trips through `Tools::ToUtf8String` /
`LocalToWString` (`CoberturaExporter.cpp:87-88`) and builds UTF-16 that is
converted on write. Coverage identifiers are ASCII-dominant; formatting into a
UTF-8 (`char`) buffer end-to-end removes the wide-buffer and the conversion.
Applies to the HTML exporter too.
- **Effort:** medium. **Measure:** add an HTML-export benchmark (see gap below).

## Tier 2 — high impact, higher effort or narrower scope

### 4. Faster branch disassembly
`--branch_coverage` decodes every covered line with
`ZydisDecoderDecodeInstruction` at full detail (`BranchDisassembler.cpp:109`).
Only the mnemonic class (is-this-a-Jcc) and length are needed. Zydis's reduced
decode / `ZydisDisassemble*` fast paths, or `ZYDIS_DECODER_MODE` minimal
flags, should raise the ~70 MiB/s throughput materially. Only matters for
branch coverage on large modules, but it is a self-contained win.
- **Validate first:** confirm the lighter decode still reports Jcc + length
  correctly against the existing `FindConditionalBranches` output.

### 5. Coalesce breakpoint writes
Run merging already cuts the *number* of breakpoints; installation still writes
them one address at a time. Batching the `0xCC` pokes per straight-line run
(one `WriteProcessMemory` over a coalesced range, or a scatter list) cuts
syscalls at instrumentation time. Largest benefit on hit-once, install-heavy
runs where the target executes briefly.
- **Effort:** medium. **Measure:** needs the engine harness
  (`run_engine_benchmark.ps1`), not a micro-benchmark.

### 6. Branch re-arm without the trap flag
The VEH engine re-arms a serviced branch by single-stepping via the trap flag
(one extra exception per branch hit). A temporary breakpoint on the following
instruction (removed once past) avoids the single-step exception on hot
branches. Trickier under threads; gate behind measurement.

### 7. Overlap PDB parse with process startup
The target is created suspended for injection anyway; the import-closure
pre-parse (`--preparse_imports`) is opt-in. Parsing the main module's PDB (and
known imports) on a worker while the loader runs would hide most of the 2.3 ms
per-module parse behind work that happens regardless. Consider making
pre-parse the default when the import count crosses a threshold.

## Tier 3 — build-level wins for the tool itself

Now that clang-cl builds cleanly, the tool binary can be optimized further
without touching source:
- **ThinLTO** (clang-cl + `lld-link`) across the DLLs — cross-module inlining
  the current per-DLL `/GL` cannot do. Add behind an `OCC_CLANG_THINLTO` option.
- **PGO**: instrument a clang-cl build, profile a representative coverage run,
  rebuild optimized. The debug loop and PDB parser are branch-heavy and would
  benefit.
- **BOLT** post-link layout on the hot debug-event path.

These are measurable end-to-end with `run_engine_benchmark.ps1`.

## Prerequisite: verifiable high-performance paths

Two of the fastest paths are under-tested (see the test-matrix analysis), which
blocks confident optimization — you cannot safely tune what has no equivalence
check:
- **FastCov** has *no* debugger-parity test; `FastCoverageImporterTest` only
  checks report formatting. Add an end-to-end "fastcov == debugger" test before
  optimizing the fast-mode path.
- The **VEH** parity matrix (`VehCoverageEngineTest`) covers basic, threads,
  handled exceptions, branch, child/nested processes and DLLs, but not
  optimized-build, multi-module, unload/reload, or unhandled exceptions. Close
  these before items 5–6.

Land the parity tests first; then each optimization above can be gated on "still
matches the debugger, and faster."
