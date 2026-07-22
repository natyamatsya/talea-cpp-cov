# Opt-in sanitizer support, wired into first-party code through the
# occ_sanitizers interface target (see OccOptions.cmake).
#
# Windows reality check:
#   * AddressSanitizer works with both cl and clang-cl (/fsanitize=address).
#   * UndefinedBehaviorSanitizer is clang-cl only.
#   * ThreadSanitizer has no Windows support in either toolchain, so it is
#     deliberately not offered here.
#
# ASAN cannot coexist with the runtime checks (/RTC1), edit-and-continue debug
# info (/ZI) or incremental linking this project uses by default, so those are
# forced off for the whole build when a sanitizer is active (done in the root
# CMakeLists, which includes this module).

add_library(occ_sanitizers INTERFACE)

if(NOT OCC_SANITIZER STREQUAL "none")
	set(_occ_san_flags "")

	if(OCC_SANITIZER MATCHES "address")
		list(APPEND _occ_san_flags /fsanitize=address)
		# The vcpkg leaf libs (fmt, spdlog, pcre2) are static and NOT built
		# with ASan, so their std::string/vector/optional carry no container
		# annotations. Ours would, and mixing the two across the link boundary
		# is an LNK2038 ABI mismatch. _DISABLE_STL_ANNOTATION turns off all of
		# them (string, vector, optional) to match; core ASan (heap/stack/
		# global overflow, use-after-free) is unaffected.
		target_compile_definitions(occ_sanitizers INTERFACE _DISABLE_STL_ANNOTATION)
	endif()

	if(OCC_SANITIZER MATCHES "undefined")
		if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			message(FATAL_ERROR
				"OCC_SANITIZER='${OCC_SANITIZER}' requests UndefinedBehaviorSanitizer, "
				"which cl.exe does not implement. Use a clang-cl preset.")
		endif()
		list(APPEND _occ_san_flags -fsanitize=undefined -fno-sanitize-recover=all)
	endif()

	if(NOT _occ_san_flags)
		message(FATAL_ERROR "Unknown OCC_SANITIZER value '${OCC_SANITIZER}'.")
	endif()

	target_compile_options(occ_sanitizers INTERFACE ${_occ_san_flags})
	target_link_options(occ_sanitizers INTERFACE ${_occ_san_flags})
	message(STATUS "Sanitizer build: ${OCC_SANITIZER} (${_occ_san_flags})")
endif()
