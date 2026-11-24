# 第 11 批：最小的 aarch64 交叉编译工具链文件（toolchain file）。
# 用法：colcon build --cmake-args -DCMAKE_TOOLCHAIN_FILE=<本文件> [-DCMAKE_SYSROOT=<目标机根文件系统>]
# 需要先装交叉编译器：apt install g++-aarch64-linux-gnu
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 头文件、库、CMake 包只在 sysroot 里找，程序（代码生成器等）在宿主机上找。
# 设计文档里 colcon mixin 和 generic_linux.cmake 做的就是这件事。
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
if(CMAKE_SYSROOT)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
