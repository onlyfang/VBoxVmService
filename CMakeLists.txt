# CMake version
cmake_minimum_required(VERSION 3.4)

# Use static runtime library
foreach(var CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE)
  if(${var} MATCHES "/MD")
    string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
  endif()
endforeach()

# define project
project (VBoxVmService)

file(GLOB util_SRC "src/util.*")
add_library(util ${util_SRC})

# VBoxVmService
file(GLOB VBoxVmService_SRC "src/VBoxVmService.*" "src/VBoxVmPipeManager.*" "src/VirtualBox*.*")
add_executable(VBoxVmService ${VBoxVmService_SRC})

# VmServiceControl
file(GLOB VmServiceControl_SRC "src/VmServiceControl.*")
add_executable(VmServiceControl ${VmServiceControl_SRC})

# VmServiceTray
file(GLOB VmServiceTray_SRC "src/VmServiceTray.*")
add_executable(VmServiceTray ${VmServiceTray_SRC})
foreach (BUILD_TYPE DEBUG RELEASE)
  set_target_properties(VmServiceTray PROPERTIES LINK_FLAGS_${BUILD_TYPE} "/SUBSYSTEM:WINDOWS,6.01")
endforeach()

foreach (EXE VBoxVmService VmServiceControl VmServiceTray)
  target_link_libraries(${EXE} util)
endforeach()
