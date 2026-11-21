# vi:set sw=2 et:
project(surge-fx VERSION ${CMAKE_PROJECT_VERSION})

add_subdirectory(${CMAKE_SOURCE_DIR}/libs/sst/sst-jucegui
                 ${CMAKE_CURRENT_BINARY_DIR}/sst-jucegui EXCLUDE_FROM_ALL)

if(WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  set(SURGE_FX_PRODUCT_NAME "Surge XT Effects (32-bit)")
else()
  set(SURGE_FX_PRODUCT_NAME "Surge XT Effects")
endif()

juce_add_binary_data(surge-fx-binary
  SOURCES
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/SurgeLogoOnlyBlack.svg
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/classes.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/airwindows.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/audio_input.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/bonsai.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/chorus.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/chow.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/combulator.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/conditioner.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/convolution.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/delay.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/distortion.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/ensemble.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/exciter.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/flanger.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/floaty_delay.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/frequency_shifter.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/graphic_eq.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/mid_side.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/neuron.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/nimbus.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/off.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/parametric_eq.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/phaser.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/resonator.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/reverb1.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/reverb2.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/ring_modulator.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/rotary_speaker.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/spring_reverb.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/tape.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/treemonster.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/vocoder.json
    ${CMAKE_SOURCE_DIR}/resources/surge-fx/layouts/waveshaper.json
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
  MICROPHONE_PERMISSION_TEXT "Surge XT Effects would like to use your microphone for audio input"

  BLUETOOTH_PERMISSION_ENABLED TRUE
  BLUETOOTH_PERMISSION_TEXT "Surge XT Effects would like to use Bluetooth"

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
  SurgeFXEditor.h
  SurgeFXProcessor.cpp
  SurgeFXProcessor.h
  SurgeLookAndFeel.h
  FXWidgets.h
  EffectPanel.h
  FXOpenSoundControl.cpp
  FXOpenSoundControl.h
  )

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge::surge-common
  surge-fx-binary
  surge-juce
  surge-platform
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
