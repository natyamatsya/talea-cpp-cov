# Toolchain: clang-cl (MSVC-compatible Clang driver) targeting x64.
# Chainloaded by the vcpkg toolchain (VCPKG_CHAINLOAD_TOOLCHAIN_FILE); expects
# to run inside a Visual Studio x64 Native Tools environment (vcvars64) so the
# MSVC headers, libraries and Windows SDK are on the compiler's search paths.
# vcpkg dependencies still build with cl (governed by the triplet), which is
# ABI-compatible with clang-cl output.

# Prefer the clang-cl bundled with the active Visual Studio over whatever
# happens to be first on PATH (Swift/LLVM toolchains also ship a clang-cl).
if(DEFINED ENV{VCINSTALLDIR} AND EXISTS "$ENV{VCINSTALLDIR}tools/Llvm/x64/bin/clang-cl.exe")
	set(CMAKE_C_COMPILER "$ENV{VCINSTALLDIR}tools/Llvm/x64/bin/clang-cl.exe")
	set(CMAKE_CXX_COMPILER "$ENV{VCINSTALLDIR}tools/Llvm/x64/bin/clang-cl.exe")
else()
	set(CMAKE_C_COMPILER clang-cl)
	set(CMAKE_CXX_COMPILER clang-cl)
endif()

set(CMAKE_C_FLAGS_INIT "--target=x86_64-pc-windows-msvc")
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-pc-windows-msvc")
