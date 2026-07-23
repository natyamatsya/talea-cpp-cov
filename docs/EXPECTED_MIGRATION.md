# Migration analysis: exceptions → `std::expected<T, E>`

Originally analysis-only. **Phase 1 (§6) is now implemented** — see
*Implementation status* below; the rest remains analysis. Excluded
`inspiration/`, `vcpkg_installed/`, `Build/ThirdParty/`, `*/x64/*`. Paths are
relative to the repo root.

## Implementation status

**Phase 1 landed** (`Tools/Error.hpp`, `Tools/Error.cpp`). The shared error type
is `Tools::Error { std::wstring message; }` with
`Tools::Expected<T> = std::expected<T, Error>`. (A structured Win32 code was part
of the original design but dropped as dead until a `THROW_LAST_ERROR` site is
migrated — none is.) A `MAKE_ERROR(msg)` macro builds
the `file:line message` string exactly as `THROW_BASE` did, so a bridged
exception reads byte-for-byte like the old thrown one. The `expected→throw`
bridge is `Tools::ValueOrThrow(...)` / `Tools::ThrowOnError(...)`, which rethrow
via `Tools::ThrowError` as `Tools::ToolsException` (kept in `Error.cpp` so callers
need not include `ToolsException.hpp` and trip its `THROW` redefinition).

Note the §8 ABI hazard is **moot in practice**: `TOOLS_DLL`/`EXPORTER_DLL` expand
to nothing (everything is statically linked), so `Tools::Error` crosses no DLL
boundary.

