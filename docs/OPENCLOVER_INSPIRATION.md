# Porting OpenClover coverage statistics & painting to talea-cpp-cov

> Design exploration — not a commitment. Grounds every claim in the source
> checkouts under `inspiration/` (all git-ignored, local reference only):
> `inspiration/openclover/clover` (OpenClover 5.x), `inspiration/jenkins-ci/coverage-model`
> (the Jenkins Coverage plugin's parser library), and
> `inspiration/talea-cpp-covPlugin` (the Visual Studio plugin).

## 1. Goal

The original Clover — especially its Eclipse source painting — was unusually
pleasant to read. This document explores which of OpenClover's **coverage
statistics** and **source-painting styles** could realistically be brought to
talea-cpp-cov's outputs, evaluated against the two consumers named in the
brief: the **Jenkins Coverage plugin** and the **Visual Studio plugin** — plus
the two surfaces we fully control (our **Cobertura** and **HTML** exporters).

The key framing: talea-cpp-cov produces *data*; the *painting* happens in
consumers. So most "port the visuals" work is really **"emit richer data so the
consumers can paint it,"** plus enhancing our own HTML exporter, which is the
one painter we own end to end.

## 2. Target reality check

### 2.1 Jenkins Coverage plugin — alive, our main target
The modern Coverage plugin (`plugins.jenkins.io/coverage`) is actively
maintained and already consumes our **Cobertura** output. Its `CloverParser`
(`inspiration/jenkins-ci/coverage-model/.../parser/CloverParser.java`) reads
only: `<file name path>`, `<class name>`, `<metrics statements coveredstatements
conditionals coveredconditionals methods coveredmethods>`, and `<line num type
count truecount falsecount>` (only `type=stmt|cond`). It renders **line**
coverage (from `<line>` hits) plus **branch/instruction/method** as aggregate
counts. It does **not** reconstruct per-line partial-branch detail from Clover's
`truecount`/`falsecount` — those are used only as a boolean "line hit?".

Its **Cobertura** parser, by contrast, *does* turn our per-line
`condition-coverage="50% (1/2)"` + `<conditions>` into partial (amber) branch
lines. **Conclusion:** for Jenkins, Cobertura is already the richer channel; a
Clover-schema exporter would add nothing it can't already show, and would lose
our partial-branch fidelity. No work recommended here beyond what §5 adds to
Cobertura.

### 2.2 Visual Studio — the old plugin is dead, but VS 2026 ships coverage
The legacy `talea-cpp-cov/talea-cpp-covPlugin` is **GitHub-archived**
(`isArchived: true`); real development stopped in **2020** (96 commits in 2016 →
15 in 2020 → nothing but a 2026 archival README), and it targets VS2013–2017
packaging. Mechanically it showed a **coverage-tree tool window**
(`VSPackage/CoverageTree/…`, per-module/file rates) — there is **no editor line
painting** in it (no `ITagger` / `IWpfTextViewMargin` / adornment / text-marker
code anywhere in the checkout). It is neither live nor a suitable target.

**The important change:** as of **Visual Studio 2026, code coverage ships in
Community and Professional** (previously Enterprise-only). The engine is
Microsoft's `dotnet-coverage` / `Microsoft.CodeCoverage` tooling, and its
`merge`/convert side reads exactly three formats: **`coverage`** (the native
binary `.coverage` the IDE opens and colors), **`cobertura`** (Cobertura XML),
and **`xml`** (Microsoft's coveragexml) — *not* lcov/Clover/JaCoCo. Its
`collect`/instrument side is .NET-only ("native modules are not supported"), but
the read/merge/convert side is **format-agnostic**, so it ingests a C++ Cobertura
file happily.

Because **we already emit Cobertura**, this is now the **primary, first-party VS
path** — no plugin required:

```
talea-cpp-cov --export_type cobertura:cov.xml -- app.exe
dotnet-coverage merge -o cov.coverage -f coverage cov.xml   # binary the IDE opens
```

Caveat to verify in practice: how richly the VS results window highlights source
from a *converted external* report (full editor coloring vs. summary %) depends
on source-path resolution — worth a real-world check before relying on it.

Other live VS-side options:
- **VS Code + Coverage Gutters** — reads our Cobertura (also Clover/JaCoCo/LCov),
  already paints covered/uncovered/**partial** gutters; richer Cobertura branch
  data improves this for free.
- **Our HTML report opened in VS** — zero-integration, and the surface we own.
- **A new modern VSIX** (editor adornments via `ITagger`/`IWpfTextView`) — the
  only way to get bespoke in-editor painting, but a substantial new project and
  out of scope here (and largely unnecessary now that VS 2026 ingests Cobertura).

## 3. What OpenClover offers

### 3.1 Coverage statistics
The metric surface is `org.openclover.core.api.registry.*Metrics`:

| Clover metric | Meaning | Maps to native C++? |
|---|---|---|
| statements / covered | executable statements hit | **Yes** — ≈ our lines |
| branches / covered (`truecount`,`falsecount`) | each conditional scored true & false separately | **Yes** — we already collect true/false per Jcc |
| methods / covered | functions entered | **Partial** — we have PDB function ranges but don't emit function coverage |
| **elements = statements + branches** | the blended denominator | Yes (see TPC below) |
| **TPC** = `getPcCoveredElements()` | the single headline % | **Yes**, minus methods |
| complexity / density | cyclomatic complexity | No — we don't compute it |
| per-test coverage, tests run/passed/failed, test time | which test covered which line; test optimization | **No** — needs a test-aware runtime we don't have |
| per-line **hit counts** | execution count per line | **No (mostly)** — we're boolean; only fast-mode has counts, capped at 256 |

The blended **"Clover coverage" (TPC)** is not a special routine — it falls out
of `getPcCoveredElements() = coveredElements/elements`
(`BlockMetrics.java:190`) because method entries are counted as statements
(`FullMethodInfo.java:482`) and each conditional contributes two branches
(`FullMethodInfo.java:500`), giving the documented
`TPC = (CT+CF+SC+MC) / (2C+S+M)`.

### 3.2 Source-painting styles
- **Eclipse editor** (`clover-eclipse/.../ui/editors/java/`): a
  `CoverageAnnotationModel` paints each line via `CoverageAnnotation` —
  `SWT.COLOR_GREEN` covered, `SWT.COLOR_RED` uncovered, a light highlight
  `RGB(199,235,242)`; a **gutter ruler column**
  (`CoverageAnnotationRulerColumn`) and **per-test contribution** overlays
  (`LineTestContributionsComputation`) showing which tests hit a line.
- **HTML source** (`reporters/html/source/SourceRenderHelper.java` + `html_res`
  CSS): line classes `srcLine`, `srcLineHilight`, `srcLineFiltered`, a
  `coverageCount` hit-count column, and `coveredByTest` / `missedByTest` /
  `coveredByTestUniq` per-test shading. Plus a rich dashboard: sortable metric
  tables, a **treemap**, coverage "clouds", and **historical trend** pages.

## 4. What talea-cpp-cov produces today (baseline)

- **Data model**: boolean line coverage (`LineCoverage.HasBeenExecuted`) +
  per-line branch outcomes (`BranchCoverage` true/false), from the debugger, VEH,
  or fast mode. Function *ranges* are known (PDB, used for run-merging and the
  fast-mode template axis) but **not** turned into a function-coverage metric.
  **No general hit counts** (the debugger retires a breakpoint on first hit; VEH
  sets `hit=1`; only fast-mode counters carry counts, and they wrap at 256).
- **Cobertura** (`Exporter/CoberturaExporter.cpp`): line-rate, branch-rate,
  `branches-covered/valid`, and per-line `branch="true"
  condition-coverage="P% (c/t)"` + `<conditions>` when `--branch_coverage` is on.
  `<methods/>` is emitted **empty**.
- **HTML** (`Exporter/Html/HtmlFileCoverageExporter.cpp`): a Clover-lite paint —
  executed line background `#dfd` (green), unexecuted `#fdd` (red). It keys
  **only on line-executed** and **ignores branch data entirely**; no partial
  (amber) lines, no hit counts, no branch markers, no method view.

## 5. Porting opportunities (feasibility matrix)

| # | Clover feature | For C++ | Target(s) | Change needed | Effort |
|---|---|---|---|---|---|
| A | **Partial/amber source lines** (line ran, but a branch on it not fully taken) | Yes — data already in model | **Our HTML** (Jenkins/Coverage-Gutters already do it from our Cobertura) | Add a 3rd state to `HtmlFileCoverageExporter`: if the line has a `BranchCoverage` not both-taken → amber `#ffd`. Pure exporter change | **Low** |
| B | **Branch true/false markers + tooltip** ("1/2 — true taken, false not") on partial lines | Yes | Our HTML | Render a small badge/tooltip from the per-line `BranchCoverage` we already carry | Low |
| C | **Blended TPC coverage %** headline number | Yes (no methods) → `(coveredLines+CT+CF)/(lines+2C)` | Our HTML index, console summary | Compute in `CoverageRate`; show as one figure alongside line/branch | **Low** |
| D | **Method / function coverage** metric | Yes — we already enumerate PDB function ranges | Cobertura `<methods>` (today empty), our HTML, Jenkins (Cobertura+Clover both read methods) | Mark a function covered if any of its lines executed; fill Cobertura `<method>` elements + a method metric | **Medium** |
| E | **Per-line hit counts** ("executed N times") + Clover's count gutter | **Hard** — we're boolean by design | Our HTML, Cobertura `hits` (currently 0/1) | Needs a counting collection mode (don't retire breakpoints; increment) with real overhead; fast-mode could surface its capped counts as a first step | **High** |
| F | **Richer HTML dashboard** — sortable metric tables, treemap, historical trend | Yes (presentation only) | Our HTML index | Enhance the HTML templates/JS; treemap + a stored history file for trends | Medium |
| G | **Per-test coverage / test optimization** | **No** — needs test-aware runtime & framework hooks | — | New runtime concept (coverage "slices" per test); large | Out of scope |
| H | **Cyclomatic complexity** | No — not computed | — | CFG analysis over disassembly; low value for a coverage tool | Out of scope |
| I | **Visual Studio integration** | Yes — via Cobertura, no plugin | **VS 2026 built-in** coverage (Community+), VS Code Coverage Gutters | Both read our existing Cobertura; VS 2026 needs a one-line `dotnet-coverage merge -f coverage` convert. A bespoke VSIX would paint the editor natively but is unnecessary | Free / Low (verify source-path resolution) |

## 6. Recommendation & phasing

**Phase 1 — cheap, high-value, all in exporters we own (A, B, C):**
Bring our **HTML report to parity with what Jenkins/Coverage-Gutters already
show from our Cobertura**: paint partial-branch lines amber (A), add per-line
branch true/false badges/tooltips (B), and surface one blended TPC-style
percentage (C). This closes the most visible gap — today our own HTML is
*less* informative about branches than a third-party Cobertura consumer — using
data we already collect, with no runtime changes.

**Phase 2 — function coverage (D):**
Turn the PDB function ranges we already have into a method/function-coverage
metric, fill Cobertura's currently-empty `<methods>`, and add a function view to
HTML. This lights up the "methods" column in both the Jenkins Cobertura and
Clover parsers and moves TPC toward Clover's full `(…+MC)/(…+M)`.

**Phase 3 — presentation polish (F):** a Clover-style HTML dashboard (sortable
tables, treemap, optional history) if the reports become a primary surface.

**Visual Studio (I) — no code needed:** the archived plugin is dead, but both
**VS 2026's built-in coverage** (Community and above, via a one-line
`dotnet-coverage merge -f coverage cov.xml` convert of our Cobertura) and **VS
Code Coverage Gutters** consume the Cobertura we already emit. So the Phase-1
Cobertura enrichments (branch data) benefit the VS side for free; a bespoke VSIX
is unnecessary.

**Deliberately not pursued:** per-line hit counts (E — fundamental to our
boolean, first-hit collection model; revisit only if a counting mode is wanted),
and per-test coverage / test optimization (G) and complexity (H) (need machinery
we don't have and that a C++ external-process profiler isn't well placed to add).

**Net:** the single best "port from Clover" is not a statistic at all — it's
adopting Clover's **green / red / amber line semantics with branch detail** in
our own HTML exporter (Phase 1), because the branch data is already in the model
and the amber-partial convention is exactly what made Clover-in-Eclipse read so
well.
