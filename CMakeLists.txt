# Surge CMake file
# Exports targets for each of our builds
#
# Current Status - this builds the following assets
#
# Win32   headless
# Win64   headless
# MacOSX  headless
# Linux   headless

cmake_minimum_required(VERSION 3.10)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Build for 10.12")

project(Surge VERSION 1.7.0 LANGUAGES CXX ASM)

#set(CMAKE_BUILD_TYPE Debug)

# Set up external packages
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
find_package(LibMidiFile ${PACKAGE_OPTIONS})

# Things we can build
set( BUILD_HEADLESS true )
set( BUILD_VST3 true )

if( DEFINED ENV{VST2SDK_DIR} )
  message( "-- Configured to build VST2" )
  set( BUILD_VST2 true )
  set( VST2SDK_DIR $ENV{VST2SDK_DIR} )
else()
  set( BUILD_VST2 false )
endif()

if( APPLE )
  set( BUILD_AU true )
else()
  set( BUILD_AU false )
endif()

if( UNIX AND NOT APPLE )
  set( BUILD_LV2 true )
else()
  set( BUILD_LV2 false )
endif()


# CMake Configuration for all platforms
set(CMAKE_CXX_EXTENSIONS false)
set(CMAKE_CXX_STANDARD 17)

# Enumerate the sources into groups
#    SURGE_COMMON_SOURCES
#    SURGE_GUI_SOURCES
#    SURGE_GUI_LIBRARY_SOURCES
#    SURGE_(target)_SOURCES  e.g. SURGE_HEADLESS_SOURCES
#    SURGE_(target)_LIBRARY_SOURCES  e.g. SURGE_VST3_LIBRARY_SOURCES
#    SURGE_(os)_SOURCES

set(SURGE_COMMON_SOURCES
  src/common/dsp/effect/ConditionerEffect.cpp
  src/common/dsp/effect/DistortionEffect.cpp
  src/common/dsp/effect/DualDelayEffect.cpp
  src/common/dsp/effect/Effect.cpp
  src/common/dsp/effect/FreqshiftEffect.cpp
  src/common/dsp/effect/FlangerEffect.cpp   
  src/common/dsp/effect/PhaserEffect.cpp
  src/common/dsp/effect/Reverb1Effect.cpp
  src/common/dsp/effect/Reverb2Effect.cpp
  src/common/dsp/effect/RotarySpeakerEffect.cpp
  src/common/dsp/effect/VocoderEffect.cpp
  src/common/dsp/AdsrEnvelope.cpp
  src/common/dsp/BiquadFilter.cpp
  src/common/dsp/BiquadFilterSSE2.cpp
  src/common/dsp/DspUtilities.cpp
  src/common/dsp/FilterCoefficientMaker.cpp
  src/common/dsp/FMOscillator.cpp
  src/common/dsp/LfoModulationSource.cpp
  src/common/dsp/Oscillator.cpp
  src/common/dsp/QuadFilterChain.cpp
  src/common/dsp/QuadFilterUnit.cpp
  src/common/dsp/SampleAndHoldOscillator.cpp
  src/common/dsp/SurgeSuperOscillator.cpp
  src/common/dsp/SurgeVoice.cpp
  src/common/dsp/VectorizedSvfFilter.cpp
  src/common/dsp/Wavetable.cpp
  src/common/dsp/WavetableOscillator.cpp
  src/common/dsp/WindowOscillator.cpp
  src/common/thread/CriticalSection.cpp
  src/common/util/FpuState.cpp
  src/common/vt_dsp/basic_dsp.cpp
  src/common/vt_dsp/halfratefilter.cpp
  src/common/vt_dsp/lipol.cpp
  src/common/vt_dsp/macspecific.cpp
  src/common/Parameter.cpp
  src/common/precompiled.cpp
  src/common/SurgeError.cpp
  src/common/SurgePatch.cpp
  src/common/SurgeStorage.cpp
  src/common/SurgeSynthesizer.cpp
  src/common/SurgeSynthesizerIO.cpp
  src/common/Tunings.cpp
  src/common/UserDefaults.cpp
  src/common/WavSupport.cpp 
  libs/xml/tinyxml.cpp
  libs/xml/tinyxmlerror.cpp
  libs/xml/tinyxmlparser.cpp
  libs/strnatcmp/strnatcmp.cpp
  libs/filesystem/filesystem.cpp
  )

