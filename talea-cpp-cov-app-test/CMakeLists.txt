add_executable(talea-cpp-cov-test
	CommandLineOptionsTest.cpp
	ImportExportTests.cpp
	talea-cpp-cov-console-test.cpp
	talea-cpp-cov-test.cpp
	talea-cpp-cov-test-tools.cpp
	WarningManagerTests.cpp)

talea_apply_conventions(talea-cpp-cov-test)
target_link_libraries(talea-cpp-cov-test PRIVATE
	talea_gtest
	talea::TaleaCppCov talea::Exporter talea::Coverage
	talea::TestCoverageSharedLib talea::TestHelper talea::Tools)

# End-to-end: runs the real talea-cpp-cov.exe over the other test binaries.
add_dependencies(talea-cpp-cov-test TaleaCppCovTest ExporterTest talea-cpp-cov)

target_precompile_headers(talea-cpp-cov-test PRIVATE stdafx.h)

talea_register_tests(talea-cpp-cov-test SERIAL TIMEOUT 300)
