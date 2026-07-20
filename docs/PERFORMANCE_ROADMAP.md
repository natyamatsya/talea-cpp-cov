# High-Performance Code Coverage — a Roadmap

*An ambitious plan for making OpenCppCoverage fast on large translation units,
ordered low-hanging fruit first, with a data-oriented design (DOD) throughline.*

> Status: **Tier 0 shipped** (commits on `main`, July 2026); Tiers 1–4 are a
> design menu with measured/estimated payoffs. This is a private, experimental
> fork — the later tiers are aspirational. This is the single canonical roadmap;
> it consolidates two earlier drafts.

---

## 1. Where the time actually goes (measured)

Synthetic TUs, Release x64, this machine. "skip" = program instrumented but
exits immediately (startup cost only); "full" = every line executed once.

| Lines | native | startup only | all lines hit |
|------:|-------:|-------------:|--------------:|
| 10k   |  98 ms |       102 ms |        626 ms |
| 50k   | 125 ms |       249 ms |      2,391 ms |
| 100k  | 184 ms |       471 ms |      5,040 ms |
| 200k  | 303 ms |       922 ms |     10,312 ms |

**Startup is dominated by the number of selected _source files_, not lines.**
Same 100k lines: 471 ms in one file, **4,731 ms across 5,000 headers**; with only
one of those 5,000 files selected, 94 ms. That is the templated / unity-build
worst case, and it repeats **per module and per child process** (no PDB cache).

**Runtime is ~50 µs per first-executed line, perfectly linear.** Each first hit
is one `WaitForDebugEvent` → `GetThreadContext` → `WriteProcessMemory` →
`ContinueDebugEvent` cross-process round trip. External anchor: mesos measures
~9,934 breakpoint hits/sec (~100 µs each) on real hardware; planting the `0xCC`
bytes, by contrast, runs at millions/sec — **setup is free, the round trip is
everything.**

So there are exactly three levers:

1. **Startup**: stop doing O(files × compilands) DIA work per module per process.
2. **Number of first-hits**: instrument fewer probes (basic-block leaders, not
   every line) — a leader hit implies every line in its straight-line block ran.
3. **Cost per first-hit**: make servicing a hit cheaper than a ~100 µs
   cross-process debug event, or stop trapping entirely (coverage-as-data).

Everything below maps to one of these.

---

## 2. The DOD lens

Two axes frame the whole design space:

- **CONTROL vs DATA.** A *control*-oriented mechanism pays a trap/exception per
  event (today: one debug event per line). A *data*-oriented one lets execution
  write into a buffer/bitmap/counter array at near-native speed and decodes it
  later. Data wins on throughput by orders of magnitude.
- **Binary-only vs recompile.** OpenCppCoverage's value is instrumenting binaries
  you *didn't* build specially. Compiler instrumentation is the throughput
  ceiling but forfeits that property. Most of this roadmap keeps binary-only.

The internal-architecture work (Tiers 0–2) is pure DOD applied to our own data
structures: replace pointer-chasing `std::map` hierarchies and per-line
`std::string`/allocations with columnar, interned, immutable, bulk-scanned data.
The collection-mechanism work (Tiers 3–4) is about moving from CONTROL toward
DATA.

---

## 3. Tier 0 — Low-hanging fruit (SHIPPED, no architecture change)

Safe, local, individually shippable fixes straight out of the profiling.
**Status: done and pushed to `main`, with measured results.**

