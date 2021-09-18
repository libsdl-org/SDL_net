include(CMakeFindDependencyMacro)
find_dependency(SDL2)
include("${CMAKE_CURRENT_LIST_DIR}/SDL2_netTargets.cmake")
