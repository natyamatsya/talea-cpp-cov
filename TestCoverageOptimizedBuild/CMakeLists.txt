# Coverage fixture DLL that is deliberately OPTIMIZED even in Debug (with
# PDBs, without runtime checks) to exercise coverage of optimized code and
# relocation handling; do not "fix" this.
add_library(TestCoverageOptimizedBuild SHARED
	TestCoverageOptimizedBuild.cpp)
add_library(occ::TestCoverageOptimizedBuild ALIAS TestCoverageOptimizedBuild)

occ_apply_conventions(TestCoverageOptimizedBuild NO_DEFAULT_OPTIMIZATION)
target_compile_definitions(TestCoverageOptimizedBuild PRIVATE
	TESTCOVERAGERELEASELIB_EXPORTS)
target_compile_options(TestCoverageOptimizedBuild PRIVATE /O2 /Ob2 /Oi /Gy
	$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/JMC>)
# No runtime checks even in Debug — /RTC is incompatible with optimization.
set_target_properties(TestCoverageOptimizedBuild PROPERTIES MSVC_RUNTIME_CHECKS "")
