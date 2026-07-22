add_executable(Benchmarks
	BranchDisassemblerBenchmark.cpp
	CoverageDataBenchmark.cpp
	main.cpp)

occ_apply_conventions(Benchmarks)
# shlwapi: required by the statically linked google-benchmark.
target_link_libraries(Benchmarks PRIVATE
	occ_boost benchmark::benchmark fmt::fmt
	occ::CppCoverage occ::Exporter occ::Plugin occ::Tools
	shlwapi)
