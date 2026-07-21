# Migration analysis: exceptions → `std::expected<T, E>`

Analysis only — no code changed. Excluded `inspiration/`, `vcpkg_installed/`,
`Build/ThirdParty/`, `*/x64/*`. Paths are relative to the repo root.

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
| `CppCoverage::CppCoverageException` | `CppCoverage/CppCoverageException.hpp:29` | `THROW` (31), `THROW_LAST_ERROR` (32, appends `GetErrorMessage(lastErrorCode)`) |
| `Exporter::ExporterException` | `Exporter/ExporterException.hpp:21` | `THROW` (23) |
| `FileFilter::FileFilterException` | `FileFilter/FileFilterException.hpp:21` | `THROW` (23) |
| `CppCoverageConsole::CppCoverageConsoleException` | `OpenCppCoverage/OpenCppCoverageException.hpp:21` | `THROW` (23) — no live throw sites |

**Custom exceptions deriving from `std::runtime_error` (not the macro)**
- `Exporter::InvalidOutputFileException` — `Exporter/InvalidOutputFileException.hpp:31` (carries `path` + `outputKind`).
- `FileFilter::AmbiguousPathException` — `FileFilter/AmbiguousPathException.hpp:27` (three `path` members + getters — the **only** custom exception carrying structured data; see §5).
- `FileFilter::UnifiedDiffParserException` — `FileFilter/UnifiedDiffParserException.hpp:28` (named static message constants).
- `Plugin::OptionsParserException` — `Plugin/OptionsParserException.hpp:24` (cross-DLL option-validation error).

**std / system / third-party types thrown or caught**
- `std::runtime_error` — thrown directly in ~a dozen places (`Plugin/Exporter/FileCoverage.cpp:36/48/64/80`, `FileFilter/UnifiedDiffCoverageFilter.cpp:38/105`, `CppCoverage/Process.cpp:154`, `Exporter/Plugin/PluginLoader.hpp:40/47/59`, `Exporter/Plugin/ExporterPluginManager.cpp:85/154`, `Exporter/Binary/CoverageDataDeserializer.cpp:38`, `Tools/Tool.cpp:58/77`).
- `std::exception` — catch-all base at every top-level/best-effort handler (§3).
- `std::filesystem::filesystem_error` — not caught by name; surfaces as `std::exception` at top level.
- **glaze** — returns errors by value (`result.error`), converted to a thrown `OptionsParserException` at `OptionsParser.cpp:407`. Does not throw across the boundary.
- **pcre2** — wrapped by `Tools::Regex`; compile failure → `THROW` at `Tools/Regex.cpp:70`.
- **ctemplate** — failures detected by return value → `THROW` (`TemplateHtmlExporter.cpp:61/86/91`).
- **DIA (COM)** — `HRESULT` checks → `THROW` throughout `DebugInformationEnumerator.cpp` (12 sites).
- **SEH / `__try` / `__except`** — none in first-party code. Structured exceptions handled via the debug-event loop and VEH, not `__try`.

## 2. throw-site inventory

~112 production throw sites (160 raw `throw`/`THROW*` incl. macros/comments/tests).

| Module (DLL) | Sites | Key files |
|---|---|---|
| CppCoverage.dll | ~64 | `OptionsParser.cpp` (14), `DebugInformationEnumerator.cpp` (12), `FastCoverageImporter.cpp` (8), `Debugger.cpp` (7), `HandleInformation.cpp` (5), `BranchCoverageManager.cpp` (4), `BreakPoint.cpp` (2), `Process.cpp` (2), `Handle.hpp` (2), `VehCoverageEngine.cpp` (3), + 1 each in several files |
| Exporter.dll | ~28 | `Binary/ColumnarCoverage.cpp` (10), `Plugin/ExporterPluginManager.cpp` (5), `Html/TemplateHtmlExporter.cpp` (3), `Plugin/PluginLoader.hpp` (3), `Html/HtmlFolderStructure.cpp` (2), + 1 each |
| Tools.dll | ~11 | `Tool.cpp` (4), `ProcessMemory.cpp` (2), `PEFileHeader.cpp` (2), `MappedFile.cpp` (1), `Regex.cpp` (1), `Tool.hpp` (1) |
| FileFilter.dll | ~5 | `UnifiedDiffParser.cpp` (2), `UnifiedDiffCoverageFilter.cpp` (2), `PathMatcher.cpp` (1) |
| Plugin.dll | ~4 | `Exporter/FileCoverage.cpp` (4) |
| OpenCppCoverage.exe | 0 | (macro definition only) |

