// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2013 OpenCppCoverage
// SPDX-FileCopyrightText: 2026 natyamatsya and talea-cpp-cov contributors

// When built with -DOCC_MIMALLOC=ON, override global new/delete with mimalloc so
// the benchmarks measure allocation under mimalloc; a no-op when off.
#ifdef TALEA_USE_MIMALLOC
#include <mimalloc-new-delete.h>
#endif

#include <benchmark/benchmark.h>

BENCHMARK_MAIN();
