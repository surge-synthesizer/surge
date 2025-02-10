# vi:set sw=2 et:
project(surge-fx VERSION ${CMAKE_PROJECT_VERSION})

if(WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  set(SURGE_FX_PRODUCT_NAME "Surge XT Effects (32-bit)")
else()
  set(SURGE_FX_PRODUCT_NAME "Surge XT Effects")
endif()

file(GLOB SURGE_FX_BANK_RESOURCES_GLOB
  ${CMAKE_SOURCE_DIR}/resources/surge-fx/*.svg
  ${CMAKE_SOURCE_DIR}/resources/surge-fx/icons/*.svg
  )

juce_add_binary_data(surge-fx-binary
  SOURCES ${SURGE_FX_BANK_RESOURCES_GLOB}
  )

juce_add_plugin(${PROJECT_NAME}
  PRODUCT_NAME ${SURGE_FX_PRODUCT_NAME}
  COMPANY_NAME "Surge Synth Team"
  COMPANY_WEBSITE "https://surge-synth-team.org/"
  BUNDLE_ID "org.surge-synth-team.surge-xt-fx"
  PLUGIN_MANUFACTURER_CODE VmbA
  PLUGIN_CODE SFXT

  ICON_BIG "${CMAKE_SOURCE_DIR}/resources/surge-fx/SurgeFXLogo.png"

  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Surge XT Effects would like to use your microphone for Audio Input"

  BLUETOOTH_PERMISSION_ENABLED TRUE
  BLUETOOTH_PERMISSION_TEXT "Surge XT Effects would like to use BlueTooth"


  COPY_PLUGIN_AFTER_BUILD ${SURGE_COPY_AFTER_BUILD}

  VST3_CATEGORIES Fx
  VST3_AUTO_MANIFEST FALSE

  VST2_CATEGORY kPlugCategEffect

  AU_MAIN_TYPE kAudioUnitType_Effect
  AU_SANDBOX_SAFE TRUE


  LV2_URI https://surge-synthesizer.github.io/lv2/surge-xt-fx
  LV2_SHARED_LIBRARY_NAME SurgeXT_FX

  FORMATS ${SURGE_JUCE_FORMATS}
  )

target_sources(${PROJECT_NAME} PRIVATE
  SurgeFXEditor.cpp
  SurgeFXProcessor.cpp
  ParameterPanel.cpp
  FXOpenSoundControl.cpp
  )

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge::surge-common
  surge-fx-binary
  surge-juce
  sst-jucegui
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_osc
  )

if(SURGE_BUILD_CLAP)
  clap_juce_extensions_plugin(TARGET surge-fx
    CLAP_ID "org.surge-synth-team.surge-xt-fx"
    CLAP_FEATURES
      "audio-effect"
      "filter"
      "phaser"
      "rotary speaker"
      "equalizer"
      "granular"
      "frequency-shifter"
      "distortion"
      "flanger"
      "chorus"
      "delay"
      "reverb"
      "multi-effects"
      "stereo"
      "free and open source")
endif()

surge_juce_package(${PROJECT_NAME} "Surge XT Effects")