**Classification**

- **(a) RECOVERABLE — ~70 sites — prime `std::expected` candidates.**
  - Option/user-input validation (~17): all of `OptionsParser.cpp` (`:56,73,131,146,183,189,217,225,234,282,289,407,433,476`), `ExportOptionParser.cpp:146`, `Options.cpp:35`.
  - Missing/unreadable/malformed input files (~25): `FastCoverageImporter.cpp` (8), `ColumnarCoverage.cpp` (10), `CoverageDataDeserializer.cpp:38`, `MappedFile.cpp:37`, `StartInfo.cpp:35`, `PEFileHeader.cpp:35/49`, `UnifiedDiffCoverageFilter.cpp:38`.
  - Parse failures (~4): `UnifiedDiffParser.cpp:75/263`, `PathMatcher.cpp:90` + rethrow `UnifiedDiffCoverageFilter.cpp:105`.
  - Bad regex pattern (1): `Regex.cpp:70`.
  - Symbol/PDB resolution (~12): `DebugInformationEnumerator.cpp`.
  - Output-file/template (~8): `CoberturaExporter.cpp:416`, `CoverageDataSerializer.cpp:40`, `TemplateHtmlExporter.cpp:61/86/91`, `HtmlFileCoverageExporter.cpp:141`.
  - Plugin load/bad plugin (~5): `PluginLoader.hpp:40/47/59`, `ExporterPluginManager.cpp:85/154/198`.
  - Launch failure (2): `VehCoverageEngine.cpp:1110/1121`.
- **(b) INVARIANT / programmer-error — ~30 sites — keep `throw` / `assert` / `std::unreachable`.**
  `BranchCoverageManager.cpp:108/122/163`, `BranchCondition.cpp:71` & `VehCoverageEngine.cpp:137`, `ExecutedAddressManager.cpp:139`, `MonitoredLineRegister.cpp:587`, `Handle.hpp:36/38`, `Process.cpp:117`, `Debugger.cpp:221/238/261/274/283`, `HtmlFolderStructure.cpp:100/120`, `LoadedPlugin.hpp:52`, `ExporterPluginManager.cpp:132`. `FileCoverage.cpp:36/48/64/80` = documented "throw on invalid argument" contract (INVARIANT-at-boundary).
- **(c) TRULY EXCEPTIONAL — ~12 sites — unexpected Win32/COM/OOM.**
  `Debugger.cpp:88/100` (`THROW_LAST_ERROR`; also §5), `BreakPoint.cpp:112/124`, `BranchCoverageManager.cpp:170` (Get/SetThreadContext), `HandleInformation.cpp:40/104`, `ProcessMemory.cpp:57/85` (but caught-as-recoverable at `MonitoredLineRegister.cpp:509`), `Tool.cpp:40/58/77`.

## 3. catch-site inventory

~22 `catch` clauses (no `__try`/`__except`).

**Top-level (exception → exit code / message)**
- `OpenCppCoverage/main.cpp:33-46` — outermost `try`; `catch(const std::exception&)` prints `"Error: " + e.what()`, `catch(...)` → `"Unknown error"`, both `return FailureExitCode`. *The* boundary converting escaped exceptions to exit code.
- `OpenCppCoverage/OpenCppCoverage.cpp:325-336` — inner guard around `Run(...)`; `catch(std::exception&)` → `LOG_ERROR`; sets `FailureExitCode`, continues to display warnings.

**Convert-to-user-message seams**
- `CppCoverage/OptionsParser.cpp:363` — `catch(const OptionsParserException&)` → `ShowExplanation()`, returns `std::nullopt`. Natural `expected` seam for the options layer.
- `FileFilter/UnifiedDiffCoverageFilter.cpp:96` — `catch(const AmbiguousPathException&)` → reformats the three paths, rethrows `std::runtime_error` (`:105`).
- `Exporter/Plugin/ExporterPluginManager.cpp:140-151` — `catch(OptionsParserException&)→throw;`, `catch(std::exception&)`/`catch(...)` → wrap plugin fault. Isolates third-party plugin faults at the DLL boundary.
- `Tools/Tool.hpp:45-54` — `Try<Exception>(fct, buildErrorMsg)` translates any error into a module exception. `Tool.cpp:123-127` similar.

