# Fixture for the fast-mode (SanitizerCoverage) collection path.
#
# MSVC only: the cl fast-mode runtime uses inline-8bit-counters and an ordinary
# link. The clang-cl variant needs lld (link.exe cannot resolve clang's
# SanitizerCoverage section-boundary symbols), which this build does not use,
# so the fixture and its test are skipped under clang-cl.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	return()
endif()

add_executable(TestFastCovConsole
	TestFastCovConsole.cpp
	"${CMAKE_SOURCE_DIR}/coverage-runtime/talea-cpp-cov-fastcov-msvc.cpp")

# Instrument only the fixture; the runtime must be compiled WITHOUT coverage
# (it defines the anchor the instrumented module needs and writes the dump).
set_source_files_properties(TestFastCovConsole.cpp PROPERTIES
	COMPILE_OPTIONS "/fsanitize-coverage=inline-8bit-counters")

# A PDB is required so the importer can map counters back to source lines;
# runtime checks are incompatible with the instrumentation.
set_target_properties(TestFastCovConsole PROPERTIES
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase
	MSVC_RUNTIME_CHECKS "")

# Link with /DEBUG (so a PDB is emitted in every preset) and keep the linker's
# section trimming off. Without /DEBUG a Release link turns on /OPT:REF /OPT:ICF,
# which prunes the zero-initialized .sancov counter region down to almost
# nothing (its VirtualSize collapses from ~0x319 to ~0x8) and the fast-mode
# import comes back empty. /OPT:NOREF /OPT:NOICF keep the full counter array.
target_link_options(TestFastCovConsole PRIVATE /DEBUG /OPT:NOREF /OPT:NOICF)

# A DLL fixture and its host for the runtime's per-module dumps: the DLL is
# instrumented and links the runtime (for the _Sancov8bitUsed anchor); the host
# is not instrumented but links the runtime as well, forced in through the
# anchor, the way a test executable of an instrumented library does.
add_library(TestFastCovDll SHARED
	TestFastCovDll.cpp
	"${CMAKE_SOURCE_DIR}/coverage-runtime/talea-cpp-cov-fastcov-msvc.cpp")
set_source_files_properties(TestFastCovDll.cpp PROPERTIES
	COMPILE_OPTIONS "/fsanitize-coverage=inline-8bit-counters")
set_target_properties(TestFastCovDll PROPERTIES
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase
	MSVC_RUNTIME_CHECKS "")
target_link_options(TestFastCovDll PRIVATE /DEBUG /OPT:NOREF /OPT:NOICF)

add_executable(TestFastCovDllHost
	TestFastCovDllHost.cpp
	"${CMAKE_SOURCE_DIR}/coverage-runtime/talea-cpp-cov-fastcov-msvc.cpp")
set_target_properties(TestFastCovDllHost PROPERTIES
	MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase
	MSVC_RUNTIME_CHECKS "")
target_link_options(TestFastCovDllHost PRIVATE /DEBUG /INCLUDE:_Sancov8bitUsed)
# Loaded by name at run time, so it must sit next to the host.
add_dependencies(TestFastCovDllHost TestFastCovDll)
