add_executable(OpenCppCoverageTest
	CommandLineOptionsTest.cpp
	ImportExportTests.cpp
	OpenCppCoverageConsoleTest.cpp
	OpenCppCoverageTest.cpp
	OpenCppCoverageTestTools.cpp
	WarningManagerTests.cpp)

occ_apply_conventions(OpenCppCoverageTest)
target_link_libraries(OpenCppCoverageTest PRIVATE
	occ_boost occ_gtest
	occ::CppCoverage occ::Exporter occ::Plugin
	occ::TestCoverageSharedLib occ::TestHelper occ::Tools)

# End-to-end: runs the real OpenCppCoverage.exe over the other test binaries.
add_dependencies(OpenCppCoverageTest CppCoverageTest ExporterTest OpenCppCoverage)

target_precompile_headers(OpenCppCoverageTest PRIVATE stdafx.h)

# The import/export tests load TestCoverageSharedLib.dll as a stand-in plugin
# from <exe dir>/Plugins/Exporter.
add_custom_command(TARGET OpenCppCoverageTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Plugins/Exporter"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:TestCoverageSharedLib>"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Plugins/Exporter"
	VERBATIM)

occ_register_tests(OpenCppCoverageTest SERIAL TIMEOUT 300)
