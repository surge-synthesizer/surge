# vi:set sw=2 et:
option(SURGE_BUILD_TESTRUNNER "Build Surge unit test runner" ON)
option(SURGE_BUILD_FX "Build Surge FX bank" ON)
option(SURGE_BUILD_XT "Build Surge XT synth" ON)
option(SURGE_BUILD_PYTHON_BINDINGS "Build Surge Python bindings with pybind11" OFF)
option(SURGE_COPY_TO_PRODUCTS "Copy built plugins to the products directory" ON)
option(SURGE_COPY_AFTER_BUILD "Copy JUCE plugins to System Plugin area after build" OFF)

set(SURGE_JUCE_PATH "${CMAKE_SOURCE_DIR}/libs/JUCE" CACHE STRING "Path to JUCE library source tree")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/lib.cmake)

message(STATUS "Using JUCE from ${SURGE_JUCE_PATH}")
add_subdirectory(${SURGE_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE ${CMAKE_SOURCE_DIR}/libs/simde)
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

  JUCE_ALSA=1
  JUCE_JACK=1

  JUCE_WASAPI=1
  JUCE_DIRECTSOUND=1

  JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
  )

set(SURGE_JUCE_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND SURGE_JUCE_FORMATS AU)
endif()

if(SURGE_XT_BUILD_AUV3)
  list(APPEND SURGE_JUCE_FORMATS AUv3)
endif()

if(DEFINED ENV{VST2SDK_DIR})
  file(TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" JUCE_VST2_DIR)
  juce_set_vst2_sdk_path(${JUCE_VST2_DIR})
  list(APPEND SURGE_JUCE_FORMATS VST)
  message(STATUS "JUCE VST2 SDK Path is $ENV{VST2SDK_DIR}")
  # VST2 headers are invalid UTF-8
  add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/wd4828>)
endif()

if(JUCE_SUPPORTS_LV2)
  list(APPEND SURGE_JUCE_FORMATS LV2)
  message(STATUS "Including JUCE LV2 support. You will need a different JUCE than the submodule")
  message(STATUS "Turning off deprecation warning to error since JUCE LV2 uses deprecated APIs")
  add_compile_options(-Wno-error=deprecated-declarations)
endif()

if(BUILD_USING_MY_ASIO_LICENSE)
  message(STATUS "** BUILD WITH YOUR ASIO LICENSE **")
  message(STATUS "   Downloading ASIO SDK from Steinberg")
  message(STATUS "   The resulting Surge Standalones are not licensed for distribution")

  if(DEFINED ENV{ASIOSDK_DIR})
    file(TO_CMAKE_PATH "$ENV{ASIOSDK_DIR}" ASIOSDK_DIR)
    message(STATUS "ASIO SDK found at ${ASIOSDK_DIR}")
  else()
    message(STATUS "Fetching ASIO SDK")
    set(ASIOSDK_DIR ${CMAKE_BINARY_DIR}/asio/asiosdk)
    add_custom_target(surge-get-local-asio)
    add_custom_command(
      TARGET surge-get-local-asio
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/asio
      COMMAND ${CMAKE_COMMAND} -D ASIO_SDK_DESTINATION=${CMAKE_BINARY_DIR}/asio -P cmake/get-asio.cmake
    )
    add_dependencies(surge-juce surge-get-local-asio)
  endif()
  target_compile_definitions(surge-juce INTERFACE JUCE_ASIO=1)
  target_include_directories(surge-juce INTERFACE ${ASIOSDK_DIR}/common)
  set(JUCE_ASIO_SUPPORT TRUE)
endif()

message(STATUS "Building Surge JUCE as ${SURGE_JUCE_FORMATS}")

add_subdirectory(common)
add_subdirectory(lua)
add_subdirectory(platform)

if(SURGE_BUILD_TESTRUNNER)
  add_subdirectory(surge-testrunner)
endif()

if(SURGE_BUILD_FX)
  add_subdirectory(surge-fx)
endif()

if(SURGE_BUILD_XT)
  add_subdirectory(surge-xt)
endif()

if(SURGE_BUILD_PYTHON_BINDINGS)
  add_subdirectory(surge-python)
endif()

if(SURGE_BUILD_FX AND SURGE_BUILD_XT AND NOT CMAKE_CROSSCOMPILING)
  surge_make_installers()
endif()

# FIXME: Plugin validation {{{
if(APPLE)
  message(STATUS "Validate me! Please!")
  add_custom_target(stage-pluginval)
  add_custom_command(TARGET stage-pluginval
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND cmake -E make_directory pluginval
    COMMAND curl -L "https://github.com/Tracktion/pluginval/releases/download/latest_release/pluginval_macOS.zip" -o pluginval/pluginval.zip
    COMMAND cd pluginval && unzip -o pluginval.zip
    )
  add_custom_target(surge-xt-pluginval-vst3)
  add_dependencies(surge-xt-pluginval-vst3 surge-xt_VST3)
  add_dependencies(surge-xt-pluginval-vst3 stage-pluginval)
  add_custom_command(TARGET surge-xt-pluginval-vst3
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND pluginval/pluginval.app/Contents/MacOS/pluginval --validate-in-process --output-dir "." --validate "${SURGE_FX_OUTPUT_DIR}/VST3/Surge XT.vst3" || exit 1
    )

  add_custom_target(surge-xt-pluginval-au)
  add_dependencies(surge-xt-pluginval-au surge-xt_AU)
  add_dependencies(surge-xt-pluginval-au stage-pluginval)
  add_custom_command(TARGET surge-xt-pluginval-au
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND pluginval/pluginval.app/Contents/MacOS/pluginval --validate-in-process --output-dir "." --validate "${SURGE_XT_OUTPUT_DIR}/AU/Surge XT.component" || exit 1
    )

  add_custom_target(surge-fx-pluginval-vst3)
  add_dependencies(surge-fx-pluginval-vst3 surge-fx_VST3)
  add_dependencies(surge-fx-pluginval-vst3 stage-pluginval)
  add_custom_command(TARGET surge-fx-pluginval-vst3
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND pluginval/pluginval.app/Contents/MacOS/pluginval --validate-in-process --output-dir "." --validate "${SURGE_FX_OUTPUT_DIR}/VST3/${SURGE_FX_PRODUCT_NAME}.vst3" || exit 1
    )

  add_custom_target(surge-fx-pluginval-au)
  add_dependencies(surge-fx-pluginval-au surge-fx_AU)
  add_dependencies(surge-fx-pluginval-au stage-pluginval)
  add_custom_command(TARGET surge-fx-pluginval-au
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND pluginval/pluginval.app/Contents/MacOS/pluginval --validate-in-process --output-dir "." --validate "${SURGE_FX_OUTPUT_DIR}/AU/${SURGE_FX_PRODUCT_NAME}.component" || exit 1
    )
endif()
# }}}

