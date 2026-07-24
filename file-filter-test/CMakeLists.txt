add_executable(FileFilterTest
	FileFilterTest.cpp
	LineFilterTest.cpp
	PathMatcherTest.cpp
	RelocationsExtractorTest.cpp
	Tools.cpp
	UnifiedDiffCoverageFilterTest.cpp
	UnifiedDiffParserTest.cpp)

talea_apply_conventions(FileFilterTest)
target_link_libraries(FileFilterTest PRIVATE
	talea_gtest
	talea::FileFilter talea::TestCoverageOptimizedBuild talea::TestHelper talea::Tools)

target_precompile_headers(FileFilterTest PRIVATE stdafx.h)
talea_register_tests(FileFilterTest)
