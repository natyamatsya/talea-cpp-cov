add_library(FileFilter STATIC
	File.cpp
	LineFilter.cpp
	PathMatcher.cpp
	ReleaseCoverageFilter.cpp
	RelocationsExtractor.cpp
	UnifiedDiffCoverageFilter.cpp
	UnifiedDiffParser.cpp)
add_library(talea::FileFilter ALIAS FileFilter)

talea_apply_conventions(FileFilter)
target_compile_definitions(FileFilter PRIVATE FILEFILTER_EXPORTS)
target_link_libraries(FileFilter
	PUBLIC talea::Tools)

target_precompile_headers(FileFilter PRIVATE stdafx.h)
