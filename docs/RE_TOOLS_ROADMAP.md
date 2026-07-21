# Coverage for binary / reverse-engineering consumers

> **Status: parked / not the current focus.** Split out of the performance work
> ([docs/PERFORMANCE.md](PERFORMANCE.md)) because it is a different *product*
> direction — coverage consumed by binary tooling and fuzzers rather than
> rendered against source. Captured here so the analysis isn't lost; nothing
> below is scheduled.

OpenCppCoverage today maps coverage back to **source** (cobertura / HTML / line
%, branch taken-or-not). A separate audience consumes coverage at the **binary**
level — basic blocks and edges over the disassembly, or raw execution feedback.
Supporting them is a real opportunity, but it is orthogonal to the source-level
tool and easy to over-scope, so the key finding up front:

**Most of what is renderable here needs only an *exporter over data the existing
engines already collect* — not a binary rewriter.**

---

## 1. The three consumer worlds

Edge / hit-count / block coverage has three distinct consumers, and only one is
"rendering" in the familiar sense:

1. **Fuzzers** (AFL++, libFuzzer, honggfuzz) consume coverage as a raw
   shared-memory **bitmap** (AFL's 64 KB map, hit counts bucketed into 8 log₂
   buckets) to steer input mutation. No human-facing view — it's a control
   signal. This is what TinyInst is mainly built to feed.
