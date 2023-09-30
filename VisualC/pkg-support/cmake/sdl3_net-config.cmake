# SDL3_net CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL3_net-devel-3.x.y-VC

include(FeatureSummary)
set_package_properties(SDL3_net PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_net/"
    DESCRIPTION "SDL_net is a portable network library for use with SDL."
)
cmake_minimum_required(VERSION 3.0)

set(SDL3_net_FOUND TRUE)

if(CMAKE_SIZEOF_VOID_P STREQUAL "4")
    set(_sdl_arch_subdir "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL "8")
    set(_sdl_arch_subdir "x64")
else()
    unset(_sdl_arch_subdir)
    set(SDL3_net_FOUND FALSE)
    return()
endif()

set(_sdl3net_incdir       "${CMAKE_CURRENT_LIST_DIR}/../include")
set(_sdl3net_library      "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL3_net.lib")
set(_sdl3net_dll          "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL3_net.dll")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3_net-target.cmake files.

if(NOT TARGET SDL3_net::SDL3_net-shared)
    add_library(SDL3_net::SDL3_net-shared SHARED IMPORTED)
    set_target_properties(SDL3_net::SDL3_net-shared
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3net_incdir}"
            IMPORTED_IMPLIB "${_sdl3net_library}"
            IMPORTED_LOCATION "${_sdl3net_dll}"
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
    )
endif()

unset(_sdl_arch_subdir)
unset(_sdl3net_incdir)
unset(_sdl3net_library)
unset(_sdl3net_dll)
