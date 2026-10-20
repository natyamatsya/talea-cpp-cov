add_executable(TaleaCppCovTest
	BranchConditionTest.cpp
	BranchDisassemblerTest.cpp
	BreakPointTest.cpp
	CodeCoverageRunnerTest.cpp
	CoverageDataMergerRandomTest.cpp
	CoverageDataMergerTest.cpp
	CoverageRateComputerTest.cpp
	CoverageRateTest.cpp
	TaleaCppCovExceptionTest.cpp
	TaleaCppCovTest.cpp
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
	WildcardsTest.cpp
	# The fast-mode dump tests write and import beyond MAX_PATH.
	"${CMAKE_SOURCE_DIR}/cmake/long-path-aware.manifest")

talea_apply_conventions(TaleaCppCovTest)
target_link_libraries(TaleaCppCovTest PRIVATE
	talea_gtest
	talea::TaleaCppCov talea::FileFilter talea::Coverage
	talea::TestCoverageOptimizedBuild talea::TestCoverageSharedLib
	talea::TestHelper talea::Tools)

# Fixtures this suite runs under its debugger at test time.
add_dependencies(TaleaCppCovTest TestCoverageConsole VehCoverageHelper)
# The fast-mode fixture exists only under MSVC (see TestFastCovConsole).
if(TARGET TestFastCovConsole)
	add_dependencies(TaleaCppCovTest TestFastCovConsole TestFastCovDllHost)
endif()

target_precompile_headers(TaleaCppCovTest PRIVATE stdafx.h)
# End-to-end suite: spawns debuggers and child processes. Each case runs in
# well under a second; the generous timeout is a hang backstop, not a target.
talea_register_tests(TaleaCppCovTest SERIAL TIMEOUT 300)
