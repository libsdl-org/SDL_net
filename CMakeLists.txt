cmake_minimum_required(VERSION 3.16)
project(SDL3_net VERSION 3.0.0)

find_package(SDL3 REQUIRED COMPONENTS SDL3)

add_library(SDL3_net SHARED SDL_net.c)
add_library(SDL3_net::SDL3_net ALIAS SDL3_net)
target_include_directories(SDL3_net PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(SDL3_net PRIVATE SDL3::SDL3)
if(WIN32)
    target_link_libraries(SDL3_net PRIVATE ws2_32)
    set_target_properties(SDL3_net PROPERTIES
        OUTPUT_NAME "SDL3_net"
        DEFINE_SYMBOL DLL_EXPORT
        EXPORT_NAME SDL3_net
        C_VISIBILITY_PRESET "hidden"
    )
endif()
if(UNIX AND NOT APPLE)
    set_property(TARGET SDL3_net PROPERTY SOVERSION ${PROJECT_VERSION_MAJOR})
    set_property(TARGET SDL3_net PROPERTY VERSION ${PROJECT_VERSION})
endif()
if(NOT MSVC)
    target_link_options(SDL3_net PRIVATE -Wl,--no-undefined)
endif()

add_executable(voipchat examples/voipchat.c)
target_link_libraries(voipchat PRIVATE SDL3_net::SDL3_net SDL3::SDL3)

add_executable(simple-http-get examples/simple-http-get.c)
target_link_libraries(simple-http-get PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
