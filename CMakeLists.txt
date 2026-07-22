cmake_minimum_required(VERSION 4.4.0)

project(OpenCppCoverage VERSION 0.9.9.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR
		"OpenCppCoverage requires an MSVC-compatible x64 toolchain (cl or clang-cl). "
		"Configure with one of the presets from a Visual Studio x64 Native Tools prompt.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(OCC_WERROR "Treat compiler warnings as errors (/WX)" ON)
option(OCC_MSVC_LTO "Whole-program optimization (/GL, /LTCG) in Release builds with cl" ON)
option(OCC_MIMALLOC "Use mimalloc as the global allocator (overrides operator new/delete)" OFF)
option(OCC_PROFILE "Profiling build: Release codegen but /OPT:NOICF for clean flame-graph symbols" OFF)
set(OCC_SANITIZER "none" CACHE STRING
	"Sanitizer to build first-party code with: none, address, undefined, address+undefined")
set_property(CACHE OCC_SANITIZER PROPERTY STRINGS none address undefined address+undefined)

# All runtime artifacts (exes, DLLs, PDBs, templates) share one flat directory:
# test code locates sibling binaries through the OUT_DIR macro, and the exe
# expects Template/ next to itself.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(OCC_SANITIZER STREQUAL "none")
	# Debug builds use edit-and-continue (/ZI) with cl exactly as MSBuild did —
	# the SpecialLineInfo coverage test depends on the line records it emits.
	# clang-cl and Release builds use /Zi; every configuration produces PDBs.
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
		"$<IF:$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>,EditAndContinue,ProgramDatabase>")
else()
	# ASan is incompatible with /ZI, /RTC1 and incremental linking; drop them.
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
	set(CMAKE_MSVC_RUNTIME_CHECKS "")
	add_link_options(/INCREMENTAL:NO)
endif()

# No C++20 modules anywhere in this codebase; scanning would only slow the
# build (and drags in clang-scan-deps for clang-cl).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The optimization level is a per-target decision in this project (some test
# fixtures compile optimized in Debug or unoptimized in Release, and mixing
# /RTC1 with /O2 is a hard compiler error), so the per-config defaults are
# cleared here and reinstated per target through occ_opt_default. Runtime
# checks (/RTC1 in Debug) stay with CMake's MSVC_RUNTIME_CHECKS abstraction;
# targets that compile optimized in Debug set that property to empty.
foreach(lang C CXX)
	set(CMAKE_${lang}_FLAGS_DEBUG "")
	set(CMAKE_${lang}_FLAGS_RELEASE "/DNDEBUG")
endforeach()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# clang-cl does not implement /RTC; suppress the abstraction's default.
	set(CMAKE_MSVC_RUNTIME_CHECKS "")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)
find_package(pcre2 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(ctemplate REQUIRED)
find_package(DiaSDK REQUIRED)

# Optional global allocator. When OCC_MIMALLOC is on, occ_mimalloc links
# mimalloc statically and defines OCC_USE_MIMALLOC so an executable's entry-point
# translation unit overrides operator new/delete via <mimalloc-new-delete.h>.
# Linked into OpenCppCoverage.exe and Benchmarks.exe; a no-op interface when off.
add_library(occ_mimalloc INTERFACE)
if(OCC_MIMALLOC)
	find_package(mimalloc CONFIG REQUIRED)
	target_link_libraries(occ_mimalloc INTERFACE mimalloc-static)
	target_compile_definitions(occ_mimalloc INTERFACE OCC_USE_MIMALLOC=1)
endif()

include(Sanitizers)
include(OccOptions)
include(CTest)
include(GoogleTest)

# Vendored and leaf libraries
add_subdirectory(RawPdb)
add_subdirectory(Zydis)
add_subdirectory(SmallVector)
add_subdirectory(Tools)
add_subdirectory(Plugin)
add_subdirectory(VehCoverageHelper)

# Core libraries and the tool
add_subdirectory(FileFilter)
add_subdirectory(CppCoverage)
add_subdirectory(Exporter)
add_subdirectory(OpenCppCoverage)

# Test fixtures
add_subdirectory(TestHelper)
add_subdirectory(TestCoverageSharedLib)
add_subdirectory(TestCoverageOptimizedBuild)
add_subdirectory(TestCoverageConsole)
add_subdirectory(TestFastCovConsole)

# Tests and benchmarks
add_subdirectory(ToolsTest)
add_subdirectory(PluginTest)
add_subdirectory(FileFilterTest)
add_subdirectory(CppCoverageTest)
add_subdirectory(ExporterTest)
add_subdirectory(OpenCppCoverageTest)
add_subdirectory(Benchmarks)
