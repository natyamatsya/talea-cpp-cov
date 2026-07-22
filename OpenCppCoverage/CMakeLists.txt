add_executable(OpenCppCoverage
	OpenCppCoverage.cpp
	main.cpp)

occ_apply_conventions(OpenCppCoverage)
target_compile_definitions(OpenCppCoverage PRIVATE OPEN_CPP_COVERAGE_EXPORTS)
target_link_libraries(OpenCppCoverage PRIVATE
	occ_boost occ::CppCoverage occ::Exporter occ::Plugin occ::Tools)

target_precompile_headers(OpenCppCoverage PRIVATE stdafx.h)

# The injected VEH helper must ship alongside the tool.
add_dependencies(OpenCppCoverage VehCoverageHelper)

# The exe scans <exe dir>/Plugins/Exporter for export plugins at startup.
add_custom_command(TARGET OpenCppCoverage POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Plugins/Exporter"
	VERBATIM)
