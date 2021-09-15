# vi:set sw=2 et:
project(surge-xt VERSION ${CMAKE_PROJECT_VERSION})

file(GLOB SURGE_XT_JUCE_RESOURCES_GLOB
  ${CMAKE_SOURCE_DIR}/resources/classic-skin-svgs/*.svg
  ${CMAKE_SOURCE_DIR}/resources/fonts/Lato*ttf
  ${CMAKE_SOURCE_DIR}/resources/fonts/IndieFlower.ttf
  ${CMAKE_SOURCE_DIR}/resources/fonts/FiraMono-Regular.ttf
  ${CMAKE_SOURCE_DIR}/resources/surge-xt/memory-skin.xml
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

  COPY_PLUGIN_AFTER_BUILD ${SURGE_COPY_AFTER_BUILD}

  VST3_CATEGORIES Instrument Synth
  VST2_CATEGORY kPlugCategSynth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice

  LV2_URI https://surge-synthesizer.github.io/lv2/surge-xt
  LV2_SHARED_LIBRARY_NAME SurgeXT

  FORMATS ${SURGE_JUCE_FORMATS}
)

if(JUCE_ASIO_SUPPORT)
  target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_ASIO=1)
endif()

target_sources(${PROJECT_NAME} PRIVATE
  LockFreeStack.h
  SurgeSynthEditor.cpp
  SurgeSynthEditor.h
  SurgeSynthProcessor.cpp
  SurgeSynthProcessor.h
)

foreach(format ${SURGE_JUCE_FORMATS})
  target_sources(${PROJECT_NAME}_${format} PRIVATE SurgeSynth${format}Extensions.cpp)
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge::surge-common
  surge::surge-gui
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_plugin_client
  PUBLIC surge-xt-binary # FIXME
)

# FIXME: surge-gui and surge-xt depend on each other :(
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

surge_juce_package(${PROJECT_NAME} "Surge XT")