Converted to `Tools::Expected`: `Regex` (new `Compile` factory; the throwing
ctor is now the bridge over it), `ReadProcessMemory`/`WriteProcessMemory` (+ the
`ReadStructInProcessMemory` template), `PEFileHeader::Load`, `MappedFile::TryCreate`
(null = missing/empty, error = unmappable), `GetExecutableFolder`,
`CreateParentFolderIfNeeded`, `ColumnarBinary::Read`/`ReadFile`,
`CoverageDataDeserializer::Deserialize`, and `FastCoverageImporter::Import` (via a
`catch→expected` boundary over the private `ImportOrThrow`). `IsColumnarFile` was
left alone (already non-throwing). The two throw-for-control-flow sites in
`MonitoredLineRegister` (§5 #7) now consume the `Expected` directly and are
throw-free; every other caller is bridged, preserving behavior (274/274 tests
green). `Tools::Regex::Compile`, the leaf conversions, and their callers are the
model for the remaining phases.

**Phase 2 partially landed** (the two clean `catch` seams from §3, which carry
the real value):

- **`PathMatcher::Match` → `std::expected<File*, AmbiguousPath>`.** The former
  `AmbiguousPathException` — the only custom exception carrying structured data
  (§5 #4) — is deleted; its three paths now travel as the `FileFilter::AmbiguousPath`
  error value, which `UnifiedDiffCoverageFilter::SearchFile` reads directly (no
  catch), still surfacing the same ambiguity `std::runtime_error` to its callers.
- **The OptionsParser path is now throw-free end-to-end.** Internal `Parse` and
  every validation helper, plus `IOptionParser::ParseOption`/`ExportOptionParser`,
  return `Tools::Expected`; internal `Parse` returns `Expected<optional<Options>>`
  (`nullopt` = `--help`) and the public overload consumes it without a catch. The
  glaze value→`throw`→catch round-trip at the old `:408` is gone — glaze's
  by-value error becomes `std::unexpected` directly.

The rest of the parse/validation layer has since been converted too:

- **`UnifiedDiffParser::Parse` → `Tools::Expected<std::vector<File>>`**, threaded
  through every helper (`ExtractTargetFile`, `ExtractHunksDifferences`,
  `ExtractUpdatedLines`, `FillUpdatedLines`, the git-prefix check); the old
  `ThrowError` became `MakeError` (same message, by value). The
  `UnifiedDiffParserException` class is kept only for its message constants.
- **`DebugInformationEnumerator::Enumerate` → `Tools::Expected<bool>`** via a
  `catch→expected` boundary over the private `EnumerateOrThrow` (the 12 DIA
  HRESULT throws stay put — they are deep in COM helpers). The concrete win is at
  the `std::execution::par` preparse site in `CodeCoverageRunner`: with the parse
  failure reported by value, its best-effort `catch(...)` is removed, so nothing
  can escape the parallel lambda (§5 #3). The debug-loop, VEH and fast-cov callers
  are bridged.

**Still on exceptions by design:** the debug-event loop (`Debugger`), the VEH
handler path, the category-(c) `THROW_LAST_ERROR` Win32 sites, category-(b)
invariants, and the top-level `main.cpp`/`OpenCppCoverage.cpp` catches — all as
§6 recommends.

## Critical distinction

Two unrelated things are both called "exception" here:

- **C++ exceptions** — `throw` / `try` / `catch`, the `THROW` macro family,
  `std::exception`. **This is the migration target.**
- **Windows debug / hardware (structured) exceptions** — `EXCEPTION_DEBUG_INFO`,
  `EXCEPTION_BREAKPOINT` (INT3), `EXCEPTION_SINGLE_STEP` (trap flag), delivered via
  `WaitForDebugEvent` (debugger) or a vectored handler (VEH). Types like
  `CppCoverage::ExceptionHandler`, `ExceptionHandlerStatus`, `FirstChanceException`
  refer to *these*. They are **not** C++ throw/catch and are **out of scope**.

Every place the two touch is flagged in §5.

## 1. Exception taxonomy

**Macro machinery (`Tools/ExceptionBase.hpp`)**
- `Tools::ExceptionBase : public std::exception` (line 31); ctor stores message via
  `Tools::ToLocalString(wstring)`.
- `GENERATE_EXCEPTION_CLASS(ns, name)` (42) — defines `ns::name : ExceptionBase`.
- `THROW_BASE(ns, class, message)` (55) — builds `"<file>:<line> <message>"` via
  `Tools::GetFilename(__FILE__)` and throws (actual `throw` at 61).

**Custom exception classes (macro-based)**

| Class | File:line | Per-module macro |
|---|---|---|
| `Tools::ToolsException` | `Tools/ToolsException.hpp:21` | `THROW` (23) |
| `CppCoverage::CppCoverageException` | `CppCoverage/CppCoverageException.hpp:30` | `THROW` (32), `THROW_LAST_ERROR` (33, appends `GetErrorMessage(lastErrorCode)`) |
| `Exporter::ExporterException` | `Exporter/ExporterException.hpp:21` | `THROW` (23) |
| `FileFilter::FileFilterException` | `FileFilter/FileFilterException.hpp:21` | `THROW` (23) |
| `CppCoverageConsole::CppCoverageConsoleException` | `OpenCppCoverage/OpenCppCoverageException.hpp:21` | `THROW` (23) — no live throw sites |

**Custom exceptions deriving from `std::runtime_error` (not the macro)**
- `Exporter::InvalidOutputFileException` — `Exporter/InvalidOutputFileException.hpp:31` (carries `path` + `outputKind`).
- `FileFilter::AmbiguousPathException` — `FileFilter/AmbiguousPathException.hpp:27` (three `path` members + getters — the **only** custom exception carrying structured data; see §5).
- `FileFilter::UnifiedDiffParserException` — `FileFilter/UnifiedDiffParserException.hpp:28` (named static message constants).
- `Plugin::OptionsParserException` — `Plugin/OptionsParserException.hpp:24` (cross-DLL option-validation error).

**std / system / third-party types thrown or caught**
- `std::runtime_error` — thrown directly in ~a dozen places (`Coverage/FileCoverage.cpp:37/49/65/81`, `FileFilter/UnifiedDiffCoverageFilter.cpp:39/106`, `CppCoverage/Process.cpp:154`, `Exporter/Binary/CoverageDataDeserializer.cpp:38`, `Tools/Tool.cpp:59/78`).
- `std::exception` — catch-all base at every top-level/best-effort handler (§3).
- `std::filesystem::filesystem_error` — not caught by name; surfaces as `std::exception` at top level.
- **glaze** — returns errors by value (`result.error`), converted to a thrown `OptionsParserException` at `OptionsParser.cpp:408`. Does not throw across the boundary.
- **pcre2** — wrapped by `Tools::Regex`; compile failure → `THROW` at `Tools/Regex.cpp:72`.
- **ctemplate** — failures detected by return value → `THROW` (`TemplateHtmlExporter.cpp:61/86/91`).
- **DIA (COM)** — `HRESULT` checks → `THROW` throughout `DebugInformationEnumerator.cpp` (12 sites).
- **SEH / `__try` / `__except`** — none in first-party code. Structured exceptions handled via the debug-event loop and VEH, not `__try`.

## 2. throw-site inventory

~104 production throw sites (the external export-plugin subsystem — `Exporter/Plugin/*` — was
removed since this analysis was first written, dropping ~8 sites).

| Module (DLL) | Sites | Key files |
|---|---|---|
| CppCoverage.dll | ~64 | `OptionsParser.cpp` (14), `DebugInformationEnumerator.cpp` (12), `FastCoverageImporter.cpp` (8), `Debugger.cpp` (7), `HandleInformation.cpp` (5), `BranchCoverageManager.cpp` (4), `BreakPoint.cpp` (2), `Process.cpp` (2), `Handle.hpp` (2), `VehCoverageEngine.cpp` (3), + 1 each in several files |
| Exporter.dll | ~20 | `Binary/ColumnarCoverage.cpp` (10), `Html/TemplateHtmlExporter.cpp` (3), `Html/HtmlFolderStructure.cpp` (2), + 1 each |
| Tools.dll | ~11 | `Tool.cpp` (4), `ProcessMemory.cpp` (2), `PEFileHeader.cpp` (2), `MappedFile.cpp` (1), `Regex.cpp` (1), `Tool.hpp` (1) |
| FileFilter.dll | ~5 | `UnifiedDiffParser.cpp` (2), `UnifiedDiffCoverageFilter.cpp` (2), `PathMatcher.cpp` (1) |
| Coverage.dll | ~4 | `FileCoverage.cpp` (4) |
| OpenCppCoverage.exe | 0 | (macro definition only) |

**Classification**

- **(a) RECOVERABLE — ~65 sites — prime `std::expected` candidates.**
  - Option/user-input validation (~17): all of `OptionsParser.cpp` (`:57,74,132,147,184,190,218,226,235,283,290,408,434,478`), `ExportOptionParser.cpp:116`, `Options.cpp:35`.
  - Missing/unreadable/malformed input files (~25): `FastCoverageImporter.cpp` (8), `ColumnarCoverage.cpp` (10), `CoverageDataDeserializer.cpp:38`, `MappedFile.cpp:37`, `StartInfo.cpp:35`, `PEFileHeader.cpp:35/49`, `UnifiedDiffCoverageFilter.cpp:39`.
  - Parse failures (~4): `UnifiedDiffParser.cpp:76/264`, `PathMatcher.cpp:90` + rethrow `UnifiedDiffCoverageFilter.cpp:106`.
  - Bad regex pattern (1): `Regex.cpp:72`.
  - Symbol/PDB resolution (~12): `DebugInformationEnumerator.cpp`.
  - Output-file/template (~8): `CoberturaExporter.cpp:451`, `CoverageDataSerializer.cpp:40`, `TemplateHtmlExporter.cpp:61/86/91`, `HtmlFileCoverageExporter.cpp:142`.
  - Launch failure (2): `VehCoverageEngine.cpp:1547/1558` (the second is now a 64-bit-target guard).
- **(b) INVARIANT / programmer-error — ~30 sites — keep `throw` / `assert` / `std::unreachable`.**
  `BranchCoverageManager.cpp:110/124/165`, `BranchCondition.cpp:71` & `VehCoverageEngine.cpp:148`, `ExecutedAddressManager.cpp:144`, `MonitoredLineRegister.cpp:651`, `Handle.hpp:36/38`, `Process.cpp:117`, `Debugger.cpp:220/237/260/273/282`, `HtmlFolderStructure.cpp:100/120`. `FileCoverage.cpp:37/49/65/81` = documented "throw on invalid argument" contract (INVARIANT-at-boundary).
- **(c) TRULY EXCEPTIONAL — ~12 sites — unexpected Win32/COM/OOM.**
  `Debugger.cpp:87/99` (`THROW_LAST_ERROR`; also §5), `BreakPoint.cpp:113/125`, `BranchCoverageManager.cpp:172` (Get/SetThreadContext), `HandleInformation.cpp:41/105`, `ProcessMemory.cpp:58/86` (but caught-as-recoverable at `MonitoredLineRegister.cpp:573`), `Tool.cpp:41/59/78`.

## 3. catch-site inventory

~18 `catch` clauses (no `__try`/`__except`; the 3 in the removed `Exporter/Plugin/*` subsystem and
the old locale-imbue swallow in `Log.cpp` are gone — see below).

**Top-level (exception → exit code / message)**
- `OpenCppCoverage/main.cpp:40-55` — outermost `try`; `catch(const std::exception&)` (`:46`) prints `"Error: " + e.what()`, `catch(...)` (`:50`) → `"Unknown error"`, both `return FailureExitCode` (`:55`). *The* boundary converting escaped exceptions to exit code.
- `OpenCppCoverage/OpenCppCoverage.cpp:296-306` — inner guard around `Run(...)`; `catch(std::exception&)` (`:300`) → `LOG_ERROR` (`:302`); sets `FailureExitCode`, continues to display warnings.

**Convert-to-user-message seams**
- `CppCoverage/OptionsParser.cpp:364` — `catch(const OptionsParserException&)` → `ShowExplanation()`, returns `std::nullopt`. Natural `expected` seam for the options layer.
- `FileFilter/UnifiedDiffCoverageFilter.cpp:97` — `catch(const AmbiguousPathException&)` → reformats the three paths, rethrows `std::runtime_error` (`:106`).
- `Tools/Tool.hpp:45-54` — `Try<Exception>(fct, buildErrorMsg)` translates any error into a module exception. `Tool.cpp:124-128` similar.

**Swallow-by-design / log-and-continue (preserve as `expected`-ignoring paths)**
- `CppCoverage/RawPdbReader.cpp:350` — `catch(...)` in a `std::execution::par` lambda sets `anyFailed` (comment `:336`: escaping exception → `std::terminate`). **Hard constraint (§5).**
- `CppCoverage/CodeCoverageRunner.cpp:105` — `catch(...)` in a `par` preparse lambda: best-effort skip.
- `CppCoverage/MonitoredLineRegister.cpp:573` — `catch(const std::exception&)`: unreadable code → fall back to one probe per line (deliberate throw-as-control-flow; good `expected` candidate on both ends).
- `Tools::Try` (`Tool.hpp:34` overload) already returns `std::optional<std::wstring>` — an `expected`-like idiom.

## 4. Cross-DLL throwing exports (signatures that would change) — most important

**Tools.dll** — `GetExecutableFolder()` (`Tool.hpp:57`→`Tool.cpp:41`), `CreateParentFolderIfNeeded()` (`Tool.cpp:163`), `LocalToWString/Utf8ToWString/ToLocalString/ToUtf8String` (`Tool.cpp:59/78`), `ReadProcessMemory/WriteProcessMemory` (`ProcessMemory.cpp:58/86`; already recoverable-consumed), `Regex` ctor (`Regex.cpp:72`), `MappedFile` ctor (`MappedFile.cpp:37`), `PEFileHeader` (`PEFileHeader.cpp:35/49`), `Try<>` template seam.

**CppCoverage.dll** — `OptionsParser::Parse` (internal throws `OptionsParserException`; public 3-arg overload already returns `std::optional<Options>` at `:364`), `ExportOptionParser` (`:116`), `DebugInformationEnumerator::Enumerate` (12), `FastCoverageImporter` (8, file-format), `Debugger::Debug` (Win32 — debug loop, §5), plus `BreakPoint`/`BranchCoverageManager`/`ExecutedAddressManager`/`HandleInformation`/`Process`/`VehCoverageEngine`/`CodeCoverageRunner` (mostly category b/c → leave on `throw`), `Options`.

**Exporter.dll** — `CoberturaExporter::Export` (`InvalidOutputFileException`, `:451`), `CoverageDataSerializer`/`CoverageDataDeserializer::Deserialize` (`:40`/`:38`), **free `EXPORTER_DLL`** `ColumnarCoverage::Write/Read/IsColumnarFile/ReadFile` (`ColumnarCoverage.hpp:77/81/85/88`; `Read`/`ReadFile` = clearest `expected<CoverageData,E>`), `TemplateHtmlExporter`/`HtmlFileCoverageExporter`/`HtmlFolderStructure`.

**FileFilter.dll** — `UnifiedDiffParser::Parse` (`:76/264`), `UnifiedDiffCoverageFilter` (`:39/106`), `PathMatcher::Match` (`AmbiguousPathException`, `:90`).

**Coverage.dll** — `FileCoverage` line-add API (`:37/49/65/81`).

## 5. Hazards / must-stay-exceptions

1. **Debug-event loop (`Debugger.cpp`)** — handles Windows *debug* exceptions; its own `WaitForDebugEvent`/`ContinueDebugEvent` failures (`:87/99`) are fatal Win32 errors mid-loop. Keep on `throw`. Don't conflate `ProcessStatus`/`continueStatus` with C++ error returns.
2. **VEH path (`VehCoverageEngine.cpp`, `ExceptionHandler`, `BranchCondition`)** — a vectored handler must not throw C++ exceptions across the SEH boundary. `ExceptionHandlerStatus` is a *hardware*-exception status enum; do not rework into `expected`.
3. **`std::execution::par` catch requirement** — `RawPdbReader.cpp:336/350`, `CodeCoverageRunner.cpp:105`: escaping exception → `std::terminate`. Converting `ReadModule`/`Enumerate` to `expected` directly *benefits* these sites; until then the `catch(...)` must remain.
4. **`AmbiguousPathException` carries structured data** (three paths + getters), consumed at `UnifiedDiffCoverageFilter.cpp:97-106`. The `expected` error type replacing it must carry those three paths, not just a string.
5. **Third-party callbacks** — glaze returns errors by value (already `expected`-friendly); pcre2 is wrapped (`Regex` ctor → factory returning `expected`); ctemplate/DIA use return-code checks. No first-party SEH-in-C++ hazard beyond the VEH handler.
6. **RAII / `ScopedAction`** — destructors must not throw; error propagation out of cleanup stays exception-free.
7. **Control-flow-not-error uses** — `MonitoredLineRegister.cpp:561-577` (ReadProcessMemory throw = "unreadable, fall back") and `CodeCoverageRunner.cpp:88-109` (best-effort skip). Converting both ends to `expected` removes throw-for-control-flow — net win — but preserve the fallback semantics exactly.
8. **DLL ABI** — exported exceptions use `#pragma warning(disable:4275)`. `std::expected<T, ErrorType>` in exported signatures means `ErrorType` must be an exported, ABI-stable type.

## 6. Recommended migration boundaries

**Phase 1 — leaf, clearly-recoverable, low fan-in**
- `Tools::ReadProcessMemory`/`WriteProcessMemory` (already recoverable-consumed at `MonitoredLineRegister:573`; de-risks the `par` lambdas).
- `Tools::Regex` construction → factory `static std::expected<Regex,E> Compile(...)`.
- `Tools::MappedFile`, `PEFileHeader`, `GetExecutableFolder`, `CreateParentFolderIfNeeded`.
- `Exporter::ColumnarCoverage::Read`/`ReadFile`/`IsColumnarFile`, `CoverageDataDeserializer::Deserialize`, `FastCoverageImporter` → `expected<CoverageData,E>`.

**Phase 2 — parse/validation layer (has a catch seam already)**
- `UnifiedDiffParser::Parse`, `PathMatcher::Match` (keep structured ambiguity data in `E`), `UnifiedDiffCoverageFilter`.
- Options: internal `OptionsParser::Parse(argc,argv)` → `expected<Options,E>`; public overload keeps returning `std::optional<Options>` (its `:364` catch becomes `if (!result) ShowExplanation(...)`). Dovetails with the **glaze** parser, which already returns `result.error` by value — the `:408` throw is a needless value→exception→value round-trip that `expected` eliminates end-to-end.
- `DebugInformationEnumerator::Enumerate` → `expected`; simplifies the two `par` catch sites.

**Leave on exceptions** — the debug-event loop (`Debugger`), the VEH/`ExceptionHandler`/`BranchCondition` path, all category-(c) `THROW_LAST_ERROR` Win32 sites, category-(b) invariants; and the `main.cpp`/`OpenCppCoverage.cpp` top-level catches (final safety net).

**Error-type design**
- One shared, DLL-exported error struct in Tools, e.g. `Tools::Error { std::wstring message; std::optional<int> win32Code; }`, reused across DLLs so exported `std::expected<T, Tools::Error>` signatures don't multiply ABI types. Preserves what `ExceptionBase` carries (the `file:line`-prefixed message; `THROW_LAST_ERROR` → `win32Code`).
- Add a structured payload only where consumed — currently just `AmbiguousPathException`'s three paths.
- Incremental bridges: `expected→throw` (`.or_throw()` at un-migrated callers) and `catch→expected` (extend the existing `Tools::Try` idiom, `Tool.hpp:34`).

**Interaction with in-flight work**
- **spdlog:** the logging migration is complete — `Tools/Log.cpp` now routes through a process-wide `spdlog::logger` (no boost::log, no locale-imbue swallow). The log-and-continue handler at `OpenCppCoverage.cpp:302` is the only exception site touching logging; independent of the swap.
- **glaze CLI:** already value-based; Phase 2 **removed** the single `throw` that used to sit at `OptionsParser.cpp:408`, so the options path is now throw-free end-to-end (see *Implementation status*).

**Totals:** ~104 production throw sites (≈65 recoverable / ≈30 invariant / ≈12 exceptional), ~18 catch clauses (2 top-level, ~3 convert-to-message, ~4 swallow-by-design), across 6 modules; ~25 `*_DLL`-exported throwers, of which the Tools leaf functions, the Exporter binary-format readers, and the FileFilter/OptionsParser parse layer are the recommended first conversions. Zero first-party SEH; the debug-loop and VEH machinery is separate from C++ exceptions and must remain untouched.
