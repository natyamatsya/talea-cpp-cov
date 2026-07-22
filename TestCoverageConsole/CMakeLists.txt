# The console fixture that coverage tests run under the debugger.
# Optimization is disabled in BOTH configs, and CodeCoverageRunnerTest
# asserts exact line numbers in TestCoverageConsole.cpp — never reformat it.
# Source order is link order and must keep TestBasic.cpp first: the linker
# takes COMDAT stubs like __empty_global_delete (attributed to "last line + 1"
# of a TU) from the first object, and CodeCoverageRunnerTest.SpecialLineInfo
# asserts that EVERY line recorded for SpecialLineInfo.cpp was executed.
add_executable(TestCoverageConsole
	TestBasic.cpp
	TestBranch.cpp
	TestCoverageConsole.cpp
	SpecialLineInfo.cpp
	TestDebugInformationEnumerator.cpp
	TestDiff.cpp
	TestThread.cpp)

occ_apply_conventions(TestCoverageConsole NO_DEFAULT_OPTIMIZATION)
target_compile_options(TestCoverageConsole PRIVATE
	$<$<CONFIG:Debug>:/Od /Ob0>
	$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/JMC>
	$<$<CONFIG:Release>:/Od>
	# The fixture code deliberately contains a null dereference and a
	# #pragma once in a .cpp; cl at /W3 never warned on these, so silence
	# clang's equivalents rather than touch sources whose line numbers are
	# asserted by tests. (-Wno-unused-variable lives in occ_defaults; listing
	# it here too would win CMake's option de-duplication and then be
	# re-enabled by /W3's -Wall.)
	$<$<CXX_COMPILER_ID:Clang>:-Wno-null-dereference;-Wno-pragma-once-outside-header>)
target_link_libraries(TestCoverageConsole PRIVATE
	occ::TestCoverageOptimizedBuild occ::TestCoverageSharedLib occ::Tools)
