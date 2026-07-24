add_executable(talea-cpp-cov
	talea-cpp-cov.cpp
	main.cpp)

talea_apply_conventions(talea-cpp-cov)
target_compile_definitions(talea-cpp-cov PRIVATE TALEA_CPP_COV_APP_EXPORTS)
target_link_libraries(talea-cpp-cov PRIVATE
	talea::TaleaCppCov talea::Exporter talea::Coverage talea::Tools talea_mimalloc)

target_precompile_headers(talea-cpp-cov PRIVATE stdafx.h)

# The injected VEH helper must ship alongside the tool.
add_dependencies(talea-cpp-cov VehCoverageHelper)
