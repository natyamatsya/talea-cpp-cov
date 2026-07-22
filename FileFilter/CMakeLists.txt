add_library(FileFilter SHARED
	AmbiguousPathException.cpp
	File.cpp
	LineFilter.cpp
	PathMatcher.cpp
	ReleaseCoverageFilter.cpp
	RelocationsExtractor.cpp
	UnifiedDiffCoverageFilter.cpp
	UnifiedDiffParser.cpp
	UnifiedDiffParserException.cpp)
add_library(occ::FileFilter ALIAS FileFilter)

occ_apply_conventions(FileFilter)
target_compile_definitions(FileFilter PRIVATE FILEFILTER_EXPORTS)
target_link_libraries(FileFilter
	PUBLIC occ::Tools
	PRIVATE occ_boost)

target_precompile_headers(FileFilter PRIVATE stdafx.h)