| Fix | File(s) | Result |
|---|---|---|
| ✅ **Skip `findSymbolByVA` per line record** unless `--optimized_build` is on (its `symIndex` result is consumed *only* by `ReleaseCoverageFilter`). | `DebugInformationEnumerator.cpp` | Removes the most expensive per-record COM query for the common debug-build case. |
| ✅ **`LineFilter` empty-regex early-out** — return `true` immediately when no `--excluded_line_regex` was given, instead of memory-mapping and line-splitting every selected source file. | `FileFilter/LineFilter.cpp` | Part of the 5,000-header startup drop below. |
| ✅ **Set `_NO_DEBUG_HEAP=1`** in the debuggee environment. A debugger-created process otherwise gets the slow NT debug heap. | `CppCoverage/Process.cpp` | Debug-heap penalty gone; allocation-heavy debuggees run near-native under coverage. |
| ✅ **Streaming Cobertura writer** replacing `boost::property_tree` (byte-identical output). | `Exporter/CoberturaExporter.cpp` | Export **~140 ms → 22.6 ms (6.2×)** in the benchmark; also drops the boost-property-tree dependency. |
| ✅ **Reuse the `wostringstream`** in the debug-event handler instead of constructing (and re-imbuing the locale on) a fresh one per event. | `CodeCoverageRunner.cpp` | Shaves a constant off every hit. |
| ✅ **`CONTEXT_CONTROL` instead of `CONTEXT_ALL`** on the breakpoint-adjust path. | `BreakPoint.cpp` | Smaller Get/SetThreadContext in the hot loop. |
| ✅ **Drop `FlushInstructionCache`** on the patch path — a no-op syscall on x86/x64 (hardware I/D coherency). | `Tools/ProcessMemory.cpp` | Removes a kernel transition per breakpoint write. |
| ⏳ **Drop the per-event `RemoveAddressLineIf` O(n) scan**; index addresses per (process, module) so unload/exit erases a bucket. | `ExecutedAddressManager.cpp` | O(M×A) → O(A) on module churn. *(Not yet done — folds naturally into the Tier 1 data model.)* |

**Measured effect:** 5,000-header startup **4,731 ms → 3,638 ms** (~23%), export
**6.2× faster**, debug-heap penalty eliminated. All test suites green; the two
Cobertura golden files are unchanged (byte-identical output).

Also pending (minor, same spirit): guard the per-file `wostringstream` message
in `WildcardCoverageFilter` behind a log-level check (built even when the log is
filtered out), and hoist the per-line-record `LOG_TRACE` out of the registration
loop in `ExecutedAddressManager`/`BranchCoverageManager`.

The remaining ~0.9 ms-per-file startup term is *not* addressed by Tier 0 — it is
the DIA enumeration itself, tackled next.

---

## 3.5. Single-pass DIA line-table enumeration — SHIPPED

The dominant startup cost was `IDiaSourceFile::get_compilands` +
`IDiaSession::findLines(compiland, file)` issued **per selected source file** —
DIA requires a file argument to `findLines`, so one findLines call (with a fixed
per-call overhead) was paid per header, i.e. O(files) calls.

