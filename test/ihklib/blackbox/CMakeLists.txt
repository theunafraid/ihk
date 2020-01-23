# Options:
#		-DWITH_MCK=<McKernel install directory>

cmake_minimum_required(VERSION 3.0)

project(ihk_resource_mgmt_if_bbtest C)

# BUILD_TARGET is used to construct paths
# Ex. ihk-<BUILD_TARGET>.ko, <BUILD_TARGET>/kernel/mckernel.img
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  add_definitions(-DBUILD_TARGET=smp-x86)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  add_definitions(-DBUILD_TARGET=smp-arm64)
endif()

add_definitions(-DWITH_MCK=${WITH_MCK})

# -Wl,--rpath=
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_INSTALL_RPATH "${WITH_MCK}/lib64")

# -L
link_directories("${WITH_MCK}/lib64")

# Add target
add_executable(001_lin src/dev/cpu/001_lin.c)

# -I
target_include_directories(001_lin
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../../linux/include"
  )

# -l
target_link_libraries(001_lin
  PRIVATE ihk
  )

install(TARGETS 001_lin DESTINATION bin)
install(PROGRAMS src/dev/cpu/001.sh DESTINATION bin)
