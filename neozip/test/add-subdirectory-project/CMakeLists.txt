cmake_minimum_required(VERSION 3.14...4.2.1)

project(neozip-add-subdirecory-test C)

include(CTest)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING ON CACHE BOOL "Build test binaries" FORCE)

add_subdirectory(../.. neozip)

add_executable(app main.c)
target_link_libraries(app neozip-static)
