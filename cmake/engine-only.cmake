# Shared, JUCE-free engine build for Wasm and the native comparison harness.
set(SURGE_COMPILE_BLOCK_SIZE 32)
set(SURGE_SKIP_ODDSOUND_MTS ON)
set(SURGE_PORTABLE_LUA ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CmakeRC.cmake)
add_library(simde INTERFACE)
target_include_directories(simde INTERFACE ${CMAKE_SOURCE_DIR}/libs/simde)
add_library(surge::simde ALIAS simde)
# The common engine only consumes the compile configuration of this target.
add_library(surge-juce INTERFACE)
add_subdirectory(common)
if(EMSCRIPTEN)
  # Replace host probing, dladdr, and native stack traces at the platform boundary.
  set_property(TARGET sst-plugininfra PROPERTY SOURCES
    ${CMAKE_BINARY_DIR}/gen/paths_subst.cpp
    ${CMAKE_SOURCE_DIR}/src/surge-web/Platform.cpp)
  set_property(TARGET sst-plugininfra PROPERTY LINK_LIBRARIES
    sst-plugininfra::filesystem sst-plugininfra::strnatcmp)
endif()
add_subdirectory(lua)
add_subdirectory(platform)
add_subdirectory(surge-web)
