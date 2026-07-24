add_library(TaleaCppCov STATIC
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
	TaleaCppCovException.cpp
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
add_library(talea::TaleaCppCov ALIAS TaleaCppCov)

talea_apply_conventions(TaleaCppCov)
target_compile_definitions(TaleaCppCov PRIVATE TALEACPPCOV_EXPORTS)
# glaze is PUBLIC: GlazeCli.hpp / OptionsSpec.hpp expose it to consumers.
target_link_libraries(TaleaCppCov
	PUBLIC talea::FileFilter talea::Coverage talea::Tools glaze::glaze
	PRIVATE talea::SmallVector talea::RawPdb talea::Zydis mio::mio DiaSDK::DiaSDK Dbghelp)

target_precompile_headers(TaleaCppCov PRIVATE stdafx.h)

# The VEH engine loads VehCoverageHelper.dll from TaleaCppCov.dll's directory.
add_dependencies(TaleaCppCov VehCoverageHelper)

# msdia140.dll is deployed side-by-side (not COM-registered). The vendored copy
# is msdia140.dll 14.51 from the Visual Studio 2026 DIA SDK, matching the build
# toolset; refresh it from "<VS>/DIA SDK/bin/amd64/msdia140.dll" on a toolset bump.
add_custom_command(TARGET TaleaCppCov POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/dia/x64/msdia140.dll"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
	VERBATIM)
