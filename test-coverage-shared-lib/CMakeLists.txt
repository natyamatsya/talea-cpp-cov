# Coverage fixture DLL. Optimization is deliberately disabled in Release so
# line coverage stays deterministic; do not "fix" this.
add_library(TestCoverageSharedLib SHARED
	TestCoverageSharedLib.cpp)
add_library(talea::TestCoverageSharedLib ALIAS TestCoverageSharedLib)

talea_apply_conventions(TestCoverageSharedLib NO_DEFAULT_OPTIMIZATION)
target_compile_options(TestCoverageSharedLib PRIVATE
	$<$<CONFIG:Debug>:/Od /Ob0>
	$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/JMC>
	$<$<CONFIG:Release>:/Od>)
target_precompile_headers(TestCoverageSharedLib PRIVATE stdafx.h)