set(SURGE_GUI_SOURCES
  src/common/gui/CAboutBox.cpp
  src/common/gui/CCursorHidingControl.cpp
  src/common/gui/CDIBitmap.cpp
  src/common/gui/CEffectSettings.cpp
  src/common/gui/CHSwitch2.cpp
  src/common/gui/CLFOGui.cpp
  src/common/gui/CModulationSourceButton.cpp
  src/common/gui/CNumberField.cpp
  src/common/gui/COscillatorDisplay.cpp
  src/common/gui/CPatchBrowser.cpp
  src/common/gui/CScalableBitmap.cpp
  src/common/gui/CSnapshotMenu.cpp
  src/common/gui/CStatusPanel.cpp
  src/common/gui/CSurgeSlider.cpp
  src/common/gui/CSurgeVuMeter.cpp
  src/common/gui/CSwitchControl.cpp
  src/common/gui/PopupEditorDialog.cpp
  src/common/gui/SkinSupport.cpp
  src/common/gui/SurgeBitmaps.cpp
  src/common/gui/SurgeGUIEditor.cpp
  src/common/gui/UIInstrumentation.cpp
  )

set(SURGE_APPLE_SOURCES
  src/mac/UserInteractionsMac.mm
  )

set(SURGE_APPLE_GUI_SOURCES
  src/mac/DisplayInfoMac.mm
  src/mac/RuntimeFontMac.cpp
  src/mac/cocoa_utils.mm
  vstgui.surge/vstgui/vstgui_mac.mm
  vstgui.surge/vstgui/vstgui_uidescription_mac.mm
  )

set(SURGE_VST3_SOURCES
  src/vst3/SurgeVst3Processor.cpp
  src/vst3/surgeentry.cpp
  )