**Done:** query the whole image once with `findLinesByRVA(0, 0xFFFFFFFF)` and
bucket every line record by source file, keyed by the remapped file *name* (DIA
hands out inconsistent `get_uniqueId`s across the source-files table and a line
record's `get_sourceFile`, so the name is the stable key). Records arrive in RVA
order, so the source file is resolved once per contiguous run, not per record.

**Measured (Release, 100k lines):** 5,000 headers **4,731 ms → 745 ms**; 1 file
**471 → 317 ms**. Tradeoff: selecting a small subset of a large module now scans
all line records (5,000-header binary, 1 file: 94 → 230 ms) — still fast, and the
common "cover my whole project" case is what improves dramatically.

This sets up the Tier 2 PDB-parse cache and the eventual DIA-free raw reader
(which removes even the remaining per-record COM calls and supersedes the
select-few tradeoff).

Still open, complementary micro-op: batch `IDiaEnum*::Next(N)` instead of
`Next(1)` to cut per-record COM dispatch in the single pass.

---

## 4. Tier 1 — Data-oriented core (the structural investment)

Replace the coverage data model with columnar, index-based, immutable structures.
This is the foundation everything else hangs off.

**Progress — shipped so far:**
- ✅ **Hash the address indexes.** `std::map<Address, …>` (per-hit lookup:
  ~log₂(A) cache-missing tree-node comparisons) → `std::unordered_map` with an
  `Address` hash, for both line (`ExecutedAddressManager`) and branch
  (`BranchCoverageManager`) hits. Dropped the per-probe `LOG_TRACE`.
- ✅ **Iterate lines without copying.** `FileCoverage::GetLineCollection()`
  (const ref to the ordered map); the rate computer, Cobertura exporter, binary
  serializer, and merger no longer copy every line per file. Merge ~35% faster.
- ✅ **Columnar probe table.** `ExecutedAddressManager` rewritten to a
  structure-of-arrays probe table (parallel `moduleId`/`fileId`/`lineNumber`/
  `executed` columns indexed by a dense probe id), interned file paths, a
  contiguous byte hit bitset, and an address fan-out of probe *ids* (indices,
  stable across growth) instead of `bool*` aliasing container nodes. Behavior
  and output unchanged. **This is a structural change, not a speedup** — the
  runtime is bounded by the ~50 µs debug-event round trip per first-hit, which
  dwarfs the data-model cost, so timings are unchanged within noise. The value
  is the clean model (no aliasing pointers; a contiguous `executed` bitset a
  future `.cov` merge / rate pass can OR + popcount; interned paths).

**Still to do:** immutable per-module line tables shared across modules /
processes (Tier 2 PDB cache), arenas for the remaining scratch, and a
bitmap-based `.cov` merge that consumes the `executed` bitset directly.

### 4.1 The problem with today's model

- `map<wstring,Module>` → `unordered_map<wstring,File>` → `map<unsigned,bool>`,
  plus a parallel `map<Address,Line>` whose `Line` holds
  `small_vector<bool*>` — **raw pointers into RB-tree node payloads**. That
  pointer-into-node invariant is *why* everything must be node-based `std::map`
  (see the "Use map to have iterator always valid" comment). It forces the whole
  design.
- Per registered line: a full path `wstring` hash + a tree insert + a map-node
  allocation. Millions of probes, thousands of unique files.
- Hot-path lookup is a two-field `Address` comparison across ~20 pointer-chased,
  likely-cache-missing tree nodes.

### 4.2 The replacement

**One frozen probe table per module, Structure-of-Arrays, module-relative:**

```cpp
struct LineTable {                 // immutable after build; shared across processes
    std::vector<uint32_t> rva;     // module-relative → process-independent, sorted
    std::vector<uint32_t> fileId;  // global interned source-file id
    std::vector<uint32_t> line;
    std::vector<uint32_t> length;  // for branch/diff filters
};
struct ModuleInstance {            // per (process, load) — cheap, transient
    HANDLE process; uint64_t base; uint32_t moduleId;
    std::vector<uint8_t> origByte; // must be read per process
};
std::vector<uint64_t> hitBits;     // global bitset, 1 bit per (moduleId, probeId)
```

- **String interning.** Every normalized path becomes a `uint32_t` id once;
  storage is one contiguous `wchar_t` buffer + offsets. A header shared by 50
  DLLs is *one* entry. Hash/normalize once at intern time; compare integers
  forever after. (Today the full path is hashed *per probe*.)
- **Duplicate semantics without pointers.** "Several (file,line) per address" →
  duplicate adjacent `rva` entries; a lookup returns a *range* and sets all bits.
  "Several addresses per line" → resolved at report time by one pass over probes
  sorted by (fileId,line). This deletes the `bool*`-into-node coupling entirely.
- **Hot path becomes:** find the module instance covering the address (tiny
  sorted array of ≤ dozens of instances) → `rva = addr - base` →
  `lower_bound` in the module's `rva` column → set a bit → restore `origByte`.
  Two array probes and a bit-set instead of ~20 dependent cache misses.
- **Cache math.** 1M probes ≈ 4 MB `rva` column that streams through L2 at
  registration; the hit bitmap is 128 KB and stays hot in L2 for the whole run.
  An `std::map` find is ~20 dependent ~100 ns misses.
- **Unload/exit** drops a `ModuleInstance` in O(1); hit bits survive (which is
  exactly the desired merge-across-processes semantics).

### 4.3 Address → probe index

Breakpoints for a module are all known before any can fire, so build a **frozen
immutable index once** at module load. Recommended: the sorted `rva[]` array
itself + branchless `lower_bound` (optionally Eytzinger-laid-out), since it is
needed anyway for merge/report. Add a **page table** accelerator
(`rva >> 12` → per-page slice) only if profiling still shows the debug loop —
it likely won't, because the round trip dominates. This is the classic
debugger/DBI pattern, and we already cluster breakpoint writes per 4 KB range.

### 4.4 Arenas

Three lifetime buckets → three arenas: per-run (interned strings, probe columns),
per-module-parse scratch (`pmr::monotonic_buffer_resource`, reset after each
`LineTable` freezes), per-export. Note the ordering: **the model change in 4.2
removes more allocations than the allocator change does — do it first.**

Risk: medium but mechanical; touches `ExecutedAddressManager`, the exporters, and
the merger. The payoff on the hot lookup is real but *bounded* by the round-trip
cost above — the bigger wins here are killing the per-probe hash+alloc at
registration and enabling Tiers 2 and 4.

---

## 5. Tier 2 — Stop re-doing work across modules and processes

### 5.1 PDB parse cache (flyweight)

Registry keyed by **`(pdbPath, GUID, age)`** → `shared_ptr<const LineTable>`. The
GUID+age is the linker's exact identity (same key symbol servers use). First load
parses; every later load — same process or child, repeated DLL, or a later run —
is a pointer copy. A templated header in 50 DLLs is parsed **once**. Only
`origByte` and diff-based filters stay per-process. Immutability makes readers
lock-free (enables 5.3).

### 5.2 Read the PDB line table without DIA

DIA costs ≥6 COM virtual calls per line record; the on-disk CodeView **C13 format
is already columnar**: `DEBUG_S_LINES` subsections are packed 8-byte records
`{uint32 offsetInContribution; uint32 packed(24-bit line, 7-bit delta, 1-bit
isStatement)}`, with `DEBUG_S_FILECHKSMS` mapping file indices to the string
table. So "enumerate all (RVA, fileId, line)" becomes **mmap the PDB and
memcpy-scan flat arrays** — millions of records as a sequential few-MB scan
instead of millions of cross-DLL calls. Records come grouped by function
contribution, so `ReleaseCoverageFilter`'s per-symbol grouping falls out for free.

- Reader of choice: **raw_pdb** (Molecular Matters, BSD, memory-mapped, immutable
  streams safe for concurrent reads, "orders of magnitude faster than DIA").
  Alternatives/references: LLVM `DebugInfo/PDB`+`CodeView`, Microsoft
  `microsoft-pdb`, PascalBeyer's PDB-Documentation.
- Risk: medium-high. `/DEBUG:FASTLINK` and older line formats — **keep DIA as a
  fallback**; the code is already behind `IDebugInformationHandler`, so ship raw
  reading as the fast path and fall back on unsupported PDB shapes.

### 5.3 Parallelism — the safe boundaries

The `WaitForDebugEvent` loop is single-threaded by OS contract; don't fight it.
Parallel-safe: **PDB parsing** (raw_pdb streams are concurrency-friendly — parse
on a thread pool; pre-parse the main exe + its static import closure *before*
`CreateProcess`), **per-file HTML generation**, **`.cov` merge** (parallel reduce
of bitmaps), **filter scans** over the frozen table. All operate on immutable
data, so no locking.

---

## 6. Tier 3 — Fewer, cheaper traps (change the mechanism, stay binary-only)

Here we attack levers 2 and 3 directly. Ordered by effort.

### 6.1 Basic-block-leader breakpoints (biggest bang-for-buck)

Instrument **BB leaders**, not every PDB line. A leader hit implies every line in
its straight-line block executed; map back to lines via the line table we already
have. This is how mesos, TinyInst, and Jackalope all work, and it slashes the
number of ~100 µs round trips (the dominant runtime cost) with **no change to the
trap mechanism** — keep the one-shot remove-on-first-hit (re-arming would pay the
round trip per *hit* instead of per *first-hit*). Requires CFG reconstruction
(leaders = branch targets + instructions after branches); we already have a Zydis
disassembler in-tree from the branch-coverage work.

### 6.2 In-process VEH servicing (biggest per-hit win, novel)

Move breakpoint servicing **into the target**: an injected DLL installs a Vectored
Exception Handler that catches the `int3`, records the hit into the shared bitmap,
restores the byte, and continues — a **usermode exception dispatch (µs-class)
instead of the cross-process debug-port round trip (~100 µs)**. VEH is the
standard in-process hooking substrate and TinyInst's fault-and-handle is the same
idea, but a packaged VEH *coverage* tool appears to be open territory — genuine
differentiation. Combined with 6.1, this is the ceiling of the
breakpoint-based approach: fewer traps, each ~100× cheaper.

*Caveat: the µs figure is directionally well-supported but not isolated in a
public benchmark; the ~100 µs cross-process anchor is solid.*

### 6.3 Selective binary rewriting (data, ~15–20%, no recompile)

TinyInst/LiteCov model: mark the target module's code non-executable, fault on
first entry, JIT-rewrite basic blocks into a nearby region with inline
coverage-recording instructions; only target modules are instrumented, everything
else runs native. Coverage becomes a **bitmap** (DATA, not CONTROL). Reported
~15% with no client, ~20% with coverage on 64-bit — and it is pure usermode, no
driver. This is the natural "fast mode" that stays binary-only. Bigger
engineering lift than 6.1/6.2; consider adopting/wrapping TinyInst rather than
writing a rewriter from scratch.

---

## 7. Tier 4 — Coverage as data: the strategic bets

### 7.1 Intel Processor Trace (the headline data-driven path)

Let the program run at near-native speed while the CPU records control flow into a
compact trace buffer; decode offline into a coverage bitmap, mapped to lines via
the PDB. This is the fully data-oriented model — **no trap per line, ever.**

- **Collection overhead ~2–20%** (usually <5%); encoded density <1 byte/instr.
- **Windows feasibility is favorable:** Win10 1809+ ships **`ipt.sys`**, a
  built-in driver with a usermode IOCTL path — **no custom kernel driver
  needed.** Alex Ionescu's **winipt** library drives it (per-process/per-thread
  tracing, IP-range filtering on 2004+). Windows virtualizes PT state across
  context switches, so **per-thread trace survives scheduling** — solving the
  multithreading problem. WinAFL's PT mode already ships on this stack.
