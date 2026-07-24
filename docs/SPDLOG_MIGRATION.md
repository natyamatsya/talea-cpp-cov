# Migration strategy: boost-log → spdlog, then stream → format strings

## Two goals

1. **Get off boost-log onto spdlog** so logging links statically (drops the
   `boost_log` / `boost_filesystem` / `boost_thread` DLLs — the last
   un-static-linkable dependency; see `static-linking-leaf-libs`).
2. **Reach idiomatic format-string logging** — `LOG_INFO("loaded {} in {} ms",
   module, ms)` — with compile-time-checked format strings.

The RAII stream adapter is the **bridge between them, not the destination.** We
land on spdlog first with the 65 `LOG_X << …` sites untouched (low risk, one
engine swap), then convert them to format strings **in verified batches**, and
**delete the adapter** once the last site is gone.

## Why boost-log can't stay

The `LOG_*` macros in `Tools/Log.hpp` expand — inline, in every module — to a
boost-log global-logger singleton:

```cpp
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(globalLogger, ...severity_logger...)
#define LOG_INFO BOOST_LOG_SEV(globalLogger::get(), ...info...)
```

That singleton is one shared instance only while boost-log is the shared DLL.
Linked statically, each of our DLLs would get its own logger core and the sinks
configured in `Tools.dll` would silently miss lines emitted elsewhere.

## The surface

- **65 call sites across 22 files**, every one **stream-style**
  (`LOG_INFO << L"msg " << widePath << n;`), on a **wide** (`wchar_t`) logger that
  formats to UTF-8 file + console.
- A custom `operator<<` for `std::set<T>` (+ a `LogCollection` helper) in
  namespace `boost::log` in `Tools/Log.hpp`.
- Public `Tools` API (only non-`Log.*` logging touchpoints):
  `InitConsoleAndFileLog(path)`, `SetLoggerMinSeverity(boost…severity_level)`
  *(leaks a boost type)*, `EnableLogger(bool)`,
  `InitLoggerOstream(boost::shared_ptr<std::ostringstream>)` *(leaks a boost type;
  a test captures error output through it)*.
- Callers: `talea-cpp-cov.cpp` (init + severity from `--verbose/--quiet`) and
  three test files.

## End state

```cpp
// Tools/Log.hpp — final
enum class LogLevel { Trace, Debug, Info, Warning, Error };

// Lazy: format only if the level is enabled. Format string checked at compile
// time. Args of our wide/domain types work via the formatters below.
#define LOG_INFO(...)  ::Tools::LogIf(::Tools::LogLevel::Info,  __VA_ARGS__)
// ...one per level.
```

Format strings are **narrow UTF-8 literals**; wide and domain arguments are made
`{}`-formattable once, centrally, so call sites stay clean:

- fmt formatters in `Tools` for `std::wstring`, `std::wstring_view`,
  `const wchar_t*`, and `std::filesystem::path` → `Tools::ToUtf8String`.
- The `std::set<T>` / collection case becomes a `Tools::Join(range)` helper (or a
  range formatter), reproducing the current `[a, b, c]` rendering.

The formatters must render **byte-identically** to today's `operator<<` — that is
what lets the parity gate (below) prove each batch is behaviour-preserving.

## Phase 0 — spdlog under the bridge adapter

Land spdlog with **zero call-site changes**:

```cpp
// Tools/Log.hpp — transitional
class TOOLS_DLL LogStream {              // move-only; exported ctor/dtor
public:
    explicit LogStream(LogLevel);
    ~LogStream();                        // wide→UTF-8, emit to spdlog (in Tools.dll)
    template <class T> LogStream& operator<<(const T& v){ stream_ << v; return *this; }
private:
    LogLevel level_; std::wostringstream stream_;
};
#define LOG_INFO ::Tools::LogStream{::Tools::LogLevel::Info}   // still stream-style
```

Because the emitting destructor lives in `Tools.dll`, **spdlog headers are
included only by `Tools/Log.cpp`** → no registry duplication → spdlog links
static. `Tools/Log.cpp` sets up one thread-safe (`_mt`) spdlog logger: a stderr
sink (`[level] message`) and, after `InitConsoleAndFileLog`, a file sink
(`[%Y-%m-%d %H:%M:%S] [level] message`) — matching today's format byte-for-byte.

De-boost the public API here too: `SetLoggerMinSeverity(LogLevel)`,
`InitLoggerOstream(std::shared_ptr<std::ostringstream>)` (spdlog `ostream_sink`);
update the ~3 callers. `vcpkg.json`: add `{"name":"spdlog","features":["fmt"]}`
(reuse our static fmt), remove `boost-log`; add `spdlog` to the static PORT set in
`custom-triplets/x64-windows.cmake`. Verify: all suites green, `boost_*` DLLs gone
(`dumpbin`), `log_parity.py` empty diff.

