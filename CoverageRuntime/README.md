# OpenCppCoverage fast-mode runtime

This is the compiler-instrumentation ("fast mode") collection path. Instead of
trapping on breakpoints, the target is compiled so that each basic block
increments an inline counter — no debugger, no per-hit kernel round trip — and
a small runtime writes the counters out at exit. OpenCppCoverage then maps them
back to source lines (and template instantiations) through the PDB.

It is a **mode, not the default**: it needs the target recompiled with
SanitizerCoverage. Use it where you own the build (typically CI) and want the
lowest possible overhead. It is architecture-agnostic — the same flags target
x64 and ARM64.

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
address by disassembling the module (`inc byte [rip+disp]` into `.sancov`) and
reconstructing basic-block leaders from the PDB function ranges. v1 instruments
the main executable only.

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

## Notes and limits

- Coverage is basic-block granular (the compiler's blocks), mapped to the source
  lines each block spans. SanitizerCoverage uses *minimal* instrumentation — not
  every block carries a counter; some (a function's entry, its return, control
  -flow merges) are left out because their execution is implied by others. The
  importer infers the entry region soundly (a function whose any block ran was
  entered), so it matches the debugger's line coverage on typical code; deeply
  nested control flow can still differ on an inferred block until full flow
  reconstruction lands. It never over-reports an unexecuted branch.
- Counters are saturating 8-bit (0–255).
- Instantiations folded together by `/OPT:ICF` (identical code) cannot be
  distinguished; fully inlined instantiations have no standalone function symbol
  and are attributed to their caller.
- The runtime is freestanding (Win32 only, no STL globals) so it links into any
  target regardless of CRT flavor.