file(GLOB SURGE_VST3_GLOB
  vst3sdk/base/source/*.cpp
  vst3sdk/base/thread/source/*.cpp
  vst3sdk/public.sdk/source/common/*.cpp
  vst3sdk/pluginterfaces/base/*.cpp
  )

set(SURGE_VST3_LIBRARY_SOURCES
  ${SURGE_VST3_GLOB}
  vst3sdk/public.sdk/source/main/pluginfactoryvst3.cpp
  vst3sdk/public.sdk/source/vst/vstguieditor.cpp
  vst3sdk/public.sdk/source/vst/vstinitiids.cpp
  vst3sdk/public.sdk/source/vst/vstnoteexpressiontypes.cpp
  vst3sdk/public.sdk/source/vst/vstsinglecomponenteffect.cpp
  vst3sdk/public.sdk/source/vst/vstaudioeffect.cpp
  vst3sdk/public.sdk/source/vst/vstcomponent.cpp
  vst3sdk/public.sdk/source/vst/vstsinglecomponenteffect.cpp
  vst3sdk/public.sdk/source/vst/vstcomponentbase.cpp
  vst3sdk/public.sdk/source/vst/vstbus.cpp
  vst3sdk/public.sdk/source/vst/vstparameters.cpp
  )

set(SURGE_AU_SOURCES
  src/au/aulayer.cpp
  src/au/aulayer_presets.cpp
  src/au/aulayer_cocoaui.mm
  )

set(SURGE_AU_LIBRARY_SOURCES
  libs/AUPublic/AUBase.cpp
  libs/AUPublic/AUBaseHelper.cpp
  libs/AUPublic/AUBuffer.cpp
  libs/AUPublic/AUDispatch.cpp
  libs/AUPublic/AUEffectBase.cpp
  libs/AUPublic/AUInputElement.cpp
  libs/AUPublic/AUInstrumentBase.cpp
  libs/AUPublic/AUMIDIBase.cpp
  libs/AUPublic/AUMIDIEffectBase.cpp
  libs/AUPublic/AUOutputElement.cpp
  libs/AUPublic/AUPlugInDispatch.cpp
  libs/AUPublic/AUScopeElement.cpp
  libs/AUPublic/ComponentBase.cpp
  libs/AUPublic/MusicDeviceBase.cpp
  libs/AUPublic/SynthElement.cpp
  libs/AUPublic/SynthNote.cpp
  libs/AUPublic/SynthNoteList.cpp
  libs/PublicUtility/CAAUMIDIMap.cpp
  libs/PublicUtility/CAAUMIDIMapManager.cpp
  libs/PublicUtility/CAAudioChannelLayout.cpp
  libs/PublicUtility/CAAudioChannelLayoutObject.cpp
  libs/PublicUtility/CABufferList.cpp
  libs/PublicUtility/CADebugMacros.cpp
  libs/PublicUtility/CADebugPrintf.cpp
  libs/PublicUtility/CADebugger.cpp
  libs/PublicUtility/CAGuard.cpp
  libs/PublicUtility/CAHostTimeBase.cpp
  libs/PublicUtility/CAMutex.cpp
  libs/PublicUtility/CAStreamBasicDescription.cpp
  libs/PublicUtility/CAVectorUnit.cpp
  libs/PublicUtility/CAXException.cpp
  
  vstgui.surge/vstgui/plugin-bindings/plugguieditor.cpp
  )

set( SURGE_VST2_SOURCES
  src/vst2/Vst2PluginInstance.cpp
  )

set( SURGE_VST2_LIBRARY_SOURCES
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/audioeffect.cpp
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/audioeffectx.cpp
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/vstplugmain.cpp
  vstgui.surge/vstgui/plugin-bindings/aeffguieditor.cpp
  )

# Includes and Compiler Flags
set(SURGE_COMMON_INCLUDES
  libs/
  libs/filesystem
  libs/xml
  libs/catch2
  libs/strnatcmp
  src/common
  src/common/dsp
  src/common/thread
  src/common/vt_dsp
  )

set(SURGE_GUI_INCLUDES
  src/common/gui
  vstgui.surge
  libs/nanosvg/src
  )

# OS Specific Variables
set(APPLE_COMPILE_OPTIONS
  -msse2
  "-D_aligned_malloc(x,a)=malloc(x)"
  "-D_aligned_free(x)=free(x)"
  )
set(APPLE_INCLUDE_DIRECTORIES
  src/mac
  )
set(APPLE_COMPILE_DEFINITIONS
  MAC=1
  MAC_COCOA=1
  COCOA=1
  OBJC_OLD_DISPATCH_PROTOTYPES=1
  )
set(APPLE_LINK_LIBRARIES
  "-framework Accelerate"
  "-framework ApplicationServices"
  "-framework AudioUnit"
  "-framework AudioToolbox"
  "-framework Carbon"
  "-framework CoreAudio"
  "-framework CoreAudioKit"
  "-framework CoreServices"
  "-framework CoreText"
  "-framework Cocoa"
  "-framework CoreFoundation"
  "-framework OpenGL"
  "-framework QuartzCore"
  )

set(LINUX_COMPILE_OPTIONS
  -msse2
  "-D_aligned_malloc(x,a)=malloc(x)"
  "-D_aligned_free(x)=free(x)"
  )

set(LINUX_COMPILE_DEFINITIONS
  LINUX=1
  )

set(WINDOWS_COMPILE_DEFINITIONS
  WINDOWS=1
  NOMINMAX=1
  )

set(WINDOWS_COMPILE_OPTIONS
  /arch:SSE2
  "/FI precompiled.h"
  /Zc:alignedNew
  /bigobj
  )


#
# AUDIO UNIT
#
if( BUILD_AU )
  add_library(surge-au
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_APPLE_SOURCES}
    ${SURGE_APPLE_GUI_SOURCES}
    ${SURGE_AU_SOURCES}
    ${SURGE_AU_LIBRARY_SOURCES}
    )

  target_compile_features(surge-au PRIVATE cxx_std_17 )

  target_compile_definitions(surge-au
    PRIVATE
    ${APPLE_COMPILE_DEFINITIONS}
    TARGET_AUDIOUNIT=1
    )

  target_compile_options(surge-au
    PRIVATE
    ${APPLE_COMPILE_OPTIONS}
    )

  target_include_directories(surge-au
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${APPLE_INCLUDE_DIRECTORIES}
    src/au
    libs/AUPublic
    libs/PublicUtility
    )

  target_link_libraries(surge-au
    PRIVATE
    ${APPLE_LINK_LIBRARIES}
    )

  add_custom_target( Surge.component ALL )
  add_dependencies( Surge.component surge-au )
  add_custom_command(
    TARGET Surge.component
    POST_BUILD
    COMMAND echo "Packaging up AU component"
    COMMAND (cd .. && ./scripts/macOS/package-au.sh )
    )

endif()

# Currently this is MAC only VST3
if( BUILD_VST3 )
  add_library(surge-vst3
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_APPLE_SOURCES}
    ${SURGE_APPLE_GUI_SOURCES}
    ${SURGE_VST3_SOURCES}
    ${SURGE_VST3_LIBRARY_SOURCES}
    )

  target_compile_features(surge-vst3 PRIVATE cxx_std_17 )

  target_compile_definitions(surge-vst3
    PRIVATE
    ${APPLE_COMPILE_DEFINITIONS}
    TARGET_VST3=1
    )

  target_compile_options(surge-vst3
    PRIVATE
    ${APPLE_COMPILE_OPTIONS}
    )

  target_include_directories(surge-vst3
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${APPLE_INCLUDE_DIRECTORIES}
    src/vst3
    vst3sdk/
    )

  target_link_libraries(surge-vst3
    PRIVATE
    ${APPLE_LINK_LIBRARIES}
    )

  add_custom_target( Surge.vst3 ALL )
  add_dependencies( Surge.vst3 surge-vst3 )
  add_custom_command(
    TARGET Surge.vst3
    POST_BUILD
    COMMAND echo "Packaging up VST3 component"
    COMMAND (cd .. && ./scripts/macOS/package-vst3.sh )
    )

endif()

if( BUILD_VST2 )
  add_library(surge-vst2
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_APPLE_SOURCES}
    ${SURGE_APPLE_GUI_SOURCES}
    ${SURGE_VST2_SOURCES}
    ${SURGE_VST2_LIBRARY_SOURCES}
    )

  target_compile_features(surge-vst2 PRIVATE cxx_std_17 )

  target_compile_definitions(surge-vst2
    PRIVATE
    ${APPLE_COMPILE_DEFINITIONS}
    TARGET_VST2=1
    VSTGUI_ENABLE_DEPRECATED_METHODS=0
    )

  target_compile_options(surge-vst2
    PRIVATE
    ${APPLE_COMPILE_OPTIONS}
    )

  target_include_directories(surge-vst2
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${APPLE_INCLUDE_DIRECTORIES}
    src/vst2
    ${VST2SDK_DIR}
    )

  target_link_libraries(surge-vst2
    PRIVATE
    ${APPLE_LINK_LIBRARIES}
    )

  add_custom_target( Surge.vst2 ALL )
  add_dependencies( Surge.vst2 surge-vst2 )
  add_custom_command(
    TARGET Surge.vst2
    POST_BUILD
    COMMAND echo "Packaging up VST2 component"
    COMMAND (cd .. && ./scripts/macOS/package-vst.sh )
    )

endif()

if( BUILD_HEADLESS )
  add_executable(surge-headless
    ${SURGE_COMMON_SOURCES}
    ${LIB_MIDIFILE_SOURCES}
    src/headless/main.cpp
    src/headless/UserInteractionsHeadless.cpp
    src/headless/LinkFixesHeadless.cpp
    src/headless/HeadlessUtils.cpp
    src/headless/Player.cpp
    src/headless/UnitTests.cpp
    src/headless/UnitTestUtilities.cpp    
    src/headless/UnitTestsDSP.cpp
    src/headless/UnitTestsIO.cpp
    src/headless/UnitTestsMIDI.cpp
    src/headless/UnitTestsMOD.cpp
    src/headless/UnitTestsTUN.cpp
    )

  target_compile_features(surge-headless PRIVATE cxx_std_17 )

  target_compile_definitions(surge-headless
    PRIVATE
    TARGET_HEADLESS=1
    LIBMIDIFILE=1
    )

  target_include_directories(surge-headless
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${LIB_MIDIFILE_INCLUDES}
    src/headless
    )

  find_package(LibSndFile ${PACKAGE_OPTION})
  if(NOT LIBSNDFILE_FOUND)
    message(WARNING "LibSndFile not installed; building without wav support")
    message(WARNING "You can 'brew install libsndfile' or 'apt-get install libsndfile1-dev'")
  else()
    target_compile_definitions(surge-headless
      PRIVATE
      LIBSNDFILE=1
      )
    target_link_libraries(surge-headless ${LIBSNDFILE_LIBRARIES})
    include_directories(surge-headless ${LIBSNDFILE_INCLUDE_DIRS})
  endif()

  # *NIX
  if (UNIX)
    target_compile_options(surge-headless
      PRIVATE
      -msse2
      "-D_aligned_malloc(x,a)=malloc(x)"
      "-D_aligned_free(x)=free(x)"
      )
  endif()

  # macOS
  if (APPLE)
    target_compile_definitions(surge-headless
      PRIVATE
      MAC=1
      MAC_COCOA=1
      COCOA=1
      OBJC_OLD_DISPATCH_PROTOTYPES=1
      )

    target_include_directories(surge-headless
      PRIVATE
      src/mac
      )

    target_link_libraries(surge-headless
      "-framework ApplicationServices"
      "-framework CoreFoundation"
      )
  endif()

  # Linux
  if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)

    add_custom_target(
      scalable-piggy ALL
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/_ScalablePiggy.S
      )

    add_custom_command(
      OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/_ScalablePiggy.S
      ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/ScalablePiggy.S

      COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/scripts/linux/emit-vector-piggy.py ${CMAKE_CURRENT_SOURCE_DIR}
      )

    target_compile_definitions(surge-headless
      PRIVATE
      LINUX=1
      )

    target_sources(surge-headless
      PRIVATE
      src/linux/ConfigurationXml.S
      src/linux/ScalablePiggy.S
      )

    target_include_directories(surge-headless
      PRIVATE
      src/linux
      )

    target_link_libraries(surge-headless
      stdc++fs
      Threads::Threads
      )
  endif()

  if (WIN32)
    target_compile_definitions(surge-headless
      PRIVATE
      WINDOWS=1
      NOMINMAX=1
      )

    target_compile_options(surge-headless
      PRIVATE
      /arch:SSE2
      "/FI precompiled.h"
      /Zc:alignedNew
      /bigobj
      )

    target_link_libraries(surge-headless winmm.lib)
  endif()
endif()
