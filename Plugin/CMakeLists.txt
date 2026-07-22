# Public plugin interface (IExportPlugin and the coverage data types).
add_library(Plugin SHARED
	Exporter/BranchCoverage.cpp
	Exporter/CoverageData.cpp
	Exporter/FileCoverage.cpp
	Exporter/LineCoverage.cpp
	Exporter/ModuleCoverage.cpp)
add_library(occ::Plugin ALIAS Plugin)

occ_apply_conventions(Plugin)
target_compile_definitions(Plugin PRIVATE PLUGIN_EXPORTS)

target_precompile_headers(Plugin PRIVATE stdafx.h)