- **The hard part is decode, not collection.** Intel's `libipt` is 2–3 orders of
  magnitude slower than collection (>85% of it instruction disassembly). Fast
  fuzzer decoders solve it: **libxdc** (~15–30× libipt, emits a coverage bitmap
  directly), **Honeybee** (~44×). We'd integrate one of these, not roll our own.
- **Constraints / risks:** Intel-only (**AMD has no equivalent** — needs a
  Tier-3 fallback); ≤4 IP-range filters (read count from CPUID); buffer overflow
  silently drops packets; JIT/self-modifying code breaks reconstruction; winipt
  is an explicit proof-of-concept (admin/privilege and buffer semantics
  undocumented — must be pinned down).
- Granularity: full decode reconstructs exact flow → BB/edge/line/branch, the
  same model we emit today, without touching the binary.

This is the "what's possible with data-driven design" answer at the hardware
level: **collect a buffer at near-native speed, transform it into a coverage
bitmap in a batch pass.** The whole engine becomes a data pipeline.

### 7.2 Optional compiler-instrumentation "fast mode" (the throughput ceiling)

If a project can recompile, nothing beats linker-gathered counter arrays: a probe
is one `inc byte ptr [rip+disp]` against a flat array — **O(1), no trap, no
kernel** — the limiting case of DOD (frozen index, flat array, no lookup).

