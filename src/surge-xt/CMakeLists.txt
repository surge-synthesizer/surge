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
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Surge XT would like to use your microphone for Audio Input"

  BLUETOOTH_PERMISSION_ENABLED TRUE
  BLUETOOTH_PERMISSION_TEXT "Surge XT would like to use BlueTooth"

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

target_sources(${PROJECT_NAME} PRIVATE
  SurgeSynthEditor.cpp
  SurgeSynthEditor.h
  SurgeSynthProcessor.cpp
  SurgeSynthProcessor.h

  SurgeCLAPPresetDiscovery.cpp

  util/LockFreeStack.h

  gui/AccessibleHelpers.h
  gui/ModulationGridConfiguration.h
  gui/RefreshableOverlay.h
  gui/RuntimeFont.cpp
  gui/RuntimeFont.h
  gui/SkinFontLoader.cpp
  gui/SkinImageMaps.h
  gui/SkinSupport.cpp
  gui/SkinSupport.h
  gui/SurgeGUICallbackInterfaces.h
  gui/SurgeGUIEditor.cpp
  gui/SurgeGUIEditor.h
  gui/SurgeGUIEditorHtmlGenerators.cpp
  gui/SurgeGUIEditorInfowindow.cpp
  gui/SurgeGUIEditorOverlays.cpp
  gui/SurgeGUIEditorTags.h
  gui/SurgeGUIEditorValueCallbacks.cpp
  gui/SurgeGUIEditorMenuStructures.cpp
  gui/SurgeGUIUtils.cpp
  gui/SurgeGUIUtils.h
  gui/SurgeImage.cpp
  gui/SurgeImage.h
  gui/SurgeImageStore.cpp
  gui/SurgeImageStore.h
  gui/SurgeJUCEHelpers.h
  gui/SurgeJUCELookAndFeel.cpp
  gui/SurgeJUCELookAndFeel.h
  gui/UndoManager.cpp
  gui/overlays/AboutScreen.cpp
  gui/overlays/AboutScreen.h
  gui/overlays/FilterAnalysis.cpp
  gui/overlays/FilterAnalysis.h
  gui/overlays/KeyBindingsOverlay.h
  gui/overlays/KeyBindingsOverlay.cpp
  gui/overlays/LuaEditors.cpp
  gui/overlays/LuaEditors.h
  gui/overlays/MSEGEditor.cpp
  gui/overlays/MSEGEditor.h
  gui/overlays/MiniEdit.cpp
  gui/overlays/MiniEdit.h
  gui/overlays/Alert.cpp
  gui/overlays/Alert.h
  gui/overlays/OverlayUtils.h
  gui/overlays/OverlayUtils.cpp
  gui/overlays/ModulationEditor.cpp
  gui/overlays/ModulationEditor.h
  gui/overlays/Oscilloscope.cpp
  gui/overlays/Oscilloscope.h
  gui/overlays/OverlayWrapper.cpp
  gui/overlays/OverlayWrapper.h
  gui/overlays/PatchDBViewer.cpp
  gui/overlays/PatchDBViewer.h
  gui/overlays/PatchStoreDialog.cpp
  gui/overlays/PatchStoreDialog.h
  gui/overlays/TuningOverlays.cpp
  gui/overlays/TuningOverlays.h
  gui/overlays/TypeinParamEditor.cpp
  gui/overlays/TypeinParamEditor.h
  gui/overlays/WaveShaperAnalysis.cpp
  gui/overlays/WaveShaperAnalysis.h
  gui/overlays/OpenSoundControlSettings.cpp
  gui/widgets/EffectChooser.cpp
  gui/widgets/EffectChooser.h
  gui/widgets/EffectLabel.h
  gui/widgets/LFOAndStepDisplay.cpp
  gui/widgets/LFOAndStepDisplay.h
  gui/widgets/MainFrame.cpp
  gui/widgets/MainFrame.h
  gui/widgets/MenuCustomComponents.cpp
  gui/widgets/MenuCustomComponents.h
  gui/widgets/MenuForDiscreteParams.cpp
  gui/widgets/MenuForDiscreteParams.h
  gui/widgets/ModulatableControlInterface.h
  gui/widgets/ModulatableSlider.cpp
  gui/widgets/ModulatableSlider.h
  gui/widgets/ModulationSourceButton.cpp
  gui/widgets/ModulationSourceButton.h
  gui/widgets/MultiSwitch.cpp
  gui/widgets/MultiSwitch.h
  gui/widgets/NumberField.cpp
  gui/widgets/NumberField.h
  gui/widgets/OscillatorWaveformDisplay.cpp
  gui/widgets/OscillatorWaveformDisplay.h
  gui/widgets/ParameterInfowindow.cpp
  gui/widgets/ParameterInfowindow.h
  gui/widgets/PatchSelector.cpp
  gui/widgets/PatchSelector.h
  gui/widgets/Switch.cpp
  gui/widgets/Switch.h
  gui/widgets/SurgeTextButton.cpp
  gui/widgets/TypeAheadTextEditor.cpp
  gui/widgets/VerticalLabel.cpp
  gui/widgets/VerticalLabel.h
  gui/widgets/VuMeter.cpp
  gui/widgets/VuMeter.h
  gui/widgets/WaveShaperSelector.cpp
  gui/widgets/WaveShaperSelector.h
  gui/widgets/WidgetBaseMixin.h
  gui/widgets/XMLConfiguredMenus.cpp
  gui/widgets/XMLConfiguredMenus.h

  osc/OpenSoundControl.cpp
  osc/OpenSoundControl.h
)

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
  message(STATUS "macOS Standalone includes CLI")

  add_custom_command(
    TARGET ${PROJECT_NAME}_Standalone
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Relocating cli executable into Standalone"
    COMMAND ${CMAKE_COMMAND} -E copy "${cliexe}" "${saname}/Surge XT.app/Contents/MacOS"
  )
endif()

surge_juce_package(${PROJECT_NAME} "Surge XT")

if (${SURGE_BUILD_TESTRUNNER})
  add_subdirectory(xt-tests)
endif()
