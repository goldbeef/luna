cmake_minimum_required(VERSION 3.14)
project(luna)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(
        # kernel headers
        "/usr/local/Cellar/lua/5.3.5_1/include/lua5.3"
)
add_executable(luna
        lua_archiver.cpp
        lua_archiver.h
        luna.cpp
        luna.h
        luna11.h
        lz4.c
        lz4.h
        makefile
        README.md
        var_int.cpp
        var_int.h example/test.cpp)