- **MSVC** `/fsanitize-coverage=inline-8bit-counters` (VS2022 17.0+) or
  **clang-cl** `-fprofile-instr-generate -fcoverage-mapping` (Chromium ships this
  on Windows at scale). We implement the tiny
  `__sanitizer_cov_8bit_counters_init(start,end)` runtime and read the array.
- Measured overhead ~tens of percent (AFL-Clang edge ~36% vs QEMU 612%).
- Merge is element-wise array addition — trivially parallel.

Trade-off: needs recompilation, so it is a *mode*, not the default. But it gives a
near-zero-overhead option for CI where the build is ours.

### 7.3 Columnar `.cov` v2 + merge-as-bitmap-OR

Give the binary format a columnar layout (version byte for back-compat): module
table, interned file-path blob + offsets, then per (module,file) a delta-varint
line column + a hit bitmap. **Merge becomes sorted-array union + bitwise-OR of
bitmaps** — O(n+m) streaming, mmap-friendly — instead of deserializing into
map-of-map hierarchies. This makes `--input_coverage` scale to many shards and is
the natural output side of the whole data pipeline. Emitting the **drcov v2
format** for BB coverage would also make output consumable by Lighthouse
(IDA/Binary Ninja).

---

## 8. What *not* to pursue

- **Hardware debug registers (DR0–DR3):** only 4 per thread; useless for
  observing thousands of not-yet-hit addresses.
