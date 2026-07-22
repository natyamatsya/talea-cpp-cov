# Interface targets and helpers replacing the old MSBuild property sheets
# (PropertySheets/*.props).

# --- Default.props ----------------------------------------------------------
add_library(occ_defaults INTERFACE)
target_compile_definitions(occ_defaults INTERFACE
	OPENCPPCOVERAGE_VERSION="${PROJECT_VERSION}"
	_CRT_SECURE_NO_WARNINGS
	NOMINMAX
	UNICODE _UNICODE
	"CTEMPLATE_DLL_DECL="
	BENCHMARK_STATIC_DEFINE)
target_compile_options(occ_defaults INTERFACE
	/W3 /FC /utf-8
	# Fully static build: C4251/C4275 (a dll-exported class has an STL/base
	# without a dll-interface) cannot occur across a real boundary, so silence
	# them globally instead of per-export-header as the DLL build once did.
	$<$<CXX_COMPILER_ID:MSVC>:/sdl;/wd4251;/wd4275>
	$<$<COMPILE_LANGUAGE:CXX>:/permissive- /std:c++latest>
	$<$<BOOL:${OCC_WERROR}>:/WX>
	# glaze exports /Zc:lambda and /Zc:preprocessor (cl-only, inert on clang);
	# clang-cl would otherwise fail them as unused arguments under /WX.
	# -Wno-unused-variable: clang-cl maps /W3 to -Wall, which includes
	# initialized-but-unused variables; cl only reports those at /W4 (C4189),
	# so silencing keeps warning parity with the MSVC build. Must stay in this
	# list AFTER /W3 — a target's own options precede these usage requirements
	# on the command line, so a per-target -Wno would be re-enabled by -Wall.
	$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-command-line-argument;-Wno-unused-variable>)
# Full PDBs and COMDAT folding in Release, as the old build produced.
target_link_options(occ_defaults INTERFACE
	$<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO>)

# --- Default optimization levels (opt-in; see the root CMakeLists.txt) ------
add_library(occ_opt_default INTERFACE)
target_compile_options(occ_opt_default INTERFACE
	$<$<CONFIG:Debug>:/Od /Ob0>
	# MSBuild Debug builds pass /JMC by default; the SpecialLineInfo coverage
	# test depends on the line records it produces.
	$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/JMC>
	$<$<CONFIG:Release>:/O2 /Ob2 /Oi /Gy>
	$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>,$<BOOL:${OCC_MSVC_LTO}>>:/GL>)
target_link_options(occ_opt_default INTERFACE
	$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>,$<BOOL:${OCC_MSVC_LTO}>>:/LTCG>)

# --- GTest.props -------------------------------------------------------------
# GTest is linked statically (fully static build), so GTEST_LINKED_AS_SHARED_-
# LIBRARY must NOT be set - otherwise the headers mark symbols dllimport and the
# link fails against the static gmock.lib.
add_library(occ_gtest INTERFACE)
target_link_libraries(occ_gtest INTERFACE GTest::gmock)
target_compile_definitions(occ_gtest INTERFACE
	_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)

# Applies the conventions every former Default.props importer relied on:
# repo-root + project include dirs, the compiled-in path macros consumed by
# the sources, and the default compile options. Targets with bespoke
# optimization levels pass NO_DEFAULT_OPTIMIZATION and set their own.
function(occ_apply_conventions target)
	cmake_parse_arguments(ARG "NO_DEFAULT_OPTIMIZATION" "" "" ${ARGN})
	target_include_directories(${target} PRIVATE
		"${CMAKE_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}")
	# Backslash form is load-bearing: coverage module/source matching compares
	# these strings against native paths reported by the debugger.
	string(REPLACE "/" "\\" _project_dir "${CMAKE_CURRENT_SOURCE_DIR}/")
	string(REPLACE "/" "\\" _solution_dir "${CMAKE_SOURCE_DIR}/")
	string(REPLACE "/" "\\" _out_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
	target_compile_definitions(${target} PRIVATE
		"PROJECT_DIR=R\"|(${_project_dir})|\""
		"SOLUTION_DIR=R\"|(${_solution_dir})|\""
		"OUT_DIR=R\"|(${_out_dir})|\""
		"TARGET_FILE_NAME=R\"|($<TARGET_FILE_NAME:${target}>)|\"")
	# occ_sanitizers carries the sanitizer flags (empty unless OCC_SANITIZER is
	# set); attaching it here scopes sanitizers to first-party code and leaves
	# the injected VehCoverageHelper and vendored RawPdb/Zydis untouched.
	target_link_libraries(${target} PRIVATE occ_defaults occ_sanitizers)
	if(NOT ARG_NO_DEFAULT_OPTIMIZATION)
		target_link_libraries(${target} PRIVATE occ_opt_default)
	endif()
endfunction()

# Registers a gtest executable with CTest. The end-to-end suites that spawn
# debuggers and write fixed-path reports pass SERIAL.
function(occ_register_tests target)
	cmake_parse_arguments(ARG "SERIAL" "TIMEOUT" "" ${ARGN})
	if(NOT BUILD_TESTING)
		return()
	endif()
	if(NOT ARG_TIMEOUT)
		set(ARG_TIMEOUT 300)
	endif()
	set(props TIMEOUT ${ARG_TIMEOUT})
	if(ARG_SERIAL)
		list(APPEND props RUN_SERIAL TRUE)
	endif()
	gtest_discover_tests(${target}
		WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
		DISCOVERY_MODE PRE_TEST
		DISCOVERY_TIMEOUT 120
		PROPERTIES ${props})
endfunction()
