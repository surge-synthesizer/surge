# vi:set sw=2 et:
project(surge-gui)

add_library(${PROJECT_NAME}
  AccessibleHelpers.h
  CMakeLists.txt
  ModulationGridConfiguration.h
  RefreshableOverlay.h
  RuntimeFont.cpp
  RuntimeFont.h
  SkinFontLoader.cpp
  SkinImageMaps.h
  SkinSupport.cpp
  SkinSupport.h
  SurgeGUICallbackInterfaces.h
  SurgeGUIEditor.cpp
  SurgeGUIEditor.h
  SurgeGUIEditorHtmlGenerators.cpp
  SurgeGUIEditorInfowindow.cpp
  SurgeGUIEditorOverlays.cpp
  SurgeGUIEditorTags.h
  SurgeGUIEditorValueCallbacks.cpp
  SurgeGUIUtils.cpp
  SurgeGUIUtils.h
  SurgeImage.cpp
  SurgeImage.h
  SurgeImageStore.cpp
  SurgeImageStore.h
  SurgeJUCEHelpers.h
  SurgeJUCELookAndFeel.cpp
  SurgeJUCELookAndFeel.h
  UIInstrumentation.cpp
  UIInstrumentation.h
  overlays/AboutScreen.cpp
  overlays/AboutScreen.h
  overlays/CoveringMessageOverlay.cpp
  overlays/CoveringMessageOverlay.h
  overlays/LuaEditors.cpp
  overlays/LuaEditors.h
  overlays/MSEGEditor.cpp
  overlays/MSEGEditor.h
  overlays/MiniEdit.cpp
  overlays/MiniEdit.h
  overlays/ModulationEditor.cpp
  overlays/ModulationEditor.h
  overlays/OverlayWrapper.cpp
  overlays/OverlayWrapper.h
  overlays/PatchDBViewer.cpp
  overlays/PatchDBViewer.h
  overlays/PatchStoreDialog.cpp
  overlays/PatchStoreDialog.h
  overlays/TuningOverlays.cpp
  overlays/TuningOverlays.h
  overlays/TypeinParamEditor.cpp
  overlays/TypeinParamEditor.h
  overlays/WaveShaperAnalysis.cpp
  overlays/WaveShaperAnalysis.h
  widgets/EffectChooser.cpp
  widgets/EffectChooser.h
  widgets/EffectLabel.h
  widgets/LFOAndStepDisplay.cpp
  widgets/LFOAndStepDisplay.h
  widgets/MainFrame.cpp
  widgets/MainFrame.h
  widgets/MenuCustomComponents.cpp
  widgets/MenuCustomComponents.h
  widgets/MenuForDiscreteParams.cpp
  widgets/MenuForDiscreteParams.h
  widgets/ModulatableControlInterface.h
  widgets/ModulatableSlider.cpp
  widgets/ModulatableSlider.h
  widgets/ModulationSourceButton.cpp
  widgets/ModulationSourceButton.h
  widgets/MultiSwitch.cpp
  widgets/MultiSwitch.h
  widgets/NumberField.cpp
  widgets/NumberField.h
  widgets/OscillatorWaveformDisplay.cpp
  widgets/OscillatorWaveformDisplay.h
  widgets/ParameterInfowindow.cpp
  widgets/ParameterInfowindow.h
  widgets/PatchSelector.cpp
  widgets/PatchSelector.h
  widgets/Switch.cpp
  widgets/Switch.h
  widgets/VerticalLabel.cpp
  widgets/VerticalLabel.h
  widgets/VuMeter.cpp
  widgets/VuMeter.h
  widgets/WaveShaperSelector.cpp
  widgets/WaveShaperSelector.h
  widgets/WidgetBaseMixin.h
  widgets/XMLConfiguredMenus.cpp
  widgets/XMLConfiguredMenus.h
)

if(EXISTS ${SURGE_JUCE_PATH}/modules/juce_gui_basics/accessibility/juce_AccessibilityHandler.h)
  set(SURGE_JUCE_ACCESSIBLE TRUE)
  message(STATUS "Including JUCE Accessibility Support")
else()
  message(STATUS "No JUCE Accessibility Support in this version")
endif()

if(EXISTS ${SURGE_JUCE_PATH}/modules/juce_audio_processors/processors/juce_AudioProcessorEditorHostContext.h)
  set(SURGE_JUCE_HOST_CONTEXT TRUE)
  message(STATUS "Including JUCE Host Context Support")
else()
  message(STATUS "No JUCE Host Context Support in this version")
endif()

if(EXISTS ${SURGE_JUCE_PATH}/modules/juce_audio_processors/utilities/juce_VST3ClientExtensions.h)
  set(SURGE_JUCE_VST3_EXTENSIONS TRUE)
  message(STATUS "Including JUCE VST3 Client Extensions")
else()
  message(STATUS "No JUCE VST3 Client Extensions")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
  SURGE_JUCE_ACCESSIBLE=$<BOOL:${SURGE_JUCE_ACCESSIBLE}>
  SURGE_JUCE_HOST_CONTEXT=$<BOOL:${SURGE_JUCE_HOST_CONTEXT}>
  SURGE_JUCE_VST3_EXTENSIONS=$<BOOL:${SURGE_JUCE_VST3_EXTENSIONS}>

  $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  juce::juce_graphics
  surge-juce
  surge::surge-common
  surge::surge-xt # FIXME
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
