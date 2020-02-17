cmake_policy(SET CMP0005 NEW)

# Options: -DWITH_MCK=<McKernel install directory>
add_definitions(-DWITH_MCK=${WITH_MCK})

# Options: -DWITH_MCK_SRC=<McKernel source directory>
add_definitions(-DWITH_MCK_SRC=${WITH_MCK_SRC})

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

# Add libraries
foreach(target IN ITEMS
    cpu
    mem
    os
    )
  add_library(${target} STATIC src/${target}.c)
  target_include_directories(${target}
    PRIVATE "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${WITH_MCK}/include"
    )
  target_link_libraries(${target}
    PUBLIC ihk
    )
endforeach()

foreach(target IN ITEMS
    params
    mod
    user
    )
  add_library(${target} STATIC src/${target}.c)
  target_include_directories(${target}
    PRIVATE "${PROJECT_SOURCE_DIR}/include"
    )
endforeach()

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
    ihk_release_mem03
    ihk_release_mem04
    ihk_release_mem05
    ihk_release_mem06
    ihk_release_mem07
    ihk_reserve_mem01
    ihk_reserve_mem02
    ihk_reserve_mem07
    ihk_reserve_mem08
    ihk_os_assign_cpu01
    ihk_os_assign_cpu02
    ihk_os_assign_cpu03
    ihk_os_assign_cpu04
    ihk_os_assign_cpu07
    ihk_os_assign_mem01
    ihk_os_assign_mem02
    ihk_os_assign_mem03
    ihk_os_assign_mem04
    ihk_os_get_num_assigned_cpus02
    ihk_os_get_num_assigned_mem_chunks02
    ihk_os_query_mem02
    ihk_os_query_cpu02
    ihk_get_num_reserved_mem_chunks02
    ihk_os_shutdown07
    ihk_os_set_ikc_map07
    ikc_map
    ihk_create_os01
    #  ihk_destroy_os07
    #ihk_os_get_ikc_map07
    ihk_destroy_os03
    #ihk_destroy_os08
    ihk_destroy_os05
    ihk_destroy_os06
    ihk_get_num_os_instances01
    ihk_os_shutdown08
    ihk_os_shutdown01
    ihk_os_release_mem01
    ihk_os_release_mem02
    ihk_os_release_mem03
    ihk_os_release_mem04
    ihk_os_release_mem05
    ihk_os_release_mem06
    ihk_os_release_mem07
    ihk_os_release_mem08
    ihk_os_release_cpu01
    ihk_os_release_cpu02
    ihk_os_release_cpu03
    ihk_os_release_cpu04
    ihk_os_release_cpu05
    ihk_os_release_cpu06
    ihk_os_release_cpu07
    )

  # Add target
  add_executable(${target} src/${target}.c)

  # -I
  target_include_directories(${target}
    PRIVATE "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${WITH_MCK}/include"
    PRIVATE "${WITH_MCK_SRC}/ihk/linux/include"
    )

  # -l
  target_link_libraries(${target}
    PRIVATE params cpu mem os user mod
    )

  # String replacement
  configure_file(src/${target}.sh.in ${target}.sh @ONLY)

  # Install
  install(TARGETS ${target} DESTINATION bin)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${target}.sh DESTINATION bin)

endforeach()

# patches
foreach(target IN ITEMS
    status_mckernel status_ihk
    )
  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/${target}.patch DESTINATION bin)
endforeach()

# programs to run on McKernel
foreach(target IN ITEMS
    panic
    hungup
    )

  # Add target
  add_executable(${target} src/${target}.c)

  # Install
  install(TARGETS ${target} DESTINATION bin)

endforeach()

# kernel module
add_custom_target(test_driver ALL COMMAND make && mv test_driver.ko ${CMAKE_BINARY_DIR} && make clean
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/driver
)
install(PROGRAMS ${CMAKE_BINARY_DIR}/test_driver.ko DESTINATION kmod)

configure_file(driver/test_driver_insmod.sh.in test_driver_insmod.sh)
install(PROGRAMS ${CMAKE_BINARY_DIR}/test_driver_insmod.sh DESTINATION bin)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/driver/test_driver_rmmod.sh DESTINATION bin)