- **Guard pages / PAGE_GUARD:** 4 KB granularity — too coarse for line/branch.
- **BTS (Branch Trace Store):** ~40× slowdown and **no Windows usermode/ETW API**
  at all (custom driver only). Superseded by PT.
- **LBR (Last Branch Record):** free but **sampled** — 16/32 records, statistical
  (call counts off by 1000–10000×). Can confirm "this ran at least once," can
  **never** prove "this never ran." At most a complementary hot-path hint, never
  a coverage source. (Accessible on Windows via ETW `TraceLbrConfigurationInfo`,
  admin-only.)

---

## 9. Suggested sequencing

```
Tier 0  ── mechanical fixes ......................... immediate, ship piecemeal
              │ (startup ~file-count-independent, export 10–50×, debug heap gone)
              ▼
Tier 1  ── columnar probe table + interning + arenas  the structural core
              │
              ├── Tier 2  ── PDB parse cache; raw C13 reader w/ DIA fallback;
              │               parallel parse/export
              │
              ▼
Tier 3  ── BB-leader breakpoints ...... fewer round trips (biggest runtime win, low effort)
              └── in-process VEH servicing ... ~100× cheaper per hit (novel)
              └── TinyInst-style rewriting ... binary-only "data" mode (~15–20%)
              ▼
Tier 4  ── Intel PT collector + libxdc decode ...... the data-driven bet (Intel-only + fallback)
           optional compiler-instrumentation fast mode (recompile; throughput ceiling)
           columnar .cov v2 (merge = bitmap-OR); optional drcov output
```

Rough impact expectation: Tier 0 alone should make the 5,000-header startup case
collapse from ~4.7 s toward the flat-file ~0.5 s and remove the debug-heap
penalty. Tier 3.1 (BB leaders) is the highest runtime-win-per-effort item —
it directly divides the number of ~50 µs round trips. Tier 3.2 (VEH) and Tier 4.1
(Intel PT) are where the order-of-magnitude changes live; they are also where the
engineering risk concentrates, which is why they come after the data-oriented
core makes the rest of the system cheap and parallel.

---

## Appendix — key references

- mesos (millions of one-shot breakpoints): https://github.com/gamozolabs/mesos
- TinyInst / LiteCov (selective rewriting): https://github.com/googleprojectzero/TinyInst
- Intel PT on Windows — winipt: https://github.com/ionescu007/winipt ·
  WinAFL PT mode: https://github.com/googleprojectzero/winafl/blob/master/readme_pt.md
- libxdc fast PT decoder: https://github.com/nyx-fuzz/libxdc ·
  Honeybee: https://blog.trailofbits.com/2021/03/19/un-bee-lievable-performance-fast-coverage-guided-fuzzing-with-honeybee-and-intel-processor-trace/
- SanitizerCoverage: https://clang.llvm.org/docs/SanitizerCoverage.html ·
  MSVC `/fsanitize-coverage`: https://learn.microsoft.com/en-us/cpp/build/reference/fsanitize-coverage
- LLVM source-based coverage: https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
- raw_pdb (fast DIA-free PDB reader): https://github.com/MolecularMatters/raw_pdb
- CodeView `DEBUG_S_LINES` / C13 format: https://github.com/PascalBeyer/PDB-Documentation
- drcov format + Lighthouse: https://www.ayrx.me/drcov-file-format/ ·
  https://github.com/gaasedelen/lighthouse
- "Full-speed Fuzzing" overhead numbers (S&P 2019): https://arxiv.org/abs/1812.11875