2. **Binary / RE viewers** render coverage over the disassembly and function
   graph, and all speak one format — **drcov** (DynamoRIO's block-coverage
   format):
   - **Lighthouse** (IDA Pro) — per-basic-block coverage on the graph,
     aggregation across runs, per-function coverage %.
   - **Cartographer** / **dragondance** (Ghidra), **bncov** (Binary Ninja).
   - **Tenet** (Lighthouse's successor) — execution *traces* (ordering), a step
     beyond coverage.
   drcov is block **boolean** coverage (which blocks ran), rendered at the
   **binary/block** level, not over source.
3. **Source-level viewers** (cobertura → Jenkins / SonarQube / ReportGenerator,
   Coverage Gutters, our own HTML) have **no edge/hit-count concept** — they
   render line coverage. Map blocks back to source lines and the edge/count
   information collapses to line coverage (which the VEH engine already produces,
   faster — see §3).

---

## 2. Option A — drcov exporter (the cheap, high-value path)

**Goal:** view OpenCppCoverage results in Lighthouse / Ghidra (Cartographer) /
Binary Ninja (bncov).

**Key point: this needs no rewriter.** The debugger and VEH engines already know
every probe/block's module-relative address and whether it executed — that *is*
drcov's payload. So this is a new **exporter over existing data**, not a new
collection engine.

drcov v2 layout (well documented):
- An ASCII header (`DRCOV VERSION: 2`, flavour line).
- A **module table**: one row per module — id, base, end, path (the engines
  already track loaded module paths + bases).
- A **basic-block table**: fixed-size records `{ uint32 start (module-relative),
  uint16 size, uint16 module_id }`, one per *covered* block.

Mapping from our model: each executed probe/leader RVA + its run length is a
covered block; module id/base come from the loaded-module list the readback
already walks. Emit as a new `--export_type drcov:<file>` (or a plugin exporter).

Effort: **small** — an exporter + wiring, no engine changes. Risk: low. The main
subtlety is block *size* (drcov wants a byte length): the VEH/columnar probe
table already carries run lengths (LineRunScanner), so a block's `[leader,
nextLeader)` span is available. Blocks the tool couldn't size can be emitted with
a best-effort length.

Open question worth checking before building: whether consumers care that our
"blocks" are line/run leaders (coverage-shaped) rather than the CFG's exact basic
blocks — Lighthouse tolerates approximate block tables, but this should be
validated against a real IDA/Ghidra session.

---

## 3. Option B — true per-block hit counts

**Only fuzzers consume raw hit counts**, as a bitmap. drcov (RE viewers) is
boolean per block, not counts; Tenet wants traces, not counts.

The existing engines can't produce hit counts cheaply: the VEH engine
**retires** a probe after its first hit (fire-once → zero steady-state cost),
and the debugger removes the breakpoint after first hit. Both are *designed* to
stop counting after the first execution. Getting per-execution counts means
keeping the probe live and paying on every hit — which is exactly what the
compiler fast mode already does (saturating 8-bit counters, §3.4 of the perf
roadmap) **when you can recompile**.

So per-block hit counts split by whether recompilation is allowed:
- **Recompile allowed** → already solved: SanitizerCoverage inline-8bit-counters
  (the fast-mode runtimes) give saturating counts today; only an exporter to an
  AFL-style bitmap would be new.
- **Binary-only (no recompile)** → this is the only place a **TinyInst-style
  rewriter** (Option C) actually earns its keep.

---

## 4. Option C — TinyInst-style binary rewriting (biggest lift, narrow payoff)

Mark the target module non-executable, fault on first execution, JIT-rewrite each
basic block into a nearby code cache with an inline coverage-recording
instruction, and chain blocks through an original→translated address map; only
target modules are instrumented. Coverage becomes a **bitmap** (DATA, not
CONTROL), ~15–20% overhead, pure usermode, no recompile. Zydis (already vendored)
covers the disassembly; the hard part is correct instruction **relocation**
(RIP-relative operands, 32-bit-displacement branches that no longer reach,
indirect-branch resolution via the address map) and thread safety — this is why
the standing advice is to **wrap TinyInst** rather than write a rewriter.

**Why it is parked, not scheduled — the value analysis that reframed §D of the
perf roadmap:**

- For **boolean line/branch coverage** (what our reports show), the **VEH engine
  already reaches zero steady-state overhead** by retiring each probe after its
  first hit. A rewriter runs an inline counter increment on *every* execution
  forever — strictly *more* work. So as "faster binary-only line coverage" it is
  a regression, not a win. The §D entry predates the VEH engine's fire-once
  retirement, which quietly closed the gap it was written for.
- Its genuine differentiator is **per-block hit counts without recompiling**
  (§3, binary-only branch) — i.e. driving a **fuzzer** on a binary you can't
  rebuild. That is a real but distinct product (fuzzing feedback), not a coverage
  report.
- Even the RE-viewer use case (Lighthouse et al.) does **not** need it: drcov is
  boolean block coverage, which Option A already produces from existing data.

**Conclusion:** build C only if the deliberate goal is *binary-only fuzzer
feedback*. For everything renderable today, Option A (drcov exporter) delivers
the RE-tool value at a fraction of the cost.

---

## 5. Suggested order, if this is ever picked up

1. **drcov v2 exporter** (Option A) — small, rides on existing engine data,
   unlocks Lighthouse / Ghidra / Binary Ninja rendering. Validate against a real
   IDA/Ghidra session.
2. **AFL-style bitmap exporter for the fast-mode counters** (Option B, recompile
   branch) — turns the existing SanitizerCoverage counts into fuzzer feedback;
   still no rewriter.
3. **TinyInst wrap** (Option C) — only if binary-only fuzzer feedback becomes a
   goal; largest lift, wrap rather than write.

---

## Appendix — references

- drcov format: https://www.ayrx.me/drcov-file-format/
- Lighthouse (IDA): https://github.com/gaasedelen/lighthouse ·
  Tenet: https://github.com/gaasedelen/tenet
- Cartographer (Ghidra): https://github.com/nccgroup/Cartographer ·
  bncov (Binary Ninja): https://github.com/ForAllSecure/bncov
- TinyInst / LiteCov: https://github.com/googleprojectzero/TinyInst
- DynamoRIO drcov tool: https://dynamorio.org/page_drcov.html
- AFL++ coverage internals: https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/technical_details.md
