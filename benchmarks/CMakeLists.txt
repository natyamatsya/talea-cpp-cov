add_executable(Benchmarks
	BranchDisassemblerBenchmark.cpp
	CoverageDataBenchmark.cpp
	main.cpp)

talea_apply_conventions(Benchmarks)
# shlwapi: required by the statically linked google-benchmark.
target_link_libraries(Benchmarks PRIVATE
	benchmark::benchmark fmt::fmt
	talea::TaleaCppCov talea::Exporter talea::Coverage talea::Tools
	shlwapi talea_mimalloc)
