cmake_minimum_required(VERSION 3.12)

project(portal C)

set(LIBPORTAL_FLAGS "-O2 -g")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LIBPORTAL_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBPORTAL_FLAGS}")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h"
  COMMAND lua ${CMAKE_CURRENT_LIST_DIR}/tool/bin2c.lua -o ${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h -n indexhtml ${CMAKE_CURRENT_LIST_DIR}/src/index.html
  DEPENDS src/index.html
)

add_executable(portal
  "${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h"
  src/main.c
)

target_include_directories(portal PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# windows

if(WIN32)

target_link_libraries(portal

)

endif(WIN32)

# unix

if(UNIX)

# TODO add platform detect and defs

find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)

# target_include_directories(portal PRIVATE ${X11_INCLUDE_DIRS})
target_link_libraries(portal PRIVATE
  ${X11_LIBRARIES}
)

endif(UNIX)

