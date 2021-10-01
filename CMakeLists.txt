if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(
    FATAL_ERROR
      "Prevented in-tree build. Please create a build directory outside of the SDL_net source code and run \"cmake -S ${CMAKE_SOURCE_DIR} -B .\" from there"
  )
endif()

cmake_minimum_required(VERSION 3.0.0)
project(SDL_net C)

if(ANDROID)
  add_library(SDL2_net SHARED)
  target_sources(SDL2_net PRIVATE SDLnet.c SDLnetselect.c SDLnetTCP.c
                                  SDLnetUDP.c)
  target_include_directories(SDL2_net PUBLIC .)
  target_link_libraries(SDL2_net PRIVATE SDL2)
else()

  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  if(NOT (TARGET SDL2 OR TARGET SDL2-static))
    find_package(SDL2 REQUIRED)
  endif()
  find_package(PkgConfig QUIET)

  set(SDL_NET_MAJOR_VERSION 2)
  set(SDL_NET_MINOR_VERSION 0)
  set(SDL_NET_MICRO_VERSION 1)
  # Are these two correct?
  set(SDL_NET_INTERFACE_AGE 1)
  set(SDL_NET_BINARY_AGE 1)

  set(SDL_NET_VERSION
      "${SDL_NET_MAJOR_VERSION}.${SDL_NET_MINOR_VERSION}.${SDL_NET_MICRO_VERSION}"
  )

  # library generation #####
  add_library(SDL2_net SDLnet.c SDLnetselect.c SDLnetTCP.c SDLnetUDP.c
                       SDL_net.h SDLnetsys.h)
  if(BUILD_SHARED_LIBS OR WIN32)
    target_link_libraries(SDL2_net SDL2::SDL2)
  else()
    target_link_libraries(SDL2_net SDL2::SDL2-static)
    set_target_properties(SDL2_net PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()

  # Windows requires additional libs
  if(WIN32)
    target_link_libraries(SDL2_net wsock32 ws2_32 iphlpapi)
  endif()

  target_include_directories(
    SDL2_net PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>)

  install(
    TARGETS SDL2_net
    EXPORT SDL2_netTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES SDL_net.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SDL2)

  # export files #####
  if(APPLE)
    set(PKG_PREFIX "SDL2_net.framework/Resources")
  elseif(WIN32)
    set(PKG_PREFIX "cmake")
  else()
    set(PKG_PREFIX "lib/cmake/SDL2")
  endif()

  write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/SDL2_netConfigVersion.cmake"
    VERSION ${SDL_NET_VERSION}
    COMPATIBILITY AnyNewerVersion)

  install(
    EXPORT SDL2_netTargets
    FILE SDL2_netTargets.cmake
    NAMESPACE SDL2_net::
    DESTINATION ${PKG_PREFIX})
  install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_netConfig.cmake
          ${CMAKE_BINARY_DIR}/SDL2_netConfigVersion.cmake
    DESTINATION ${PKG_PREFIX}
    COMPONENT Devel)

  # pkg-config #####
  if(PKG_CONFIG_FOUND)
    set(SDL_VERSION 2.0.0)
    set(prefix ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix "\${prefix}")
    set(libdir "\${exec_prefix}/lib${LIB_SUFFIX}")
    set(bindir "\${exec_prefix}/bin")
    set(includedir "\${prefix}/include")

    configure_file("${SDL_net_SOURCE_DIR}/SDL2_net.pc.in"
                   "${SDL_net_BINARY_DIR}/SDL2_net.pc" @ONLY)

    if(CMAKE_SYSTEM_NAME MATCHES FreeBSD)
      # FreeBSD uses ${PREFIX}/libdata/pkgconfig
      install(FILES ${SDL_net_BINARY_DIR}/SDL2_net.pc
              DESTINATION "libdata/pkgconfig")
    else()
      install(FILES ${SDL_net_BINARY_DIR}/SDL2_net.pc
              DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
    endif()
  endif()

endif()

option(BUILD_SHOWINTERFACE
       "Enable building of the test program to show network interface" OFF)

if(BUILD_SHOWINTERFACE)
  add_executable(showinterfaces showinterfaces.c)
  target_link_libraries(showinterfaces SDL2_net)
  # Windows requires SDL2main
  if(WIN32)
    target_link_libraries(showinterfaces SDL2::SDL2main)
  endif()
endif()
