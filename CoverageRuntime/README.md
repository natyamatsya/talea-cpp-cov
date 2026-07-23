# OpenCppCoverage instrumentation runtime

This is the compiler-instrumentation collection path (`--engine
instrumentation`), the standard path for whole test suites and CI. Instead of
trapping on breakpoints, the target is compiled so that each basic block
increments an inline counter — no debugger, no per-hit kernel round trip — and
a small runtime writes the counters out at exit. OpenCppCoverage then maps them
back to source lines (and template instantiations) through the PDB. Collection
runs at native speed, independent of execution count, which is what makes it
the scalable choice for large suites.

Its one requirement is that you **own the build**: the target must be recompiled
with SanitizerCoverage. Where you cannot recompile — an arbitrary binary, or an
exact branch audit of a scoped target — use the breakpoint or debugger engine
instead (`--engine breakpoint` / `--engine debugger`). The instrumentation
engine is architecture-agnostic — the same flags target x64 and ARM64.

**Two toolchains are supported**, each with its own runtime source file:
`OpenCppCoverageFastCov.cpp` for clang-cl and `OpenCppCoverageFastCovMsvc.cpp`
for MSVC (`cl.exe`). Pick the one matching your compiler.

## Building with clang-cl

Compile every translation unit you want covered with clang-cl and
SanitizerCoverage, compile the runtime **without** the coverage flag, and
**link with lld** — the MSVC linker (`link.exe`) does not resolve clang's
SanitizerCoverage section-boundary symbols, which silently produces garbage
coverage.

```
REM instrumented code
clang-cl -fsanitize-coverage=inline-8bit-counters,pc-table /Zi /O2 /c your_code.cpp

REM the clang runtime, NOT instrumented
clang-cl /Zi /O2 /c path\to\OpenCppCoverageFastCov.cpp

REM link with lld
clang-cl your_code.obj OpenCppCoverageFastCov.obj -fuse-ld=lld /Fe:your_app.exe /link /DEBUG
```

`clang-cl` and `lld` ship with Visual Studio's "C++ Clang tools for Windows"
component. Build with a matching MSVC toolset (its STL rejects an older Clang).

## Building with MSVC (cl.exe)

MSVC supports `inline-8bit-counters` but not clang's `pc-table`, and uses its own
section scheme (no lld needed). Use the MSVC runtime, and note there is no
`pc-table`:

```
REM instrumented code
cl /fsanitize-coverage=inline-8bit-counters /Zi /O2 /c your_code.cpp

REM the MSVC runtime, NOT instrumented
cl /Zi /O2 /c path\to\OpenCppCoverageFastCovMsvc.cpp

REM ordinary MSVC link
link /DEBUG your_code.obj OpenCppCoverageFastCovMsvc.obj /OUT:your_app.exe
```

Because MSVC omits the PC table, the importer recovers each counter's block
address by disassembling the module — any instruction that *writes* the
`.sancov` region is a counter: `inc byte [rip+disp]`, `add byte [rip+disp], 1`,
and, when `/O2` hoists a loop's counter into a register, the `mov byte
[rip+disp], reg` that stores it back after the loop — and reconstructing
basic-block leaders from the PDB function ranges. (Gating on a *written* memory
operand rather than a fixed mnemonic is what catches the hoisted-counter form; a
read or `lea` into the section is not a counter.)

Keep the `/DEBUG` on the link (the examples above do). The 8-bit counters live
in a zero-initialized `.sancov` section (`SizeOfRawData` is 0), and a Release
link *without* `/DEBUG` turns on `/OPT:REF`, which prunes that counter region
so the dump comes back nearly empty. If you must link without `/DEBUG`, pass
`/OPT:NOREF` (and `/OPT:NOICF`) so the linker keeps the full `.sancov`.

### Covering instrumented DLLs

At exit the MSVC runtime enumerates every loaded module and dumps each one that
carries a `.sancov` section, so an instrumented DLL is covered alongside the
executable. Two things to note when instrumenting a DLL:

- Build the DLL's translation units with the coverage flag, **and link the
  runtime into the DLL too** — an instrumented module needs the
  `_Sancov8bitUsed` anchor the runtime defines, or it will not link.
- Linking the runtime into both the DLL and the executable is fine: only the
  copy in the main executable writes the dump (it enumerates every module), so
  there is exactly one dump regardless of how many modules link the runtime.

```
REM instrumented DLL: link the runtime for the anchor
cl /fsanitize-coverage=inline-8bit-counters /Zi /O2 /c your_lib.cpp
link /DLL /DEBUG your_lib.obj OpenCppCoverageFastCovMsvc.obj /OUT:your_lib.dll

