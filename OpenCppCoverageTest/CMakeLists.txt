add_executable(OpenCppCoverageTest
	CommandLineOptionsTest.cpp
	ImportExportTests.cpp
	OpenCppCoverageConsoleTest.cpp
	OpenCppCoverageTest.cpp
	OpenCppCoverageTestTools.cpp
	WarningManagerTests.cpp)

occ_apply_conventions(OpenCppCoverageTest)
target_link_libraries(OpenCppCoverageTest PRIVATE
	occ_gtest
	occ::CppCoverage occ::Exporter occ::Coverage
	occ::TestCoverageSharedLib occ::TestHelper occ::Tools)

# End-to-end: runs the real OpenCppCoverage.exe over the other test binaries.
add_dependencies(OpenCppCoverageTest CppCoverageTest ExporterTest OpenCppCoverage)

target_precompile_headers(OpenCppCoverageTest PRIVATE stdafx.h)

occ_register_tests(OpenCppCoverageTest SERIAL TIMEOUT 300)
