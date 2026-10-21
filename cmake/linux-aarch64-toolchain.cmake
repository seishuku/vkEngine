set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT "/opt/aarch64-sysroot")

file(GLOB _gcc_dirs "${CMAKE_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/*")
list(SORT _gcc_dirs COMPARE NATURAL)
list(POP_BACK _gcc_dirs _gcc_install_dir)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

set(CMAKE_C_FLAGS_INIT   "--gcc-install-dir=${_gcc_install_dir}")
set(CMAKE_CXX_FLAGS_INIT "--gcc-install-dir=${_gcc_install_dir}")

set(_lld_flags "-fuse-ld=lld -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_lld_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lld_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lld_flags}")

set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