REM the executable links the runtime as well; its copy writes the dump
link /DEBUG your_app.obj OpenCppCoverageFastCovMsvc.obj your_lib.lib /OUT:your_app.exe
```

A module with a `.sancov` section but no PDB is skipped by the importer; system
and uninstrumented DLLs have no such section and are ignored.

## Running and importing

Set `OPENCPPCOVERAGE_FASTCOV_OUTPUT` to the dump path before running the target,
then import the dump:

```
set OPENCPPCOVERAGE_FASTCOV_OUTPUT=coverage.fastcov
your_app.exe

OpenCppCoverage --sources MySource* --input_fastcov coverage.fastcov ^
               --fastcov_template_report templates.md ^
               --export_type cobertura:coverage.xml
```

`--input_fastcov` produces ordinary coverage data (exported like any other run).
`--fastcov_template_report` additionally writes a per-template report: for every
template, each instantiation's line coverage, and the lines covered in some but
not all instantiations — the gaps a single boolean per line hides (e.g. a line
exercised for `foo<int>` but never for `foo<double>`).

## Branch coverage (approximate)

The fast import also fills in **branch coverage** (the Cobertura `condition-coverage`
attributes and `<conditions>` elements): for every conditional jump the report
says whether each direction — taken and fall-through — was exercised.

It is **derived from block coverage**, not observed directly. The importer already
rebuilds each function's control-flow graph; a branch direction is reported
*covered* when the block that direction leads to was itself covered. This is what
makes it free: no extra instrumentation, no per-branch trapping, so it runs at the
same native speed as line coverage.

**What "approximate" means.** The derivation is *exact* when a branch's successor
block is reached **only** by that edge — the common case: the body of an `if`, the
exit of a `do/while`. It can diverge from the exact breakpoint/debugger engines at a
control-flow **merge**, where a block is reached by several edges:

- **over-report** — a direction is marked covered because its successor block ran,
  even though that block was actually reached by a *different* edge (e.g. the join
  after an `if/else`, or a loop-exit block shared with the zero-trip guard MSVC
  emits for every `for`/`while`);
- **under-report** — the conservative block propagation (which only flows coverage
  across *single*-successor / *single*-predecessor edges) can leave a merge block
  unmarked even though it ran, so a direction into it reads uncovered.

Neither affects **line** coverage, which stays block-accurate. Only the
taken/not-taken *direction* attribution is affected, and only at merges.

**Exact branch coverage** requires observing each conditional jump's real outcome —
which is what the breakpoint (`--engine breakpoint`) and debugger (`--engine
debugger`) engines do, at the cost of a trap per branch execution. There is no way
to recover it exactly from block counters alone (that would need edge-level
instrumentation, which MSVC's SanitizerCoverage does not emit). So treat the
instrumentation engine's branch coverage as a **fast indicator** — excellent for
tracking the trend and finding never-exercised branches across a large suite — and
use a breakpoint/debugger run when you need an *exact* branch audit of a scoped
target.

## Notes and limits

- Coverage is basic-block granular (the compiler's blocks), mapped to the source
  lines each block spans. SanitizerCoverage uses *minimal* instrumentation — not
  every block carries a counter; some (a function's entry, its return, control
  -flow merges) are left out because their execution is implied by others. The
  importer rebuilds each function's control-flow graph and propagates coverage
  from the instrumented blocks (entry dominates all; a covered block's single
  successor and single predecessor also ran), so it matches the debugger's line
  coverage — verified line-identical on simple and complex control flow at `/Od`
  and `/O2`. It never over-reports an unexecuted branch; only a path that threw
  before a single-successor edge can differ (the same limit as the debugger
  engine's run merging).
- Counters are saturating 8-bit (0–255).
- Instantiations folded together by `/OPT:ICF` (identical code) cannot be
  distinguished; fully inlined instantiations have no standalone function symbol
  and are attributed to their caller.
- The runtime is freestanding (Win32 only, no STL globals) so it links into any
  target regardless of CRT flavor.
