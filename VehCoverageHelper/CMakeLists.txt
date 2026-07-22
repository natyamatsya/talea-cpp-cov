# Helper DLL injected into profiled targets for --experimental_veh.
# Self-contained on purpose: static CRT so it runs without VC runtime DLLs,
# no dependency beyond kernel32, and none of the project-wide conventions.
add_library(VehCoverageHelper SHARED VehCoverageHelper.cpp)
add_library(occ::VehCoverageHelper ALIAS VehCoverageHelper)

set_target_properties(VehCoverageHelper PROPERTIES
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_definitions(VehCoverageHelper PRIVATE NOMINMAX UNICODE _UNICODE)
target_compile_options(VehCoverageHelper PRIVATE
	/W4 /utf-8 /permissive- /std:c++latest
	$<$<CXX_COMPILER_ID:MSVC>:/sdl>
	$<$<BOOL:${OCC_WERROR}>:/WX>
	$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-command-line-argument;-Wno-unused-function>)
target_link_libraries(VehCoverageHelper PRIVATE occ_opt_default kernel32)
