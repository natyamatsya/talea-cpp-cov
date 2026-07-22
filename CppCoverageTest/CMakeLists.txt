add_executable(CppCoverageTest
	BranchConditionTest.cpp
	BranchDisassemblerTest.cpp
	BreakPointTest.cpp
	CodeCoverageRunnerTest.cpp
	CoverageDataMergerRandomTest.cpp
	CoverageDataMergerTest.cpp
	CoverageRateComputerTest.cpp
	CoverageRateTest.cpp
	CppCoverageExceptionTest.cpp
	CppCoverageTest.cpp
	DebugInformationEnumeratorTest.cpp
	DebuggerTest.cpp
	ExceptionHandlerTest.cpp
	ExecutedAddressManagerTest.cpp
	FastCoverageImporterTest.cpp
	FilterAssistantTest.cpp
	GlazeCliTest.cpp
	HandleInformationTest.cpp
	ImportClosureTest.cpp
	LineRunScannerTest.cpp
	OptionsParserConfigTest.cpp
	OptionsParserExportTest.cpp
	OptionsParserPatternTest.cpp
	OptionsParserTest.cpp
	OptionsParserUnifiedDiffTest.cpp
	ProcessTest.cpp
	RawPdbReaderTest.cpp
	StartInfoTest.cpp
	TestTools.cpp
	UnifiedDiffCoverageFilterManagerTest.cpp
	VehCoverageEngineTest.cpp
	WildcardCoverageFilterTest.cpp
	WildcardsTest.cpp)

occ_apply_conventions(CppCoverageTest)
target_link_libraries(CppCoverageTest PRIVATE
	occ_gtest
	occ::CppCoverage occ::FileFilter occ::Plugin
	occ::TestCoverageOptimizedBuild occ::TestCoverageSharedLib
	occ::TestHelper occ::Tools)

# Fixtures this suite runs under its debugger at test time.
add_dependencies(CppCoverageTest TestCoverageConsole VehCoverageHelper)
# The fast-mode fixture exists only under MSVC (see TestFastCovConsole).
if(TARGET TestFastCovConsole)
	add_dependencies(CppCoverageTest TestFastCovConsole)
endif()

target_precompile_headers(CppCoverageTest PRIVATE stdafx.h)
# End-to-end suite: spawns debuggers and child processes. Each case runs in
# well under a second; the generous timeout is a hang backstop, not a target.
occ_register_tests(CppCoverageTest SERIAL TIMEOUT 300)
