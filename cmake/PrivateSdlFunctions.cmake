# This file is shared amongst SDL_image/SDL_mixer/SDL_ttf/SDL_net

macro(sdl_calculate_derived_version_variables)
    if (NOT DEFINED MAJOR_VERSION OR NOT DEFINED MINOR_VERSION OR NOT DEFINED MICRO_VERSION)
        message(FATAL_ERROR "MAJOR_VERSION, MINOR_VERSION and MICRO_VERSION need to be defined")
    endif()

    set(FULL_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}")

    # Calculate a libtool-like version number
    math(EXPR BINARY_AGE "${MINOR_VERSION} * 100 + ${MICRO_VERSION}")
    math(EXPR IS_DEVELOPMENT "${MINOR_VERSION} % 2")
    if (IS_DEVELOPMENT)
        # Development branch, 2.5.1 -> libSDL2_XXXXX-2.0.so.0.501.0
        set(INTERFACE_AGE 0)
    else()
        # Stable branch, 2.6.1 -> libSDL2_XXXXX-2.0.so.0.600.1
        set(INTERFACE_AGE ${MICRO_VERSION})
    endif()

    # Increment this if there is an incompatible change - but if that happens,
    # we should rename the library from SDL2 to SDL3, at which point this would
    # reset to 0 anyway.
    set(LT_MAJOR "0")

    math(EXPR LT_AGE "${BINARY_AGE} - ${INTERFACE_AGE}")
    math(EXPR LT_CURRENT "${LT_MAJOR} + ${LT_AGE}")
    set(LT_REVISION "${INTERFACE_AGE}")
    # For historical reasons, the library name redundantly includes the major
    # version twice: libSDL2_XXXXX-2.0.so.0.
    # TODO: in SDL 3, set the OUTPUT_NAME to plain SDL3_XXXXX, which will simplify
    # it to libSDL2_XXXXX.so.0
    set(LT_RELEASE "2.0")
    set(LT_VERSION "${LT_MAJOR}.${LT_AGE}.${LT_REVISION}")

    # The following should match the versions in the Xcode project file.
    # Each version is 1 higher than you might expect, for compatibility
    # with libtool: macOS ABI versioning is 1-based, unlike other platforms
    # which are normally 0-based.
    math(EXPR DYLIB_CURRENT_VERSION_MAJOR "${LT_MAJOR} + ${LT_AGE} + 1")
    math(EXPR DYLIB_CURRENT_VERSION_MINOR "${LT_REVISION}")
    math(EXPR DYLIB_COMPAT_VERSION_MAJOR "${LT_MAJOR} + 1")
    set(DYLIB_CURRENT_VERSION "${DYLIB_CURRENT_VERSION_MAJOR}.${DYLIB_CURRENT_VERSION_MINOR}.0")
endmacro()

macro(sdl_find_sdl2 TARGET VERSION)
    if(NOT TARGET ${TARGET})
        # FIXME: can't add REQUIRED since not all SDL2 installs ship SDL2ConfigVersion.cmake (or sdl2-config-version.cmake)
        find_package(SDL2 ${VERSION} QUIET)
    endif()
    if(NOT TARGET ${TARGET})
        # FIXME: can't add REQUIRED since not all SDL2 installs ship SDL2Config.cmake (or sdl2-config.cmake)
        find_package(SDL2 QUIET)
        if(SDL2_FOUND)
            message(WARNING "Could not verify SDL2 version. Assuming SDL2 has version of at least ${VERSION}.")
        endif()
    endif()

    # Use Private FindSDL2.cmake module to find SDL2 for installations where no SDL2Config.cmake is available,
    # or for those installations where no target is generated.
    if (NOT TARGET ${TARGET})
        message(STATUS "Using private SDL2 find module")
        find_package(PrivateSDL2 ${VERSION} REQUIRED)
        add_library(${TARGET} INTERFACE IMPORTED)
        set_target_properties(${TARGET} PROPERTIES
            INTERFACE_LINK_LIBRARIES "PrivateSDL2::PrivateSDL2"
        )
    endif()
endmacro()

macro(sdl_check_linker_flag flag var)
    # FIXME: Use CheckLinkerFlag module once cmake minimum version >= 3.18
    include(CMakePushCheckState)
    include(CheckCSourceCompiles)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LINK_OPTIONS "-Wl,--no-undefined")
    check_c_source_compiles("int main() { return 0; }" ${var})
    cmake_pop_check_state()
endmacro()

function(sdl_target_link_options_no_undefined TARGET)
    if(NOT MSVC)
        if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
            target_link_options(${TARGET} PRIVATE "-Wl,-undefined,error")
        else()
            sdl_check_linker_flag("-Wl,--no-undefined" HAVE_WL_NO_UNDEFINED)
            if(HAVE_WL_NO_UNDEFINED)
                target_link_options(${TARGET} PRIVATE "-Wl,--no-undefined")
            endif()
        endif()
    endif()
endfunction()
