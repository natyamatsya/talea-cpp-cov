# Performance roadmap

Further optimization opportunities, prioritized by impact / effort, grounded in
the current architecture and the in-tree micro-benchmarks (`Benchmarks.exe`).
It complements [PERFORMANCE.md](PERFORMANCE.md), which documents the engines and
formats already in place.

## Baseline (msvc-release, 24-core / 4.7 GHz, warm file cache)

> Re-measured on an **otherwise idle** machine (10 repetitions,
> `--benchmark_report_aggregates_only`, medians below; coefficient of variation
> under ~2% for every row except `SerializeAndDeserialize` at ~4%). These are
> the decisive absolute figures and supersede the earlier under-load capture;
> they confirm every optimization delta recorded in the items below.

| Benchmark | Time | Notes |
|---|---|---|
| `ParsePdbRawPdb` | 2.00 ms | cold raw_pdb parse of CppCoverage.dll (160 source files) |
| `MergeCoverageData` | 1.43 ms | 3 modules x 20 files x 500 lines = 30k lines (was 3.1 ms; Tier 1 #1) |
| `MergeColumnarShards/30` | 12.6 ms | ~0.42 ms per same-build shard (was 15.5 ms; Tier 1 #1) |
| `SerializeAndDeserializeCoverageData` | 2.15 ms | columnar `.cov` round-trip (was 3.5 ms; Tier 1 #1) |
| `ExportCobertura` | 7.30 ms | full document, the fixture dataset (was 18.4 -> 9.3 ms; Tier 1 #1, #2) |
| `CoberturaLine_{Legacy,StdFormat,Fmt}` | 39.6 / 9.70 / 9.01 ms | 100k `<line/>` elements: baseline vs `std::format` vs fmt |
| `FindConditionalBranches/100000` | 11.3 ms | ~93 MiB/s Zydis decode (minimal mode; Tier 2 #4) |

Where wall-clock actually goes on a real run: **cold start** is dominated by
per-module PDB parsing; **steady state** by breakpoint servicing (debug-event
round trips, already cut by run merging and the VEH engine); **finish** by
export formatting. The items below target each in turn.

## Compiler comparison: MSVC (`cl`) vs `clang-cl`

Both release presets produce the same fully-static executable; only the compiler
differs. Measured on the idle baseline machine (10 repetitions, medians, all CVs
< 1.6%). Δ is `clang-cl` relative to MSVC — negative means `clang-cl` is faster.

| Benchmark | MSVC | clang-cl | Δ | faster |
|---|---:|---:|---:|---|
| `FindConditionalBranches/100000` (Zydis decode, minimal mode) | 11.3 ms | 10.1 ms | −11% | clang-cl |
| `ParsePdbRawPdb` | 2.12 ms | 1.94 ms | −8% | clang-cl |
| `ExportCobertura` | 7.32 ms | 5.44 ms | −26% | clang-cl |
| `CoberturaLine_Fmt` (fmt) | 9.56 ms | 7.50 ms | −22% | clang-cl |
| `CoberturaLine_StdFormat` (`std::format`) | 9.72 ms | 8.85 ms | −9% | clang-cl |
| `CoberturaLine_Legacy` (`wostream`) | 34.1 ms | 33.4 ms | −2% | ~tie |
| `MergeCoverageData` (`flat_map`) | 1.48 ms | 1.67 ms | +13% | MSVC |
| `MergeColumnarShards/30` | 12.9 ms | 15.6 ms | +21% | MSVC |
| `SerializeAndDeserializeCoverageData` | 2.25 ms | 2.45 ms | +9% | MSVC |

The split is systematic, not noise:

- **`clang-cl` wins the compute / codegen-bound loops** — Zydis instruction
  decode (branch disassembly, ~11% after the minimal-mode change of Tier 2 #4;
  it was ~19% when both compilers ran the full semantic pass, so MSVC gained
  more from the switch), string/number formatting (Cobertura export +26%, `fmt`
  line emission +22%), and raw byte parsing (raw_pdb +8%). Tight loops where
  LLVM's backend and vectorizer produce better code.
- **MSVC wins the MSVC-STL-container-heavy work** — `std::flat_map` merge (+13%),
  columnar shard merge (+21%), serialize (+9%). These lean entirely on
  Microsoft's own STL (`std::flat_map`, `std::vector`, hashing); `cl`'s codegen
  is tuned for its STL's inlining and container layout, and `clang-cl` consuming
  the same headers is slightly less optimal.

**Implication.** A typical single-target run is dominated by PDB parse → breakpoint
servicing → export; `clang-cl` is faster on the parse and export stages (and on
branch disassembly), so it tends to edge ahead there. MSVC pulls ahead only for
heavy multi-shard aggregation (`--input_coverage`), where the merge paths
dominate. Neither compiler is a clean overall winner — it depends on the
workload. The switch to a fully static (`/MT`) build was performance-neutral for
this in-process work: msvc-release medians are within noise of the earlier
dynamic-CRT build.

## Tier 1 — high impact, contained effort

### 1. Store line coverage in `std::flat_map`, not `std::map` — DONE
`Plugin::FileCoverage` kept lines in a `std::map<unsigned int, LineCoverage>`
(red-black tree, a heap node per line, pointer-chasing on every access).
Coverage is built once and read in order, the ideal case for a flat container,
so it now uses `std::flat_map` (C++23, MSVC STL). Contiguous keys/values give
better cache behaviour and lower per-line overhead; measured (5-rep aggregates,
`std::map` → `std::flat_map`):
- `MergeCoverageData` 3.1 → 1.47 ms (~2.1x)
- `ExportCobertura` 9.3 → 7.6 ms (~1.2x, on top of the fmt rewrite)
- `SerializeAndDeserializeCoverageData` 3.5 → 2.5 ms (~1.4x)
- `MergeColumnarShards/30` 15.5 → 12.8 ms (~1.2x)

`FileCoverage` is exported from `Plugin.dll`, so this changed the layout; the
plugin ABI version was bumped to 3. **Caveat:** `flat_map` insertion is O(n),
so building is cheap only when lines arrive in (roughly) increasing order — the
common case for PDB-driven collection. If a future path adds lines out of
order, batch into a vector and use the sorted-range constructor rather than
per-line `emplace`.

### 2. Format the Cobertura document without `wostringstream` — DONE
The exporter previously wrote every element through a `std::wostream` via
`operator<<`, paying the stream's locale/codecvt and stream-buffer dispatch on
each insertion. `XmlWriter` now builds the whole document in a reused
`std::wstring` (integers via `fmt::format_to`, escaping appended in place) and
writes it once. Output is byte-for-byte identical (golden-file test);
`ExportCobertura` dropped from **18.4 ms to 9.3 ms** (~2x), at the
`CoberturaLine_Fmt` envelope. The same buffer-then-write approach applies to
the HTML exporter next.

### 3. Emit reports as UTF-8 directly
The Cobertura path round-trips through `Tools::ToUtf8String` /
`LocalToWString` (`CoberturaExporter.cpp:87-88`) and builds UTF-16 that is
converted on write. Coverage identifiers are ASCII-dominant; formatting into a
UTF-8 (`char`) buffer end-to-end removes the wide-buffer and the conversion.
Applies to the HTML exporter too.
- **Effort:** medium. **Measure:** add an HTML-export benchmark (see gap below).

## Tier 2 — high impact, higher effort or narrower scope

### 4. Faster branch disassembly — DONE
`--branch_coverage` decoded every covered line with
`ZydisDecoderDecodeInstruction` running Zydis's full semantic-analysis pass,
when only the mnemonic (to classify the Jcc) and the instruction length are
needed. The decoder now runs in `ZYDIS_DECODER_MODE_MINIMAL` (mnemonic +
length, no category/flags/operands/ISA metadata), and the branch classifier
relies on the mnemonic alone: `GetBranchCondition` already returns a value only
for the conditional-jump mnemonics, so the previous
`meta.category == ZYDIS_CATEGORY_COND_BR` guard was redundant (and category is
not decoded in minimal mode). `cmov`/`setcc` and `LOOP*` stay excluded, as
before. Measured (`FindConditionalBranches/100000`, 10-rep medians, idle):
- MSVC: 13.4 → **11.3 ms** (~75 → ~93 MiB/s, ~16% faster).
- clang-cl: 10.9 → **10.1 ms** (~105 MiB/s, ~7% faster — its full decode was
  already tighter, so it had less semantic overhead to shed).

Correctness is covered by `BranchDisassemblerTest` (short/long Jcc, length,
32/64-bit modes, and — critically — that `cmove`/`setcc` are *not* reported,
which validates dropping the category guard) and the end-to-end branch-coverage
tests, all green across the four-config matrix.

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

## Tier 4 — dependency cleanup

### Drop the boost dependency — DONE
The audit found that boost had already receded to two live uses (everything else
— `boost::log`, `boost::regex`, `boost::algorithm`, `boost::program_options`,
`boost::property_tree`, `boost::spirit`, `boost::uuids` — was replaced in
earlier work and survives only as historical comments). Both are now gone, so
**the project no longer depends on boost at all**:

- `boost::range` (`boost::count_if` in `CodeCoverageRunnerTest`) → classic
  `std::count_if(begin, end, pred)`. `boost::count_if` was itself only a
  `begin/end` wrapper over `std::count_if`, so this is the exact 1:1 replacement;
  the `std::ranges` overload was deliberately avoided — its iterator wrapping can
  be slower, markedly so under MSVC's checked iterators in Debug.
- `boost::container::flat_map` / `flat_set` → `std::flat_map` / `std::flat_set`.
  Done in the line-coverage store (Tier 1, item 1).
- `boost::container::small_vector<std::uint32_t, 1>` in `ExecutedAddressManager`
  → **vendored `gch::small_vector`** (Gene Harvey, MIT; a single header under
  `SmallVector/gch/`). There is no standard small-buffer-with-spill vector —
  `std::inplace_vector` (C++26) is fixed-capacity and cannot spill to the heap,
  so it does not cover the multi-probe case — and the inline capacity of 1 saves
  a heap allocation on the common one-probe-per-address path, of which there can
  be hundreds of thousands, so a plain `std::vector` would regress the hot path.
  A header-only vendored library keeps that optimization without any package
  dependency. There is no vcpkg port for it, so it is vendored like `RawPdb` and
  `Zydis`.

Net effect: `boost-range` and `boost-container` are both removed from
`vcpkg.json`; the `occ_boost` CMake interface (which also pulled in `bcrypt`, a
transitive-only boost dependency) is deleted; `find_package(Boost ...)` is gone.
The vcpkg build no longer fetches or builds any boost package. Revisit only if a
standard small-vector with heap spill lands, at which point the vendored header
can be dropped for the STL type.

## Prerequisite: verifiable high-performance paths

The fastest paths were under-tested (see the test-matrix analysis); the gaps
are now mostly closed, so optimization can be gated on an equivalence check:
- **FastCov** now has an end-to-end "fastcov == debugger" parity test
  (`FastCoverageImporterTest.MatchesDebugger`): it runs the instrumented
  `TestFastCovConsole` fixture, imports the dump and compares to the debugger
  over the same binary. It is MSVC-only (the clang-cl fast-mode runtime needs
  lld) and skips under clang-cl; it runs in both Debug and Release.

  This test initially failed in the Release preset (empty import), which turned
  out to be instructive: `inline-8bit-counters` are emitted as a zero-init
  (BSS-like) `.sancov` section with `SizeOfRawData = 0`, and a Release link
  *without* `/DEBUG` turns on `/OPT:REF`, which trims that counter region (its
  `VirtualSize` collapsed from `0x319` to `0x8`) so the runtime read a
  truncated array. Linking the instrumented target with `/DEBUG /OPT:NOREF`
  restores it. **Real-world implication:** a target compiled with
  `/fsanitize-coverage` for fast mode must be linked so the linker does not
  prune `.sancov` — worth calling out in the CoverageRuntime docs, and the
  fast-mode runtime would be more robust sizing the counter array from the
  sanitizer boundary/init-callback bounds rather than the PE section's
  `VirtualSize`.
- The **VEH** parity matrix (`VehCoverageEngineTest`) now also covers
  multi-module and unhandled access violations. Extending it surfaced two real
  VEH divergences from the debugger, both reproduced with the CLI
  (`--experimental_veh`) and captured as `DISABLED_` parity tests to enable
  once fixed:
  - **Unload / reload of a DLL**: VEH does not re-instrument a module after it
    is unloaded and loaded again, so the reloaded code reads as uncovered.
  - **Optimized build**: VEH diverges on optimized code where line records
    share addresses (e.g. over-reporting a line that shares an address with an
    executed one).
  A third difference is benign: after an unhandled C++ exception the debugger
  reports the exception code as the exit code while VEH reports the CRT abort
  code — coverage matches, only the exit code differs.

Land the parity tests first; then each optimization above can be gated on "still
matches the debugger, and faster."

## Known clang-cl limitations (coverage of clang-cl-built targets)

Two coverage behaviours differ when the *profiled target* is built with
clang-cl rather than cl. They are not build-system issues and are tracked here
because closing them is real feature work; the corresponding tests are skipped
on clang-cl with these reasons.

1. **Exception-handler funclets are not covered.** clang-cl emits `catch`
   blocks as separate funclets invoked by the unwinder; the debugger line
   instrumentation does not record execution inside them, so a `catch` body
   reads as uncovered even though it runs (cl's handler code is covered).
   Verified by giving a `catch` a real statement, which is also reported
   uncovered. Fixing it means instrumenting clang-cl's EH funclet code ranges.
2. **`--continue_after_cpp_exception` cannot resume.** The feature works by
   letting execution fall through the throw site after an unhandled C++
   exception. cl leaves a valid instruction there; clang-cl marks
   `__CxxThrowException` `noreturn` and emits no continuation, so resuming runs
   into invalid code. Supporting it on clang-cl would require explicitly
   redirecting the instruction pointer to a safe continuation, which the throw
   site does not provide.
