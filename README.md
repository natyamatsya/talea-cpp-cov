# talea-cpp-cov (experimental fork)

OpenCppCoverage is an open source code coverage tool for C++ under Windows.

The main usage is for unit testing coverage, but you can also use it to know the executed lines in a program for debugging purpose.

---------------------
## Fork status: experimental, for private usage

This is a personal fork of the archived
[OpenCppCoverage/OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage)
project, which is no longer maintained by its original author.

It exists to experiment with features I need myself — currently
[branch coverage](#branch-coverage-experimental). Please note:

* this fork is **not** a community successor of the original project;
* there are no plans for serious maintenance, releases, or support;
* issues and pull requests may not be reviewed or answered;
* things may break or change without notice — use at your own risk.

The project remains available under the terms described in the
[LICENSE](LICENSE.txt) file. Feel free to use, modify, and fork it according
to those terms.

### Engineering changes in this fork

Beyond the coverage features below, this fork has modernized the project:

* **CMake + Ninja build** replacing the MSBuild solution, with presets for both
  MSVC (`cl.exe`) and `clang-cl.exe` — see [Building from source](#building-from-source).
* **Single self-contained executable** — `talea-cpp-cov.exe` links every
  dependency and the C runtime statically, so it ships as one file with no
  side-by-side DLLs and no Visual C++ redistributable; its only dependencies are
  Windows system libraries.
* **No third-party package dependencies** — Boost was dropped entirely (its one
  small-vector use is a vendored MIT header); the remaining libraries (fmt,
  spdlog, PCRE2, ctemplate, …) are statically linked in.
* **Explicit fixed-width integer types** throughout first-party code — see
  [docs/adr/0001](docs/adr/0001-explicit-width-integer-types.md).
* **Faster export and merge paths** (fmt-based Cobertura writer, `std::flat_map`
  line storage) — see [docs/PERFORMANCE_ROADMAP.md](docs/PERFORMANCE_ROADMAP.md).

One consequence of the single-executable design: the external export-plugin
mechanism (drop-in `Plugins\Exporter\*.dll`) has been removed — its DLL boundary
passed C++ objects across a heap boundary, which the static CRT cannot allow.
The built-in HTML, Cobertura and binary exporters are unaffected.

---------------------
## Features:
- **Visual Studio support**: Support compiler with program database file (.pdb).
- **Non intrusive**: Just run your program with talea-cpp-cov, no need to recompile your application.
- **HTML reporting**
- **Line coverage**.
- **Branch coverage** (experimental): See [Branch coverage](#branch-coverage-experimental) below.
- **Run as Visual Studio Plugin**: See [here](https://github.com/OpenCppCoverage/OpenCppCoveragePlugin) for more information.
- **Jenkins support**: See [here](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/Jenkins) for more information. For unattended runs, a crashing target does not pop a WER dialog — see [Crashing targets and the Windows Error Reporting dialog](#crashing-targets-and-the-windows-error-reporting-dialog).
- **Support optimized build**.
- **Exclude a line based on a regular expression**.
- **Child processes coverage**.
- **Coverage aggregation**: Run several code coverages and merge them into a single report.
 
## Requirements
- 64-bit Windows 10 or later.
- The profiled program must be **64-bit** and built with a program database
  (`.pdb`). Use the [original OpenCppCoverage releases](https://github.com/OpenCppCoverage/OpenCppCoverage/releases)
  if you need to profile 32-bit binaries.

## Building from source

The build uses CMake (≥ 4.4) with the Ninja generator and supports both
`cl.exe` (MSVC) and `clang-cl.exe`.

Prerequisites:
- Visual Studio with the *Desktop development with C++* workload (this
  includes the DIA SDK, ATL, CMake and Ninja). For the clang-cl presets, also
  install the *C++ Clang tools for Windows* component (or a standalone LLVM).
- Run `InstallThirdPartyLibraries.ps1` once: it clones and bootstraps a pinned
  [vcpkg](https://github.com/microsoft/vcpkg) into `build\third-party\vcpkg`.

Then, from an **x64 Native Tools Command Prompt** (the presets rely on the
compiler and Windows SDK being on the environment paths):

```
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

Available presets: `msvc-debug`, `msvc-release`, `clang-cl-debug`,
`clang-cl-release`. Binaries land in `out\<preset>\bin`. Dependencies are
declared in `vcpkg.json` (manifest mode) and are built automatically during
the first configure, which therefore takes a while.

Build options: `-DOCC_WERROR=OFF` disables warnings-as-errors,
`-DOCC_MSVC_LTO=OFF` disables whole-program optimization in Release builds.

Notes:
- This fork is x64 only — both the tool itself and the profiled programs must
  be 64-bit. Use the original releases if you need to profile 32-bit binaries.
- The C++/CLI test fixture (`TestCppCli`) was removed together with the old
  MSBuild system: the test suite no longer covers coverage of managed (mixed
  managed/native) test assemblies. Coverage of mixed-mode binaries by the tool
  itself is unchanged.

## Download
talea-cpp-cov can be downloaded from [here](../../releases).

## Usage
You can simply run the following command:

```talea-cpp-cov.exe --sources MySourcePath* -- YourProgram.exe arg1 arg2```

For example, *MySourcePath* can be *MyProject*, if your sources are located in *C:\Dev\MyProject*.

See [Getting Started](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki) for more information about the usage.
You can also have a look at [Command-line reference](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/Command-line-reference).

### PDB line-table cache

Parsed PDB line tables are cached under `%LOCALAPPDATA%\talea-cpp-cov\PdbCache`,
keyed by the PDB's GUID and age (the linker's exact identity), so repeated runs
against unchanged binaries skip the debug-information parse entirely. A relink
changes the key, so the cache can never return stale data. Use `--no_pdb_cache`
to disable it.

### Import-closure pre-parse (optional)

`--preparse_imports` parses the executable and its imported DLLs' PDBs in
parallel before the program starts, warming the line-table cache so each module
then loads without a per-module parse stalling the debug loop. It is a pure
performance optimization — coverage results are identical — and its benefit
scales with the number and size of the imported DLLs (measured ~11% faster cold
startup on an 8-DLL target). Off by default because the payoff is
target-dependent.

### Straight-line run merging

Instead of one breakpoint per source line, one breakpoint is placed per
straight-line run of code: hitting the run's first instruction proves execution
of every line the run touches, so the number of debug-event round trips — the
dominant cost of a coverage run — drops by up to an order of magnitude.
Merging is sound (runs end at every branch, call and return; functions with
jump tables fall back to per-line breakpoints), so coverage results are
identical to the unmerged mode. Use `--no_line_merging` to opt out; it is
disabled automatically with `--optimized_build`.

### Choosing an engine (`--engine`)

`--engine` selects how coverage is collected. All three produce identical line
coverage; they differ in speed and in how branches are reported:

| `--engine` | Mechanism | Speed | Branches | Recompile? |
|---|---|---|---|---|
| `instrumentation` | Compiler counters, imported from `--input_fastcov` | Native, execution-count independent | Derived (approximate at merges) | Yes |
| `breakpoint` *(default when running)* | In-process VEH breakpoints | Per-hit, x64 only | Exact (observed) | No |
| `debugger` | Classic cross-process debugger | Slower; x64 + x86 | Exact (observed) | No |

The **instrumentation** engine is the standard path for whole suites and CI (its
build and run steps are in
[Instrumentation engine: the standard path](#instrumentation-engine-the-standard-path)
below). The **breakpoint** and **debugger** engines are the exact reference —
reach for them to run an uninstrumented binary or to cross-check the
instrumentation engine's branch coverage on a scoped target. `--engine` defaults
to `instrumentation` when `--input_fastcov` is given, otherwise `breakpoint`.

### Breakpoint and debugger engines (the exact reference)

The **breakpoint** engine runs the target without a debugger: it is launched
suspended, a small helper DLL is injected, and a Vectored Exception Handler
services each breakpoint inside the process. This replaces the cross-process
debug-event round trip (~50 µs per first hit) with an in-process exception
dispatch, so hit-heavy runs speed up several-fold (measured ~5× on a
fully-executed 100k-line program) while producing exactly the same coverage as
the debugger. It covers the main executable, filter-matching DLLs, branch
coverage (`--branch_coverage`), and child processes (covered by default,
recursive across the whole process tree; opt out with `--no_cover_children`).
It is 64-bit only. Pass `--engine debugger` to use the classic cross-process
debugger, which is slower but works on x86 and additionally re-instruments a
DLL that is unloaded and then loaded again.

### Crashing targets and the Windows Error Reporting dialog

When a covered process crashes, the run-based engines differ in what the OS does:

- Under the **debugger engine** (`--engine debugger`), a debugger is attached, so
  the target's unhandled exceptions are delivered to the tool and Windows Error
  Reporting (WER) never engages.
- Under the **default breakpoint engine**, the target runs natively. So that a
  crashing process cannot pop the "*… has stopped working*" dialog — which
  would block an unattended run and hide the failure — the tool places the
  whole target process tree in a [job object](https://learn.microsoft.com/windows/win32/procthread/job-objects)
  with `JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION`. Every process in the tree
  (the target and, transitively, a test runner's spawned test executables) then
  terminates *silently* on an unhandled exception, and its exception code
  surfaces as the exit code, which the tool logs
  (`the target exited with crash code 0xc0000005…`). Coverage collected up to
  the fault is still reported.

  This is automatic and needs no machine configuration. It reliably suppresses
  the dialog for classic unhandled exceptions (access violations and the like).
  A few failure modes — notably `__fastfail` (`0xC0000409`, e.g. a `/GS`
  stack-cookie check or a CRT fatal error) — can still reach WER on some Windows
  builds despite the job. For a hard guarantee on an **unattended CI runner**,
  also disable the WER UI machine-wide once, in
  `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting`:

  - `DontShowUI` = `1` (DWORD) — WER never shows a dialog; faults are handled
    silently and the exit code still propagates.
  - Optionally add a `LocalDumps` subkey (`DumpFolder`, `DumpType` = `2` for a
    full dump) to capture crash dumps to a folder for post-mortem instead of
    losing them.

### Instrumentation engine: the standard path

For anything larger than a scoped run — a whole test suite, a CI lane — the
instrumentation engine (`--engine instrumentation`) is the path to reach for.
Build the target with
SanitizerCoverage so each basic block increments an inline counter — no
debugger, no per-hit kernel round trip — run it, and import the resulting dump
with `--input_fastcov` (which selects this engine). Collection happens at
**native speed**, independent of how many times a line runs, so it stays fast
where the breakpoint engines pay per hit.

Line coverage is **byte-identical to the breakpoint/debugger engines**. Branch
coverage is *derived* from the block counters rather than observed per jump, so
it is exact at sole-predecessor successors and approximate at control-flow
merges — see [Branch coverage](#branch-coverage-experimental) and
[coverage-runtime/README.md](coverage-runtime/README.md). Use a breakpoint/debugger
run when you need an exact branch audit of a scoped target.

Both **clang-cl** and **MSVC (`cl.exe`)** are supported, and the runtime covers
the executable and its instrumented DLLs, not just the main module. Because the
compiler emits a distinct function per template instantiation,
`--fastcov_template_report` additionally writes a per-template report showing,
for every instantiation, which lines it covered — and the lines covered in some
but not all instantiations (a line exercised for `foo<int>` but never for
`foo<double>`), which plain line coverage hides. It is architecture-agnostic
(x64 and ARM64); its one cost is the recompile with instrumentation. See
[coverage-runtime/README.md](coverage-runtime/README.md) for the build and run
steps.

### Performance and benchmarks

All three engines produce identical **line** coverage; they differ in speed and
in how they report branches. The instrumentation engine is execution-count
independent (the standard path for large suites); the breakpoint and debugger
engines observe every branch outcome directly (the exact comparison, at a trap
per branch execution). For how they work, when to use each, the measured
benchmarks, and the columnar `.cov` aggregation format, see
[docs/PERFORMANCE.md](docs/PERFORMANCE.md).

## Branch coverage (experimental)

In addition to line coverage, talea-cpp-cov can measure branch coverage: for every
conditional jump, it records whether the branch was taken, not taken, or both.
Branch coverage is disabled by default and enabled with `--branch_coverage`:

```
talea-cpp-cov.exe --branch_coverage --sources MySourcePath* --export_type cobertura:coverage.xml -- YourProgram.exe
```

### How it works

Conditional jump instructions are located by disassembling the code of each covered
source line ([Zydis](https://github.com/zyantific/zydis) disassembler, no recompilation
needed). Each conditional jump is instrumented with a breakpoint; when it is hit, the
branch outcome is computed from the CPU flags and the breakpoint stays armed until both
outcomes have been observed. Because of this extra instrumentation, the profiled program
runs slower than with line coverage only.

### Report formats

Branch coverage is exported in the **Cobertura** format:

- `branch-rate`, `branches-covered` and `branches-valid` are filled with real values;
- every line containing conditional jumps gets `branch="true"` and
  `condition-coverage="50% (1/2)"` style attributes, plus a `<conditions>` element with
  one entry per conditional jump.

This output works out of the box with the Jenkins
[Coverage Plugin](https://plugins.jenkins.io/coverage/) and with the VS Code
[Coverage Gutters](https://marketplace.visualstudio.com/items?itemName=ryanluker.vscode-coverage-gutters)
extension (name the report `coverage.xml` or `cobertura.xml` so it is detected
automatically; partially covered branch lines are highlighted in yellow).

The **binary** export format (`--export_type binary`) also carries branch data, so it
survives `--input_coverage` imports and coverage aggregation: outcomes of the same
branch are merged across runs. Old `.cov` files without branch data can still be
imported. The HTML report and the console summary show line coverage only.

### Limitations

- Branches are measured at the assembly level: one source line can contain several
  conditional jumps (e.g. `&&`, `||`, loops), and the compiler can generate additional
  ones. Conversely, some conditional constructs compile to branch-free code
  (e.g. `cmov`) or jump tables (some `switch` statements) and are not measured.
- Aggregating runs with `--input_coverage` matches branches by file, line and index,
  which is only reliable when all runs use the same binaries.
- Branch data is exported through the built-in Cobertura and binary formats. This
  fork no longer supports external export plugins (see
  [Engineering changes in this fork](#engineering-changes-in-this-fork)).
