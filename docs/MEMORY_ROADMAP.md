# Memory roadmap

How to make the VEH engine degrade gracefully on memory-constrained machines
(small CI VMs are a primary deployment target), without hurting performance or
robustness on well-provisioned ones. It complements
[PERFORMANCE_ROADMAP.md](PERFORMANCE_ROADMAP.md), which optimizes for time;
this document optimizes for *commit* — on Windows the resource that actually
kills runs.

## Ground rules: commit vs. swapping

The ctest-scale incident that motivated this document was **commit
exhaustion**, not slow paging: private heap and pagefile-backed sections charge
the system commit limit (RAM + pagefile cap) the moment they are allocated,
whether or not the pages are ever touched. Swapping does not relieve commit;
only *not charging it* does. The one kind of memory that is both evictable
under pressure and (nearly) commit-free is a **file-backed mapping**: clean
pages are silently discarded and re-read from the file, no pagefile involved.

Corollaries:

- Moving a data structure from heap to a *pagefile-backed* section changes
  nothing — same commit, worse locality. File-backed or bust.
- Anything transient should stay heap (cheap, fast, released promptly).
- Anything large, read-mostly and long-lived is a candidate for a mapped file.

## Already in place (baseline)

- raw_pdb parses PDBs through a file mapping — the dominant transient
  consumer is already clean/discardable.
- Per-target shared sections carry only the target's own exe + non-exe
  candidates, and are absorbed + unmapped the moment their process exits
  (`CoverageCollector`); host steady state on a 3500-target suite is
  ~140 MB (down from 17 GB before the 2026-07-23 fixes).
- Source-file paths are interned per module (`ModuleProbes::files_`);
  per-line/per-branch records carry 4-byte indices.
- Probe computation failures (including `bad_alloc`) cost one module, not
  the run.

## Tier 1 — mapped line-table cache (impact: high, effort: medium)

`PdbLineTableCache` already persists parsed line tables to disk in a columnar
format, but loads them back into heap vectors — private commit, and the
random-access pattern makes it thrash when it does swap. Consume the on-disk
format **in place** through `MapViewOfFile`:

- a `ModuleLineTable` view type over the mapped columns (the format is already
  columnar; offsets + counts replace vectors);
- zero commit for cached tables, transparent eviction/re-fetch under pressure,
  and cross-process sharing between concurrent runs for free;
- when RAM is plentiful the pages sit in the page cache — no slowdown.

Requires a cache-format version bump and read-path indirection; the write path
is unchanged.

## Tier 2 — persistent, mapped probe cache (impact: high, effort: high)

`ModuleProbes` (probe RVAs, line refs, branch probes, file tables) is compact
after interning but heap-resident for every module for the whole run. Serialize
it into the same disk cache, keyed by PDB identity (GUID + age) **plus a hash
of the probe-relevant settings** (line merging, branch coverage, filters), and
map it like Tier 1:

- in-run memory becomes evictable file-backed pages;
- later runs skip probe computation entirely — the full-function Zydis
  disassembly and branch discovery are the dominant cold-start cost after PDB
  parsing, so this is also a CI wall-clock win;
- the settings hash keeps a cache entry from ever being reused across
  incompatible configurations.

## Tier 3 — evict-and-retry on allocation failure (impact: medium, effort: low)

Today an allocation failure during probe computation skips the module (logged,
negatively cached). With Tiers 1–2 in place the cache holds the large state, so
add an LRU size budget and one retry: on `bad_alloc`, evict least-recently-used
cache entries and recompute once before giving up. Coverage then degrades
predictably (one module at a time, loudest-first in the log) instead of
depending on allocation order.

## Non-goals

- **Working-set trimming** (`SetProcessWorkingSetSize`, `EmptyWorkingSet`):
  cosmetic — reduces the working set, not commit, and the OS re-grows it on
  first touch.
- **Moving live hit/branch arrays out of the shared sections**: they must stay
  writable and shared with the target; they are pagefile-backed by necessity
  and, post-filtering, small.
- **Large pages**: pin memory harder, the opposite of graceful.

## Deployment note

The commit limit is RAM + pagefile cap: on small VMs a tightly capped or
disabled pagefile turns modest spikes into hard failures regardless of what
this tool does. Leave the pagefile system-managed on coverage runners.
