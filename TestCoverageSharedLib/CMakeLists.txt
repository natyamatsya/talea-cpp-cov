# Coverage fixture DLL. Optimization is deliberately disabled in Release so
# line coverage stays deterministic; do not "fix" this.
add_library(TestCoverageSharedLib SHARED
	SimpleTextExport.cpp
	TestCoverageSharedLib.cpp)
add_library(occ::TestCoverageSharedLib ALIAS TestCoverageSharedLib)

occ_apply_conventions(TestCoverageSharedLib NO_DEFAULT_OPTIMIZATION)
target_compile_options(TestCoverageSharedLib PRIVATE
	$<$<CONFIG:Debug>:/Od /Ob0>
	$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/JMC>
	$<$<CONFIG:Release>:/Od>)
target_link_libraries(TestCoverageSharedLib
	PUBLIC occ::Plugin)

target_precompile_headers(TestCoverageSharedLib PRIVATE stdafx.h)
