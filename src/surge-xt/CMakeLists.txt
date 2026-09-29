# vi:set sw=2 et:
project(surge-xt VERSION ${CMAKE_PROJECT_VERSION})

file(GLOB SURGE_XT_JUCE_RESOURCES_GLOB
  ${CMAKE_SOURCE_DIR}/resources/classic-skin-svgs/*.svg
  ${CMAKE_SOURCE_DIR}/resources/fonts/Lato*ttf
  ${CMAKE_SOURCE_DIR}/resources/fonts/IndieFlower.ttf
  ${CMAKE_SOURCE_DIR}/resources/fonts/FiraMono-Regular.ttf
  ${CMAKE_SOURCE_DIR}/resources/surge-xt/memory-skin.xml
  ${CMAKE_SOURCE_DIR}/resources/surge-xt/wtfile_icon.svg
  ${CMAKE_SOURCE_DIR}/resources/surge-xt/wtscript_icon.svg
)

juce_add_binary_data(surge-xt-binary
  NAMESPACE SurgeXTBinary
  HEADER_NAME SurgeXTBinary.h
  SOURCES ${SURGE_XT_JUCE_RESOURCES_GLOB}
)

if(WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  set(SURGE_XT_PRODUCT_NAME "Surge XT (32-bit)")
else()
  set(SURGE_XT_PRODUCT_NAME "Surge XT")
endif()

juce_add_plugin(${PROJECT_NAME}
  PRODUCT_NAME ${SURGE_XT_PRODUCT_NAME}
  COMPANY_NAME "Surge Synth Team"
  COMPANY_WEBSITE "https://surge-synth-team.org/"
  BUNDLE_ID "org.surge-synth-team.surge-xt"
  PLUGIN_MANUFACTURER_CODE VmbA
  PLUGIN_CODE SgXT

  ICON_BIG "${CMAKE_SOURCE_DIR}/resources/surge-xt/SurgeLogo.png"

  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Surge XT would like to use your microphone for audio input"

  BLUETOOTH_PERMISSION_ENABLED TRUE
  BLUETOOTH_PERMISSION_TEXT "Surge XT would like to use Bluetooth"

  COPY_PLUGIN_AFTER_BUILD ${SURGE_COPY_AFTER_BUILD}

  VST3_CATEGORIES Instrument Synth
  VST3_AUTO_MANIFEST FALSE

  VST2_CATEGORY kPlugCategSynth

  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  AU_SANDBOX_SAFE TRUE

  # JUCE7 made a different choice than community branch
  LV2_URI https://surge-synthesizer.github.io/lv2/surge-xt
  LV2URI https://surge-synthesizer.github.io/lv2/surge-xt

  LV2_SHARED_LIBRARY_NAME SurgeXT

  FORMATS ${SURGE_JUCE_FORMATS}
)

if(SURGE_BUILD_CLAP)
  clap_juce_extensions_plugin(TARGET surge-xt
    CLAP_ID "org.surge-synth-team.surge-xt"

    CLAP_SUPPORTS_CUSTOM_FACTORY 1
    CLAP_FEATURES "instrument" "synthesizer" "stereo" "free and open source")
endif()

if(JUCE_ASIO_SUPPORT)
  target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_ASIO=1)
endif()

if (SURGE_EXPOSE_PRESETS)
  message(STATUS "Building with JUCE Preset API (getProgramName etc) enabled")
  target_compile_definitions(${PROJECT_NAME} PUBLIC SURGE_EXPOSE_PRESETS=1)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)

if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE
    gui/AccessibleAnnouncementWin.cpp
    gui/AccessibleAnnouncementWin.h
  )
endif()

if(NOT EXISTS ${SURGE_JUCE_PATH}/modules/juce_gui_basics/accessibility/juce_AccessibilityHandler.h)
  message(FATAL_ERROR "You must build against at least JUCE 6.1 with accessibility support!")
endif()

if(EXISTS ${SURGE_JUCE_PATH}/modules/juce_audio_processors/processors/juce_AudioProcessorEditorHostContext.h)
  set(SURGE_JUCE_HOST_CONTEXT TRUE)
  message(STATUS "Including JUCE VST3 host-side context menu support...")
else()
  message(STATUS "No JUCE VST3 host-side context menu support in this version!")
endif()

if(EXISTS ${SURGE_JUCE_PATH}/modules/juce_audio_processors/utilities/juce_VST3ClientExtensions.h)
  set(SURGE_JUCE_VST3_EXTENSIONS TRUE)
  message(STATUS "Including JUCE VST3 client extensions...")
else()
  message(STATUS "No JUCE VST3 client extensions in this version!")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
  $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge-common
  surge-platform
  surge-juce
  juce::juce_graphics
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_plugin_client
  juce::juce_osc
  juce::juce_gui_basics
  juce::juce_dsp
  surge-xt-binary
  sst-filters-extras
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/gui
  ${CMAKE_CURRENT_SOURCE_DIR}/osc
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(cli)

if(APPLE)
  add_dependencies(${PROJECT_NAME}_Standalone ${PROJECT_NAME}-cli)
  get_property(cliname TARGET ${PROJECT_NAME}-cli PROPERTY RUNTIME_OUTPUT_DIRECTORY)
  get_property(saname TARGET ${PROJECT_NAME}_Standalone PROPERTY RUNTIME_OUTPUT_DIRECTORY)
  set(cliexe ${cliname}/${PROJECT_NAME}-cli)
  message(STATUS "macOS standalone includes CLI")

  add_custom_command(
    TARGET ${PROJECT_NAME}_Standalone
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Relocating CLI executable into standalone"
    COMMAND ${CMAKE_COMMAND} -E copy "${cliexe}" "${saname}/Surge XT.app/Contents/MacOS"
  )
endif()

surge_juce_package(${PROJECT_NAME} "Surge XT")

if (${SURGE_BUILD_TESTRUNNER})
  add_subdirectory(xt-tests)
endif()
