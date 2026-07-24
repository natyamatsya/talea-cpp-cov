# CPU profiling & flame graphs

Repeatable tooling for the CPU flame graphs and the engine comparison in
[../PERFORMANCE.md](../PERFORMANCE.md). ETW CPU sampling is the right profiler
here: it samples *on-CPU*, so it profiles where talea-cpp-cov actually burns
cycles (debug-event handling, disassembly, PDB parse) without over-counting the
time the tool sits blocked in `WaitForDebugEvent` — and x64 unwind tables make
optimized-release stacks reliable without frame pointers.

## Prerequisites

- **Windows Performance Toolkit** (`xperf.exe`) — from the Windows SDK / ADK.
- **Python 3** — for the folder.
- **Elevation** — ETW kernel sampling requires admin; `capture-flamegraph.ps1`
  self-elevates (one UAC prompt).

## Build

Use the profiling preset — Release codegen (`/O2`, mimalloc) but with
`/OPT:NOICF`, so identical-COMDAT folding doesn't merge distinct functions into
one symbol and misattribute self-time:

```
cmake --preset clang-cl-profile
cmake --build --preset clang-cl-profile
```

Binaries land in `out/clang-cl-profile/bin` with PDBs beside them.

## 1 — Capture

```powershell
docs\profiling\capture-flamegraph.ps1 `
    -OccExe  out\clang-cl-profile\bin\talea-cpp-cov.exe `
    -Target  out\clang-cl-profile\bin\TaleaCppCovTest.exe `
    -Sources . -Modules TaleaCppCovTest `
    -TargetArgs "--gtest_filter=-CodeCoverageRunnerTest.*:DebuggerTest.*" `
    -Etl talea.etl
```

The default engine is the in-process breakpoint (VEH) engine; add `-Debugger` to
profile the classic debugger instead. Profile a workload that runs for *seconds* — a
sub-second run yields too few samples.

## 2 — Fold into a flame graph

Point the symbol path at the build's PDBs and stream the symbolized dump through
the folder. Local PDBs resolve our code; add the Microsoft symbol server
(`srv*C:\sym*https://msdl.microsoft.com/download/symbols`) if you also want
kernel (`ntoskrnl`/`ntdll`) frames resolved.

```powershell
$env:_NT_SYMBOL_PATH = "$PWD\out\clang-cl-profile\bin"
xperf -i talea.etl -symbols -a dumper -stacktimeshifting `
  | python docs\profiling\fold-flamegraph.py talea-cpp-cov.exe talea.svg
```

This prints the top self-time hotspots, writes `talea.svg` (open in a browser),
and writes `talea.svg.folded` — the universal folded-stacks format you can drag
into <https://speedscope.app> or feed to `flamegraph.pl`. Under the VEH engine
the servicing work moves into the target, so also fold the target process:
`… | python fold-flamegraph.py TaleaCppCovTest.exe target.svg`.

The folder streams in constant memory (~2M dump lines/s), so it scales to large
traces; xperf's symbol resolution, not the fold, is the cost.

## 3 — Engine timing

```powershell
docs\profiling\bench-engines.ps1 `
    -ForkExe     out\clang-cl-release\bin\talea-cpp-cov.exe `
    -Target      out\clang-cl-release\bin\TaleaCppCovTest.exe `
    -Sources . -Modules TaleaCppCovTest `
    -BaselineExe C:\path\to\official\talea-cpp-cov.exe
```

Reports best-of-N wall-clock for the baseline (e.g. the official release), the
fork's debugger engine, and the fork's VEH engine, plus the speedups — the
numbers behind the table in [../PERFORMANCE.md](../PERFORMANCE.md#engine-comparison-vs-the-official-release).