**Swallow-by-design / log-and-continue (preserve as `expected`-ignoring paths)**
- `CppCoverage/RawPdbReader.cpp:342` — `catch(...)` in a `std::execution::par` lambda sets `anyFailed` (comment `:328`: escaping exception → `std::terminate`). **Hard constraint (§5).**
- `CppCoverage/CodeCoverageRunner.cpp:104` — `catch(...)` in a `par` preparse lambda: best-effort skip.
- `CppCoverage/MonitoredLineRegister.cpp:509` — `catch(const std::exception&)`: unreadable code → fall back to one probe per line (deliberate throw-as-control-flow; good `expected` candidate on both ends).
- `Tools/Log.cpp:76` — swallows a failed `".UTF-8"` locale imbue.
- `Tools::Try` (`Tool.hpp:34` overload) already returns `std::optional<std::wstring>` — an `expected`-like idiom.

## 4. Cross-DLL throwing exports (signatures that would change) — most important

**Tools.dll** — `GetExecutableFolder()` (`Tool.hpp:57`→`Tool.cpp:40`), `CreateParentFolderIfNeeded()` (`Tool.cpp:162`), `LocalToWString/Utf8ToWString/ToLocalString/ToUtf8String` (`Tool.cpp:58/77`), `ReadProcessMemory/WriteProcessMemory` (`ProcessMemory.cpp:57/85`; already recoverable-consumed), `Regex` ctor (`Regex.cpp:70`), `MappedFile` ctor (`MappedFile.cpp:37`), `PEFileHeader` (`PEFileHeader.cpp:35/49`), `Try<>` template seam.

**CppCoverage.dll** — `OptionsParser::Parse` (internal throws `OptionsParserException`; public 3-arg overload already returns `std::optional<Options>` at `:363`), `ExportOptionParser` (`:146`), `DebugInformationEnumerator::Enumerate` (12), `FastCoverageImporter` (8, file-format), `Debugger::Debug` (Win32 — debug loop, §5), plus `BreakPoint`/`BranchCoverageManager`/`ExecutedAddressManager`/`HandleInformation`/`Process`/`VehCoverageEngine`/`CodeCoverageRunner` (mostly category b/c → leave on `throw`), `Options`.

**Exporter.dll** — `CoberturaExporter::Export` (`InvalidOutputFileException`, `:416`), `CoverageDataSerializer`/`CoverageDataDeserializer::Deserialize` (`:40`/`:38`), **free `EXPORTER_DLL`** `ColumnarCoverage::Write/Read/IsColumnarFile/ReadFile` (`ColumnarCoverage.hpp:77/81/85/88`; `Read`/`ReadFile` = clearest `expected<CoverageData,E>`), `TemplateHtmlExporter`/`HtmlFileCoverageExporter`/`HtmlFolderStructure`, `ExporterPluginManager`.

**FileFilter.dll** — `UnifiedDiffParser::Parse` (`:75/263`), `UnifiedDiffCoverageFilter` (`:38/105`), `PathMatcher::Match` (`AmbiguousPathException`, `:90`).

**Plugin.dll** — `FileCoverage` line-add API (`:36/48/64/80`), `IExportPlugin::CheckArgument` (`IExportPlugin.hpp:50` — **contract requires throwing** `OptionsParserException`; changing it is a plugin-ABI break, §6).

## 5. Hazards / must-stay-exceptions

