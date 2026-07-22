add_executable(FileFilterTest
	FileFilterTest.cpp
	LineFilterTest.cpp
	PathMatcherTest.cpp
	RelocationsExtractorTest.cpp
	Tools.cpp
	UnifiedDiffCoverageFilterTest.cpp
	UnifiedDiffParserTest.cpp)

occ_apply_conventions(FileFilterTest)
target_link_libraries(FileFilterTest PRIVATE
	occ_boost occ_gtest
	occ::FileFilter occ::TestCoverageOptimizedBuild occ::TestHelper occ::Tools)

target_precompile_headers(FileFilterTest PRIVATE stdafx.h)
occ_register_tests(FileFilterTest)
