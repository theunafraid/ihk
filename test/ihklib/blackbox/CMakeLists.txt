# Options:
#		-DWITH_MCK=<McKernel install directory>

cmake_minimum_required(VERSION 3.0)

project(ihk_rmif_bbtest C)

# Pass string to .c
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  add_definitions(-DBUILD_TARGET=smp-x86)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  add_definitions(-DBUILD_TARGET=smp-arm64)
endif()

add_definitions(-DWITH_MCK=${WITH_MCK})

# -Wl,--rpath=, this must be done before adding dependants
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${WITH_MCK}/lib64")

# -L, this must be done before adding dependants
link_directories("${WITH_MCK}/lib64")

# Add library
add_library(init_fini STATIC common/insmod.c common/rmmod.c)
target_include_directories(init_fini
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  )

add_library(input_vector STATIC common/input_vector.c)
target_include_directories(input_vector
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  )

# Add target
add_executable(001_lin dev/cpu/001_lin.c)

# CFLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")

# -I
target_include_directories(001_lin
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/../../../linux/include"
  )

# -l
target_link_libraries(001_lin
  PRIVATE ihk init_fini input_vector
  )

# Install
install(TARGETS 001_lin DESTINATION bin)
install(PROGRAMS dev/cpu/001.sh DESTINATION bin)