1. **Debug-event loop (`Debugger.cpp`)** — handles Windows *debug* exceptions; its own `WaitForDebugEvent`/`ContinueDebugEvent` failures (`:88/100`) are fatal Win32 errors mid-loop. Keep on `throw`. Don't conflate `ProcessStatus`/`continueStatus` with C++ error returns.
2. **VEH path (`VehCoverageEngine.cpp`, `ExceptionHandler`, `BranchCondition`)** — a vectored handler must not throw C++ exceptions across the SEH boundary. `ExceptionHandlerStatus` is a *hardware*-exception status enum; do not rework into `expected`.
3. **`std::execution::par` catch requirement** — `RawPdbReader.cpp:328/342`, `CodeCoverageRunner.cpp:104`: escaping exception → `std::terminate`. Converting `ReadModule`/`Enumerate` to `expected` directly *benefits* these sites; until then the `catch(...)` must remain.
4. **`AmbiguousPathException` carries structured data** (three paths + getters), consumed at `UnifiedDiffCoverageFilter.cpp:96-105`. The `expected` error type replacing it must carry those three paths, not just a string.
5. **Third-party callbacks** — glaze returns errors by value (already `expected`-friendly); pcre2 is wrapped (`Regex` ctor → factory returning `expected`); ctemplate/DIA use return-code checks. No first-party SEH-in-C++ hazard beyond the VEH handler.
6. **RAII / `ScopedAction`** — destructors must not throw; error propagation out of cleanup stays exception-free.
7. **Control-flow-not-error uses** — `MonitoredLineRegister.cpp:497-512` (ReadProcessMemory throw = "unreadable, fall back") and `CodeCoverageRunner.cpp:92-107` (best-effort skip). Converting both ends to `expected` removes throw-for-control-flow — net win — but preserve the fallback semantics exactly.
8. **DLL ABI** — exported exceptions use `#pragma warning(disable:4275)`. `std::expected<T, ErrorType>` in exported signatures means `ErrorType` must be an exported, ABI-stable type.

## 6. Recommended migration boundaries

**Phase 1 — leaf, clearly-recoverable, low fan-in**
- `Tools::ReadProcessMemory`/`WriteProcessMemory` (already recoverable-consumed at `MonitoredLineRegister:509`; de-risks the `par` lambdas).
- `Tools::Regex` construction → factory `static std::expected<Regex,E> Compile(...)`.
- `Tools::MappedFile`, `PEFileHeader`, `GetExecutableFolder`, `CreateParentFolderIfNeeded`.
- `Exporter::ColumnarCoverage::Read`/`ReadFile`/`IsColumnarFile`, `CoverageDataDeserializer::Deserialize`, `FastCoverageImporter` → `expected<CoverageData,E>`.

**Phase 2 — parse/validation layer (has a catch seam already)**
- `UnifiedDiffParser::Parse`, `PathMatcher::Match` (keep structured ambiguity data in `E`), `UnifiedDiffCoverageFilter`.
- Options: internal `OptionsParser::Parse(argc,argv)` → `expected<Options,E>`; public overload keeps returning `std::optional<Options>` (its `:363` catch becomes `if (!result) ShowExplanation(...)`). Dovetails with the **glaze** parser, which already returns `result.error` by value — the `:407` throw is a needless value→exception→value round-trip that `expected` eliminates end-to-end.
- `DebugInformationEnumerator::Enumerate` → `expected`; simplifies the two `par` catch sites.

**Leave on exceptions** — the debug-event loop (`Debugger`), the VEH/`ExceptionHandler`/`BranchCondition` path, all category-(c) `THROW_LAST_ERROR` Win32 sites, category-(b) invariants; `IExportPlugin::CheckArgument`'s throw-contract (plugin-ABI break — defer to a versioned API change); and the `main.cpp`/`OpenCppCoverage.cpp` top-level catches (final safety net).

**Error-type design**
- One shared, DLL-exported error struct in Tools, e.g. `Tools::Error { std::wstring message; std::optional<int> win32Code; }`, reused across DLLs so exported `std::expected<T, Tools::Error>` signatures don't multiply ABI types. Preserves what `ExceptionBase` carries (the `file:line`-prefixed message; `THROW_LAST_ERROR` → `win32Code`).
- Add a structured payload only where consumed — currently just `AmbiguousPathException`'s three paths.
- Incremental bridges: `expected→throw` (`.or_throw()` at un-migrated callers) and `catch→expected` (extend the existing `Tools::Try` idiom, `Tool.hpp:34`).

**Interaction with in-flight work**
- **spdlog:** the log-and-continue handlers (`OpenCppCoverage.cpp:331`, `Log.cpp:76`) are the only exception sites touching logging; independent of the swap.
- **glaze CLI:** already value-based; Phase 2 removes the single `throw` at `OptionsParser.cpp:407`, making the options path throw-free end-to-end.

**Totals:** ~112 production throw sites (≈70 recoverable / ≈30 invariant / ≈12 exceptional), ~22 catch clauses (2 top-level, ~4 convert-to-message, ~5 swallow-by-design), across 6 DLLs; ~30 `*_DLL`-exported throwers, of which the Tools leaf functions, the Exporter binary-format readers, and the FileFilter/OptionsParser parse layer are the recommended first conversions. Zero first-party SEH; the debug-loop and VEH machinery is separate from C++ exceptions and must remain untouched.
