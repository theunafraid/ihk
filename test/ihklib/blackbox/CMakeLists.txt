cmake_policy(SET CMP0005 NEW)

# Options: -DWITH_MCK=<McKernel install directory>
add_definitions(-DWITH_MCK=${WITH_MCK})

cmake_minimum_required(VERSION 3.0)

project(ihk_rmif_bbtest C)

# CPPFLAGS
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  add_definitions(-DBUILD_TARGET=smp-x86)
  add_definitions(-DKMOD_POSTFIX=smp_x86)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  add_definitions(-DBUILD_TARGET=smp-arm64)
  add_definitions(-DKMOD_POSTFIX=smp_arm64)
endif()

add_definitions(-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})

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

add_library(cpu STATIC src/cpu.c)
target_include_directories(cpu
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${WITH_MCK}/include"
  )
target_link_libraries(cpu
  PUBLIC ihk
  )

add_library(mem STATIC src/mem.c)
target_include_directories(mem
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${WITH_MCK}/include"
  )
target_link_libraries(mem
  PUBLIC ihk
  )

add_library(os STATIC src/os.c)
target_include_directories(os
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${WITH_MCK}/include"
  )
target_link_libraries(os
  PUBLIC ihk
  )

add_library(params STATIC src/params.c)
target_include_directories(params
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
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
    ihk_query_cpu01
    ihk_query_cpu02
    ihk_query_cpu03
    ihk_query_cpu04
    ihk_query_cpu05
    ihk_query_cpu06
    ihk_query_mem02
    ihk_release_cpu01
    ihk_release_cpu02
    ihk_release_cpu03
    ihk_release_cpu04
    ihk_release_cpu05
    ihk_release_cpu06
    ihk_release_mem01
    ihk_release_mem02
    ihk_reserve_mem01
    ihk_reserve_mem02
    ihk_reserve_mem07
    ihk_reserve_mem08
    ihk_os_assign_cpu01
    ihk_os_assign_cpu02
    ihk_os_assign_cpu03
    ihk_os_assign_cpu04
    ihk_os_assign_mem01
    ihk_os_assign_mem02
    ihk_os_assign_mem03
    ihk_os_assign_mem04
    ihk_os_get_num_assigned_cpus02
    ihk_os_get_num_assigned_mem_chunks02
    ihk_os_release_cpu02
    ihk_os_query_mem02
    ihk_os_query_cpu02
    ihk_os_release_mem02
    ihk_get_num_reserved_mem_chunks02
    ihk_os_shutdown07
    ihk_os_set_ikc_map07
    ikc_map
    ihk_create_os01
    ihk_destroy_os07
    ihk_os_get_ikc_map07
    ihk_destroy_os03
    ihk_destroy_os08
    ihk_destroy_os05
    ihk_destroy_os06
    ihk_get_num_os_instances01
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
    PRIVATE params cpu mem os init_fini
    )

  # String replacement
  configure_file(src/${target}.sh.in ${target}.sh @ONLY)

  # Install
  install(TARGETS ${target} DESTINATION bin)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${target}.sh DESTINATION bin)

endforeach()

# kernel module
add_custom_target(test_driver ALL COMMAND make && mv test_driver.ko ${CMAKE_BINARY_DIR} && make clean
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/driver
)
install(PROGRAMS ${CMAKE_BINARY_DIR}/test_driver.ko DESTINATION kmod)

configure_file(driver/test_driver_insmod.sh.in test_driver_insmod.sh)
install(PROGRAMS ${CMAKE_BINARY_DIR}/test_driver_insmod.sh DESTINATION bin)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/driver/test_driver_rmmod.sh DESTINATION bin)

