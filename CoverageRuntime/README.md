# OpenCppCoverage fast-mode runtime

This is the compiler-instrumentation ("fast mode") collection path. Instead of
trapping on breakpoints, the target is compiled so that each basic block
increments an inline counter — no debugger, no per-hit kernel round trip — and
this small runtime writes the counters out at exit. OpenCppCoverage then maps
them back to source lines (and template instantiations) through the PDB.

It is a **mode, not the default**: it needs the target recompiled with
SanitizerCoverage. Use it where you own the build (typically CI) and want the
lowest possible overhead. It is architecture-agnostic — the same flags target
x64 and ARM64.

## Building your target

Compile every translation unit you want covered with clang-cl and
SanitizerCoverage, compile this runtime **without** the coverage flag (so it does
not instrument itself), and **link with lld** — the MSVC linker (`link.exe`) does
not resolve the SanitizerCoverage section-boundary symbols, which silently
produces garbage coverage.

```
REM instrumented code
clang-cl -fsanitize-coverage=inline-8bit-counters,pc-table /Zi /O2 /c your_code.cpp

REM the runtime, NOT instrumented
clang-cl /Zi /O2 /c path\to\OpenCppCoverageFastCov.cpp

REM link with lld
clang-cl your_code.obj OpenCppCoverageFastCov.obj -fuse-ld=lld /Fe:your_app.exe /link /DEBUG
```

`clang-cl` and `lld` ship with Visual Studio's "C++ Clang tools for Windows"
component. Build with a matching MSVC toolset (its STL rejects an older Clang).

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
  lines each block spans — the same result as the debugger engine's line
  coverage, verified byte-identical on optimized and unoptimized builds.
- Counters are saturating 8-bit (0–255).
- Instantiations folded together by `/OPT:ICF` (identical code) cannot be
  distinguished; fully inlined instantiations have no standalone function symbol
  and are attributed to their caller.
- The runtime is freestanding (Win32 only, no STL globals) so it links into any
  clang-cl target regardless of CRT flavor.
