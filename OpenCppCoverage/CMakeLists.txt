add_executable(OpenCppCoverage
	OpenCppCoverage.cpp
	main.cpp)

occ_apply_conventions(OpenCppCoverage)
target_compile_definitions(OpenCppCoverage PRIVATE OPEN_CPP_COVERAGE_EXPORTS)
target_link_libraries(OpenCppCoverage PRIVATE
	occ::CppCoverage occ::Exporter occ::Plugin occ::Tools occ_mimalloc)

target_precompile_headers(OpenCppCoverage PRIVATE stdafx.h)

# The injected VEH helper must ship alongside the tool.
add_dependencies(OpenCppCoverage VehCoverageHelper)
