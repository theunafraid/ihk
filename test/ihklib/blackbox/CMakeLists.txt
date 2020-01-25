cmake_policy(SET CMP0005 NEW)

# Options: -DWITH_MCK=<McKernel install directory>
add_definitions(-DWITH_MCK=${WITH_MCK})

cmake_minimum_required(VERSION 3.0)
  
project(ihk_rmif_bbtest C)

# CPPFLAGS
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  add_definitions(-DBUILD_TARGET=smp-x86)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  add_definitions(-DBUILD_TARGET=smp-arm64)
endif()

# CFLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")

# -L, this must be done before adding dependants
link_directories("${WITH_MCK}/lib64")

# -Wl,--rpath=, this must be done before adding dependants
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${WITH_MCK}/lib64")

# Add library
add_library(init_fini STATIC src/insmod.c src/rmmod.c)
target_include_directories(init_fini
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  )

add_library(input_vector STATIC src/input_vector.c)
target_include_directories(input_vector
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  )

add_library(params STATIC src/params.c)
target_include_directories(params
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  )

add_library(check STATIC src/check.c)
target_include_directories(check
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${WITH_MCK}/include"
  )
target_link_libraries(check
  PUBLIC ihk
  )

foreach(target IN ITEMS
    ihk_reserve_cpu01
    ihk_reserve_cpu02
    ihk_reserve_cpu03
    ihk_reserve_cpu04
    ihk_reserve_cpu05
    ihk_reserve_cpu06
    ihk_get_num_reserved_cpus01
    ihk_get_num_reserved_cpus02
    ihk_get_num_reserved_cpus03
    ihk_get_num_reserved_cpus05
    ihk_get_num_reserved_cpus06
    )

  # Add target
  add_executable(${target} src/${target}.c)

  # -I
  target_include_directories(${target}
    PRIVATE "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${WITH_MCK}/include"
    )

  # -l
  target_link_libraries(${target}
    PRIVATE params input_vector init_fini check
    )

  # String replacement
  configure_file(src/${target}.sh.in ${target}.sh)

  # Install
  install(TARGETS ${target} DESTINATION bin)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${target}.sh DESTINATION bin)

endforeach()

