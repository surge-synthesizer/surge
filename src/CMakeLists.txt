# vi:set sw=2 et:
option(SURGE_BUILD_FX "Build Surge FX bank" ON)
option(SURGE_BUILD_XT "Build Surge XT synth" ON)
option(SURGE_BUILD_PYTHON_BINDINGS "Build Surge Python bindings with pybind11" OFF)
option(SURGE_COPY_TO_PRODUCTS "Copy built plugins to the products directory" ON)
option(SURGE_COPY_AFTER_BUILD "Copy JUCE plugins to system plugin area after build" OFF)
option(SURGE_EXPOSE_PRESETS "Expose surge presets via the JUCE Program API" OFF)

# Currently the JUCE LV2 build crashes in our CI pipeline, so leave it for users to self build
option(SURGE_BUILD_LV2 "Build Surge as an LV2" OFF)
if (NOT SURGE_COMPILE_BLOCK_SIZE)
  set(SURGE_COMPILE_BLOCK_SIZE 32)
endif()

set(SURGE_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libs/JUCE" CACHE STRING "Path to JUCE library source tree")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/lib.cmake)

if (NOT SURGE_SKIP_JUCE_FOR_RACK)
  message(STATUS "Using JUCE from ${SURGE_JUCE_PATH}")
  add_subdirectory(${SURGE_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../libs/melatonin_inspector" ${CMAKE_BINARY_DIR}/melatonin_inspector EXCLUDE_FROM_ALL)
endif()

if(${CMAKE_VERSION} VERSION_LESS 3.21)
  message(WARNING "CMake version less than 3.21. Skipping clap builds. Please "
    "consider using a newer version of CMake")
  set(SURGE_BUILD_CLAP FALSE)
else()
  message(STATUS "CMake version ${CMAKE_VERSION} allows CLAP build")
  set(SURGE_BUILD_CLAP TRUE)
endif()

if(SURGE_BUILD_CLAP)
  add_subdirectory(../libs/clap-juce-extensions clap-extensions EXCLUDE_FROM_ALL)
endif()

if(SURGE_SKIP_STANDALONE OR SURGE_SKIP_ALSA)
  set(SURGE_USE_ALSA FALSE)
else()
  set(SURGE_USE_ALSA TRUE)
endif()

set(SURGE_SIMDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libs/simde" CACHE STRING "Path to simde library source tree")
add_library(simde INTERFACE)
target_include_directories(simde INTERFACE ${SURGE_SIMDE_PATH})
add_library(surge::simde ALIAS simde)

include (cmake/setup-juce.cmake)

add_subdirectory(common)
add_subdirectory(lua)
add_subdirectory(platform)

if(SURGE_BUILD_TESTRUNNER AND NOT SURGE_SKIP_JUCE_FOR_RACK)
  add_subdirectory(surge-testrunner)
endif()

if(SURGE_BUILD_FX AND NOT SURGE_SKIP_JUCE_FOR_RACK)
  add_subdirectory(surge-fx)
endif()

if(SURGE_BUILD_XT AND NOT SURGE_SKIP_JUCE_FOR_RACK)
  add_subdirectory(surge-xt)
endif()

if(SURGE_BUILD_PYTHON_BINDINGS)
  add_subdirectory(surge-python)
endif()

if(SURGE_BUILD_FX AND SURGE_BUILD_XT AND NOT CMAKE_CROSSCOMPILING)
  surge_make_installers()
endif()

if(NOT CMAKE_CROSSCOMPILING AND NOT SURGE_SKIP_JUCE_FOR_RACK)
  include(cmake/pluginval.cmake)
endif()
