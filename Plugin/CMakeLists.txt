# The shared coverage data types (module / file / line / branch coverage).
add_library(Plugin STATIC
	Exporter/BranchCoverage.cpp
	Exporter/CoverageData.cpp
	Exporter/FileCoverage.cpp
	Exporter/LineCoverage.cpp
	Exporter/ModuleCoverage.cpp)
add_library(occ::Plugin ALIAS Plugin)

occ_apply_conventions(Plugin)
target_compile_definitions(Plugin PRIVATE PLUGIN_EXPORTS)

target_precompile_headers(Plugin PRIVATE stdafx.h)
