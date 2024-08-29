cmake_minimum_required(VERSION 3.12)

project(portal C)

set(CMAKE_C_STANDARD 99)
add_compile_options(-O0)


add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bin2c.exe"
  COMMAND ${CMAKE_C_COMPILER} -o "${CMAKE_CURRENT_BINARY_DIR}/bin2c.exe" "${CMAKE_CURRENT_LIST_DIR}/tool/bin2c.c"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h"
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/bin2c.exe" -o "${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h" -n indexhtml "${CMAKE_CURRENT_LIST_DIR}/src/index.html"
  DEPENDS src/index.html "${CMAKE_CURRENT_BINARY_DIR}/bin2c.exe"
)


add_executable(portal
  "${CMAKE_CURRENT_BINARY_DIR}/res_indexhtml.h"
  src/main.c
)

target_include_directories(portal PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")


option(ENABLE_GUI "Include GUI" ON)

# windows
option(PLATFORM_GDI "Include support for Windows (GDI)" ON)
# macos
# options(PLATFORM_)
# linux
option(PLATFORM_X11 "Include support for x11" ON)
option(PLATFORM_FBDEV "Include support for fbdev" ON)


if(ENABLE_GUI)
  target_compile_options(portal PRIVATE "-DPORTAL_ENABLE_GUI")
endif(ENABLE_GUI)

# windows

if(WIN32)
  if(PLATFORM_GDI)
    target_compile_options(portal PRIVATE "-DLIBPORTAL_PLATFORM_HAS_GDI")
    target_link_libraries(portal PRIVATE ws2_32 gdi32)
  endif(PLATFORM_GDI)
endif(WIN32)

# macos

if(APPLE)

error("Apple platforms haven't been supported yet")

endif(APPLE)

# unix ## TODO linux or unix?

if(UNIX)

  if(PLATFORM_FBDEV)
    target_compile_options(portal PRIVATE "-DLIBPORTAL_PLATFORM_HAS_FBDEV")
  endif(PLATFORM_FBDEV)

  if(PLATFORM_X11)
    target_compile_options(portal PRIVATE "-DLIBPORTAL_PLATFORM_HAS_X11")
    target_link_libraries(portal PRIVATE
      X11 Xext
    )
  endif(PLATFORM_X11)

endif(UNIX)

