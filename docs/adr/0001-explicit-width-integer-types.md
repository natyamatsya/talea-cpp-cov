# 1. Use explicit-width integer types in first-party code

Date: 2026-07-22

## Status

Accepted.

## Context

The codebase mixes implicit-width integer types (`int`, `unsigned`,
`unsigned long`, `unsigned char`) with fixed-width types from `<cstdint>`.
Implicit-width types are a portability and readability hazard:

- Their size is model-dependent. On the toolchains this project targets
  (MSVC / clang-cl, LLP64) `int` and `long` are both 32-bit, but that is not
  guaranteed elsewhere, and `long` is 64-bit on LP64 systems. A field declared
  `unsigned long` says nothing about the range it is meant to hold.
- They obscure intent. A line number, a branch index, a counter and a byte are
  all "some unsigned integer"; the width and role are left implicit.
- They leak into serialized formats and container layouts (e.g. the columnar
  `.cov` format and the coverage data model) where the exact width is part of
  the contract.

The alternative — fixed-width types (`std::uint32_t`, `std::int32_t`,
`std::uint8_t`, `std::uint64_t`, …) — states the width and, by extension, the
intended range at the declaration.

Windows and its debug APIs (Win32, DIA, the CRT) are built on their own integer
types: `int`, `DWORD` / `ULONG` (`unsigned long`), `BYTE` (`unsigned char`),
`DWORD64`, and so on. Adopting fixed-width types for our own code therefore
creates a type mismatch at every call into those APIs.

## Decision

First-party code uses explicit fixed-width integer types from `<cstdint>` for
every integer value that represents data with a defined numeric role or width:

- 32-bit values (line numbers, branch indices, counts, container keys, format
  fields) use `std::uint32_t` / `std::int32_t`.
- Bytes use `std::uint8_t`.
- 64-bit values (addresses, RVAs, file offsets) use `std::uint64_t` /
  `std::int64_t`.
- `std::size_t` remains the type for sizes of and indices into standard
  containers (it is already explicit about its role).

Platform and API integer types (`DWORD`, `ULONG`, `BYTE`, `HANDLE`, `int` as a
Win32 parameter, …) are **not** first-party types. They appear only at the
interop boundary — the immediate call site of the API — and are not propagated
into first-party data structures or signatures.

This will break interop where an API expects its own type: passing a
`std::uint32_t*` to a parameter typed `DWORD*` (`unsigned long*`) does not
compile, because the pointee types differ even at the same width. That breakage
is expected and accepted. It is fixed by a clean, explicit conversion at the
boundary, not by weakening the first-party type back to the implicit one.

## Consequences

**Positive**

- Declarations state their width and intended range; no LLP64/LP64 ambiguity.
- The coverage data model and on-disk formats have widths that match their
  contract by construction.
- Reviews can reason about overflow and truncation locally.

**Cost, and how it is paid**

- Interop with Win32 / DIA / CRT requires a conversion at the boundary. The
  discipline is: keep the API's own type in a local variable at the call site,
  then convert to (or from) the first-party fixed-width type with an explicit
  `static_cast`. Do not spread the API type inward.

  ```cpp
  // Reading a DIA out-parameter (DWORD == unsigned long):
  DWORD apiLine = 0;
  if (lineNumber.get_lineNumber(&apiLine) != S_OK)
      THROW("DIA: cannot get line number");
  std::uint32_t line = static_cast<std::uint32_t>(apiLine);
  ```

- The CRT character-classification idiom `static_cast<unsigned char>(c)` before
  `std::isspace` / `std::tolower` is itself a boundary cast against the CRT's
  contract and stays as written.

- Because `std::uint32_t` is a typedef for `unsigned int` (and `std::uint8_t`
  for `unsigned char`) on the supported toolchains, migrating existing
  same-width code is source-level only — no binary or behavioural change — so
  it can be done incrementally under the test matrix.

**Scope note**

- Plain, short-lived loop indices with no stored or interface role may stay
  `int` / `std::size_t` where that reads more naturally; the rule targets values
  that carry meaning (are stored, serialized, compared across a boundary, or
  exposed in a signature), not every arithmetic temporary.

## History

- The first step under this decision was the sweep replacing first-party
  `unsigned int` with `std::uint32_t` (line numbers, branch indices, counts and
  the `Plugin::FileCoverage` `std::flat_map` key), which was source-only and
  kept all four build configurations green.
