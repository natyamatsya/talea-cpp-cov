add_library(CppCoverage STATIC
	Address.cpp
	BranchCondition.cpp
	BranchCoverageManager.cpp
	BranchDisassembler.cpp
	BreakPoint.cpp
	CodeCoverageRunner.cpp
	CoverageDataMerger.cpp
	CoverageFilterManager.cpp
	CoverageFilterSettings.cpp
	CoverageRate.cpp
	CoverageRateComputer.cpp
	CppCoverageException.cpp
	DebugInformationEnumerator.cpp
	Debugger.cpp
	ExceptionHandler.cpp
	ExecutedAddressManager.cpp
	ExportOptionParser.cpp
	FastCoverageImporter.cpp
	FileSystem.cpp
	FilterAssistant.cpp
	HandleInformation.cpp
	IDebugEventsHandler.cpp
	ImportClosure.cpp
	LineRunScanner.cpp
	MonitoredLineRegister.cpp
	Options.cpp
	OptionsExport.cpp
	OptionsParser.cpp
	Patterns.cpp
	PdbLineTableCache.cpp
	PdbSignature.cpp
	PeImageFileReader.cpp
	Process.cpp
	ProgramOptions.cpp
	RawPdbReader.cpp
	RunCoverageSettings.cpp
	StartInfo.cpp
	UnifiedDiffCoverageFilterManager.cpp
	UnifiedDiffSettings.cpp
	VehCoverageEngine.cpp
	WildcardCoverageFilter.cpp
	Wildcards.cpp)
add_library(occ::CppCoverage ALIAS CppCoverage)

occ_apply_conventions(CppCoverage)
target_compile_definitions(CppCoverage PRIVATE CPPCOVERAGE_EXPORTS)
# glaze is PUBLIC: GlazeCli.hpp / OptionsSpec.hpp expose it to consumers.
target_link_libraries(CppCoverage
	PUBLIC occ::FileFilter occ::Coverage occ::Tools glaze::glaze
	PRIVATE occ::SmallVector occ::RawPdb occ::Zydis mio::mio DiaSDK::DiaSDK Dbghelp)

target_precompile_headers(CppCoverage PRIVATE stdafx.h)

# The VEH engine loads VehCoverageHelper.dll from CppCoverage.dll's directory.
add_dependencies(CppCoverage VehCoverageHelper)

# msdia140.dll is deployed side-by-side (not COM-registered); the vendored
# copy matches what the old Dia.props post-build step shipped.
add_custom_command(TARGET CppCoverage POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/Dia/x64/msdia140.dll"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
	VERBATIM)
