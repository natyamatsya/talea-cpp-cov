# Vendored gch::small_vector (Gene Harvey, MIT) - a single-header small_vector
# with a small-buffer optimization and heap spill, replacing
# boost::container::small_vector so the project carries no Boost dependency.
#
# Source: https://github.com/gharveymn/small_vector
#         source/include/gch/small_vector.hpp  (see LICENSE)
#
# Header-only: an INTERFACE target that only propagates the include directory,
# so gch/small_vector.hpp is reachable as <gch/small_vector.hpp>.
add_library(SmallVector INTERFACE)
add_library(occ::SmallVector ALIAS SmallVector)

target_include_directories(SmallVector
	INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