At the end of Phase 0 we are fully on spdlog; boost-log is gone; the adapter
carries every site unchanged.

## Phase 1 — introduce the format API alongside the adapter

Add the `LOG_*(...)` format macros + the wide/domain fmt formatters, **coexisting**
with the stream adapter. To let both spellings live during the transition, the
stream form is temporarily renamed `LOG_*_STREAM` (mechanical, tree-wide) and the
new `LOG_*(...)` is the format form. No behaviour change; parity stays empty.

## Phases 2..N — convert in verified batches

Batch = one file (or a small module). Per batch, in order:

1. **Codemod** the file's `LOG_*_STREAM << … ;` into `LOG_*("…", args)`
   (`tools/log_codemod.py`, below). Easy sites rewritten automatically; hard sites
   left in place and flagged.
2. **Human review** of the diff — especially escaped braces, arg order, and any
   wide/domain arg relying on a formatter.
3. **Build** — the compile-time format-string check now catches
   placeholder/arg-count mismatches for free.
4. **Unit tests** for the affected module.
5. **`log_parity.py`** — run the command battery at high verbosity, capture
   `LastCoverageResults.log`, normalize volatile fields, diff **old-binary vs
   new-binary**. Must be empty. A non-empty diff means a mis-rewrite → fix before
   moving on.

Suggested order: leaf modules with the simplest lines first (`Tools`,
`FileFilter`), then `Exporter`, then `TaleaCppCov` (the bulk), then
`talea-cpp-cov`. ~5–10 sites per batch keeps each parity diff trivial to reason
about.

## Phase final — remove the bridge

When no `LOG_*_STREAM` remains: delete `LogStream`, the `_STREAM` macros, and the
`std::set` `operator<<` shim (now replaced by `Tools::Join`). `Tools/Log.hpp` ends
with only the `LogLevel` enum and the `LOG_*(...)` format macros. Turn on the
`log_audit.py` CI check.

## Python tooling

1. **`tools/log_audit.py`** — inventory + **classify** every site. Parses each
   `LOG_*` statement, splits the `<<` chain into literal segments vs expression
   args, and tags:
   - *simple* — literals + plain value args (auto-convertible),
   - *hard* — stream manipulators (`std::hex`, `setw`), a custom-`operator<<`
     type, multi-line, or inside a conditional/loop expression.
   Emits the per-file batch worklist and counts. Also enforces the invariants
   (fails CI on `boost::log` outside `Log.*`, or a `spdlog` include outside
   `Log.cpp` — the rule that keeps spdlog static).

2. **`tools/log_codemod.py`** — the batch rewriter. Transforms a stream chain into
   a format call: string literals → format text (with `{`→`{{`), each non-literal
   operand → `{}` plus its verbatim source as an argument; `L"…"` → `"…"` for
   ASCII literals. It only touches *simple* sites; *hard* ones are skipped and
   reported for manual conversion. Preferred backend is libclang over a
   `compile_commands.json`; since this repo is MSBuild (no CMake), the fallback is
   a token-level rewriter scoped to the single-statement `LOG_* << … ;` shape,
   which covers the large majority — the codemod is **assistive** (propose →
   human-review → parity-verify), not blind.

3. **`tools/log_parity.py`** — the regression gate used after Phase 0 and every
   batch. Runs a fixed battery (`--help`, a real `--verbose` coverage run over
   `TestCoverageConsole`, an error case), captures stdout/stderr +
   `LastCoverageResults.log`, normalizes timestamps / temp paths / durations with
   regexes, and `difflib`-diffs the two binaries. Empty diff = behaviour
   preserved. This is the safety mechanism that makes batching safe: a bad rewrite
   surfaces as a diff in that batch and nowhere else.

## Risks & mitigations

- **Formatter fidelity** — a wide/collection formatter that renders differently
  from `operator<<` shows up immediately as a `log_parity.py` diff in the first
  batch that logs that type; fix the formatter once, centrally.
- **Hard sites** (manipulators / custom types) — converted by hand, still
  parity-gated. Small in number; `log_audit.py` lists them up front.
- **The capture test** (`InitLoggerOstream`) asserts on log text — pin the spdlog
  pattern to reproduce `[level] message`; only change the expectation on a
  deliberate format change.
- **Concurrency** — spdlog `_mt` logger; the parity battery includes a parallel
  export run.
- **Static invariant** — `log_audit.py` forbids `spdlog` includes outside
  `Tools/Log.cpp`; without it a stray include would duplicate spdlog's registry
  and silently re-create the boost-log problem.
