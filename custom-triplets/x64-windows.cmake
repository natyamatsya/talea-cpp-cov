# Overlay of the built-in x64-windows triplet: fully static.
#
# OpenCppCoverage.exe is a single, self-contained static executable - all of
# our own libraries are static, the CRT is static (/MT), and every vcpkg
# dependency is linked statically with the static CRT too. This removes the
# Visual C++ redistributable requirement and ships the tool as one file.
#
# The CRT linkage must match our targets (CMAKE_MSVC_RUNTIME_LIBRARY =
# MultiThreaded[Debug]): mixing a static-CRT exe with dynamic-CRT dependency
# DLLs would give each module its own heap, which corrupts on any allocation
# that crosses the boundary.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
