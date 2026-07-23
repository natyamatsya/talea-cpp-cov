add_executable(Benchmarks
	BranchDisassemblerBenchmark.cpp
	CoverageDataBenchmark.cpp
	main.cpp)

occ_apply_conventions(Benchmarks)
# shlwapi: required by the statically linked google-benchmark.
target_link_libraries(Benchmarks PRIVATE
	benchmark::benchmark fmt::fmt
	occ::CppCoverage occ::Exporter occ::Coverage occ::Tools
	shlwapi occ_mimalloc)
