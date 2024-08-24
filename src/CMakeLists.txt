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

add_library(surge-juce INTERFACE)
target_compile_definitions(surge-juce INTERFACE
  JUCE_ALLOW_STATIC_NULL_VARIABLES=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1

  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CAMERA=disabled

  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0

  JUCE_MODAL_LOOPS_PERMITTED=0

  JUCE_COREGRAPHICS_DRAW_ASYNC=1

  JUCE_ALSA=$<IF:$<BOOL:${SURGE_USE_ALSA}>,1,0>
  JUCE_JACK=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

  JUCE_WASAPI=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>
  JUCE_DIRECTSOUND=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

  JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
  )

if (NOT SURGE_SKIP_JUCE_FOR_RACK)
  target_link_libraries(surge-juce INTERFACE melatonin_inspector)
endif()

if(NOT SURGE_SKIP_VST3)
  list(APPEND SURGE_JUCE_FORMATS VST3)
endif()

if(NOT SURGE_SKIP_STANDALONE)
  list(APPEND SURGE_JUCE_FORMATS Standalone)
endif()

if(APPLE AND NOT CMAKE_CROSSCOMPILING)
  # AU requires the Rez resource compiler, which seems to be unavailable on osxcross
  list(APPEND SURGE_JUCE_FORMATS AU)
endif()

if(SURGE_XT_BUILD_AUV3)
  list(APPEND SURGE_JUCE_FORMATS AUv3)
endif()

if(DEFINED ENV{VST2SDK_DIR})
  file(TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" JUCE_VST2_DIR)
  juce_set_vst2_sdk_path(${JUCE_VST2_DIR})
  list(APPEND SURGE_JUCE_FORMATS VST)
  message(STATUS "VST2 SDK path is $ENV{VST2SDK_DIR}")
  # VST2 headers are invalid UTF-8
  add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/wd4828>)
endif()

if(SURGE_BUILD_LV2)
  list(APPEND SURGE_JUCE_FORMATS LV2)
  message(STATUS "Including JUCE7 LV2 support.")
endif()


if(DEFINED ENV{ASIOSDK_DIR} OR BUILD_USING_MY_ASIO_LICENSE)
  if(BUILD_USING_MY_ASIO_LICENSE)
    message(STATUS "** BUILD USING OWN ASIO LICENSE **")
    message(STATUS "The resulting Surge standalone executable is not licensed for distribution!")
    message(STATUS "Fetching ASIO SDK...")

    set(ASIOSDK_DIR ${CMAKE_BINARY_DIR}/asio/asiosdk)
    add_custom_target(surge-get-local-asio)
    add_custom_command(
      TARGET surge-get-local-asio
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/asio
      COMMAND ${CMAKE_COMMAND} -D ASIO_SDK_DESTINATION=${CMAKE_BINARY_DIR}/asio -P cmake/get-asio.cmake
    )
    add_dependencies(surge-juce surge-get-local-asio)
  else()
    file(TO_CMAKE_PATH "$ENV{ASIOSDK_DIR}" ASIOSDK_DIR)
    message(STATUS "ASIO SDK found at ${ASIOSDK_DIR}")
    message(STATUS "The resulting Surge standalone executable is not licensed for distribution!")
  endif()

  target_compile_definitions(surge-juce INTERFACE JUCE_ASIO=1)
  target_include_directories(surge-juce INTERFACE ${ASIOSDK_DIR}/common)
  set(JUCE_ASIO_SUPPORT TRUE)
endif()

message(STATUS "Building Surge XT using the following JUCE wrappers: ${SURGE_JUCE_FORMATS}")

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
