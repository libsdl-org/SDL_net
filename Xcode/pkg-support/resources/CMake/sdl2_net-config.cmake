# SDL2_net CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL2_net framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

include(FeatureSummary)
set_package_properties(SDL2_net PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_net/"
    DESCRIPTION "SDL_net is an example portable network library for use with SDL."
)

set(SDL2_net_FOUND TRUE)

string(REGEX REPLACE "SDL2_net\\.framework.*" "SDL2_net.framework" _sdl2net_framework_path "${CMAKE_CURRENT_LIST_DIR}")
string(REGEX REPLACE "SDL2_net\\.framework.*" "" _sdl2net_framework_parent_path "${CMAKE_CURRENT_LIST_DIR}")


if(NOT TARGET SDL2_net::SDL2_net)
    add_library(SDL2_net::SDL2_net INTERFACE IMPORTED)
    set_target_properties(SDL2_net::SDL2_net
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "SHELL:-F \"${_sdl2net_framework_parent_path}\""
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl2net_framework_path}/Headers"
            INTERFACE_LINK_OPTIONS "SHELL:-F \"${_sdl2net_framework_parent_path}\";SHELL:-framework SDL2_net"
            COMPATIBLE_INTERFACE_BOOL "SDL2_SHARED"
            INTERFACE_SDL2_SHARED "ON"
    )
endif()

unset(_sdl2net_framework_path)
unset(_sdl2net_framework_parent_path)
