# The shared coverage data types (module / file / line / branch coverage).
add_library(Coverage STATIC
	BranchCoverage.cpp
	CoverageData.cpp
	FileCoverage.cpp
	LineCoverage.cpp
	ModuleCoverage.cpp)
add_library(occ::Coverage ALIAS Coverage)

occ_apply_conventions(Coverage)
target_compile_definitions(Coverage PRIVATE COVERAGE_EXPORTS)

target_precompile_headers(Coverage PRIVATE stdafx.h)
