# Surge CMake file
# Exports targets for each of our builds
#
# Current Status - this builds the whole shooting match
# See issue 1206 for our working list to get this finished
#

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE STRING "Build for 10.9")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(NOT BUILD_TESTING)
  set(BUILD_TESTING "False" CACHE STRING "" FORCE)
endif()

project(Surge VERSION 1.9.0.0 LANGUAGES CXX ASM)

set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
  set(CMAKE_CXX_STANDARD 14)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  # Any Clang or any GCC
  add_compile_options(
    -Wno-multichar
    # Targetting Windows with GCC/Clang is experimental
    $<$<NOT:$<BOOL:${WIN32}>>:-Werror>

    # PE/COFF doesn't support visibility
    $<$<NOT:$<BOOL:${WIN32}>>:-fvisibility=hidden>
    # Inlines visibility is only relevant with C++
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<COMPILE_LANGUAGE:CXX>>:-fvisibility-inlines-hidden>
  )
  # Enable SSE2 on x86-32 only. It's implied on x86-64 and N/A elsewhere.
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("#ifndef __i386__
    #error
    #endif
    int main() {}" SURGE_ARCH_I386)
    if (SURGE_ARCH_I386)
      add_compile_options(-msse2 -mfpmath=sse)
    endif()
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Any Clang
    add_compile_options(
      -Wno-deprecated-declarations
      -Werror=inconsistent-missing-override
      -Werror=logical-op-parentheses
      -Werror=dynamic-class-memaccess
      -Werror=undefined-bool-conversion
      -Werror=bitwise-op-parentheses
      -Werror=pointer-bool-conversion
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "^AppleClang$")
      # Apple Clang only
      add_compile_options(
        -fasm-blocks
      )
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^GNU$")
    # GCC only
    add_compile_options(
        -Wformat-truncation=0 # squelch warning about snprintf truncating strings (see PR #3977)
        -Wno-free-nonheap-object # https://github.com/surge-synthesizer/surge/issues/4251
    )
  endif()
endif()

if(MSVC)
  # MSVC and pretenders
  add_compile_options(
    -WX       # treat all warnings as errors

    # MSVC-only warnings, Clang-cl silently ignores these
    /wd4244   # float to double
    /wd4305   # truncation of variable
    /wd4267   # int and size_t
    /wd4018   # signed unsigned mismatch
    /wd4388   # signed unsigned mismatch in comparison
    /wd4477   # blah blah wide strings after we made it work
    /wd4065   # standalone default in a switch with no case
    /wd4702   # unreachable code. I generally do if( a ) return foo else return bar; return nullptr so don't warn on that

    # Clang-cl-only warnings
    $<$<CXX_COMPILER_ID:Clang>:-Wno-microsoft-exception-spec>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-pragma-pack>

    /Zc:alignedNew
    /bigobj

    # Build with Multiple Processes (Clang-cl builds use Ninja instead)
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
  )
endif()

message( STATUS "CMake Version is ${CMAKE_VERSION}" )
message( STATUS "Compiler Version is ${CMAKE_CXX_COMPILER_VERSION}" )
if( ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
  message( STATUS "Building in 32 bit configuration" )
else()
  message( STATUS "Building in 64 bit configuration" )
endif()

set(SURGE_PRODUCT_DIR ${CMAKE_BINARY_DIR}/surge_products)
file(MAKE_DIRECTORY ${SURGE_PRODUCT_DIR})

include(cmake/stage-extra-content.cmake)

add_library(surge-shared)
add_library(surge-tests INTERFACE)

add_subdirectory(libs/airwindows)
add_subdirectory(libs/catch2)
add_subdirectory(libs/eurorack)
add_subdirectory(libs/filesystem)
add_subdirectory(libs/tinyxml)
add_subdirectory(libs/escape-from-vstgui)
add_subdirectory(libs/oddsound-mts)
add_subdirectory(libs/libsamplerate EXCLUDE_FROM_ALL)

target_link_libraries(surge-shared PUBLIC
  surge::airwindows
  surge::eurorack
  surge::filesystem
  surge::tinyxml
  surge::oddsound-mts
  samplerate
)

# We want to run this once alas, since JUCE needs it even though it is a byproduct of a phase to build the
# JUCE Cmake file list.
set(SURGE_COMPILER_INFO "${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
execute_process(
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -D PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}  -D PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
                           -D SURGESRC=${CMAKE_SOURCE_DIR} -D SURGEBLD=${CMAKE_BINARY_DIR}
                           -D AZURE_PIPELINE=${AZURE_PIPELINE}
                           -D WIN32=${WIN32}
                           -D CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                           -D SURGE_COMPILER_INFO="${SURGE_COMPILER_INFO}"
                           -P ${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake
                           )
add_custom_target( git-info BYPRODUCTS ${CMAKE_BINARY_DIR}/geninclude/version.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -D PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}  -D PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
                           -D SURGESRC=${CMAKE_SOURCE_DIR} -D SURGEBLD=${CMAKE_BINARY_DIR}
                           -D AZURE_PIPELINE=${AZURE_PIPELINE}
                           -D WIN32=${WIN32}
                           -D CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                           -D SURGE_COMPILER_INFO="${SURGE_COMPILER_INFO}"
                           -P ${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake
                           )
add_dependencies( surge-shared git-info )

# Set up external packages
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
find_package(LibMidiFile ${PACKAGE_OPTIONS})

if( BUILD_SURGE_JUCE_PLUGINS )
  set( BUILD_SURGE_EFFECTS_BANK TRUE )
  set( BUILD_SURGE_XT TRUE )
endif()

if( BUILD_ONLY_SURGE_XT)
  set( BUILD_SURGE_JUCE_PLUGINS TRUE )
  set( BUILD_SURGE_XT TRUE )
else()
  # Things we can build
  set( BUILD_HEADLESS true )
  set( BUILD_VST3 true )

  if( DEFINED ARM_NATIVE )
     include( cmake/arm-${ARM_NATIVE}.cmake )
  endif()

  if( DEFINED ENV{VST2SDK_DIR} )
    set( BUILD_VST2 true )
    file( TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" VST2SDK_DIR )
    if( AZURE_PIPELINE )
      message( ERROR " Building VST2 using " ${VST2SDK_DIR} " - VST2 pipeline builds not allowed" )
    else()
      message( WARNING " Building VST2 using " ${VST2SDK_DIR} " - this is an unsupported configuration" )
    endif()
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
  elseif( APPLE )
    set( BUILD_LV2 false )  # Status as of Apr 26. This CMake will build 'something' in products/Surge.lv2 but :shrug:
  else()
    set( BUILD_LV2 false )
  endif()
endif()

# Enumerate the sources into groups
#    SURGE_SHARED_SOURCES - these are used to make the synth-free ui-free dsp and param lib
#    SURGE_SYNTH_SOURCES - these are the synth classes which are target type dependant
#    SURGE_GUI_SOURCES
#    SURGE_GUI_LIBRARY_SOURCES
#    SURGE_(target)_SOURCES  e.g. SURGE_HEADLESS_SOURCES
#    SURGE_(target)_LIBRARY_SOURCES  e.g. SURGE_VST3_LIBRARY_SOURCES
#    SURGE_(os)_SOURCES

set(SURGE_SHARED_SOURCES
  src/common/dsp/effect/BBDEnsembleEffect.cpp
  src/common/dsp/effect/ChorusEffectImpl.h
  src/common/dsp/effect/CombulatorEffect.cpp
  src/common/dsp/effect/ConditionerEffect.cpp
  src/common/dsp/effect/DistortionEffect.cpp
  src/common/dsp/effect/DualDelayEffect.cpp
  src/common/dsp/effect/Effect.cpp
  src/common/dsp/effect/Eq3BandEffect.cpp
  src/common/dsp/effect/FreqshiftEffect.cpp
  src/common/dsp/effect/FlangerEffect.cpp
  src/common/dsp/effect/GEQ11Effect.cpp
  src/common/dsp/effect/NimbusEffect.cpp
  src/common/dsp/effect/PhaserEffect.cpp
  src/common/dsp/effect/ResonatorEffect.cpp
  src/common/dsp/effect/Reverb1Effect.cpp
  src/common/dsp/effect/Reverb2Effect.cpp
  src/common/dsp/effect/RingModulatorEffect.cpp
  src/common/dsp/effect/RotarySpeakerEffect.cpp
  src/common/dsp/effect/TreemonsterEffect.cpp
  src/common/dsp/effect/VocoderEffect.cpp
  src/common/dsp/effect/airwindows/AirWindowsEffect.cpp
  src/common/dsp/effect/airwindows/AirWindowsEffect.h
  src/common/dsp/effect/chowdsp/CHOWEffect.cpp
  src/common/dsp/effect/chowdsp/ExciterEffect.cpp
  src/common/dsp/effect/chowdsp/NeuronEffect.cpp
  src/common/dsp/effect/chowdsp/TapeEffect.cpp
  src/common/dsp/effect/chowdsp/bbd_utils/BBDDelayLine.cpp
  src/common/dsp/effect/chowdsp/exciter/LevelDetector.cpp
  src/common/dsp/effect/chowdsp/shared/DelayLine.cpp
  src/common/dsp/effect/chowdsp/tape/ChewProcessor.cpp
  src/common/dsp/effect/chowdsp/tape/DegradeProcessor.cpp
  src/common/dsp/effect/chowdsp/tape/HysteresisProcessing.cpp
  src/common/dsp/effect/chowdsp/tape/HysteresisProcessor.cpp
  src/common/dsp/effect/chowdsp/tape/LossFilter.cpp
  src/common/dsp/effect/chowdsp/tape/ToneControl.cpp
  src/common/dsp/filters/VintageLadders.cpp
  src/common/dsp/filters/Obxd.cpp
  src/common/dsp/filters/K35.cpp
  src/common/dsp/filters/DiodeLadder.cpp
  src/common/dsp/filters/NonlinearFeedback.cpp
  src/common/dsp/filters/NonlinearStates.cpp
  src/common/dsp/AliasOscillator.cpp
  src/common/dsp/AudioInputOscillator.cpp
  src/common/dsp/BiquadFilter.cpp
  src/common/dsp/BiquadFilterSSE2.cpp
  src/common/dsp/ClassicOscillator.cpp
  src/common/dsp/DspUtilities.cpp
  src/common/dsp/FilterCoefficientMaker.cpp
  src/common/dsp/FM2Oscillator.cpp
  src/common/dsp/FM3Oscillator.cpp
  src/common/dsp/LanczosResampler.cpp
  src/common/dsp/LfoModulationSource.cpp
  src/common/dsp/ModernOscillator.cpp
  src/common/dsp/MSEGModulationHelper.cpp
  src/common/dsp/Oscillator.cpp
  src/common/dsp/QuadFilterChain.cpp
  src/common/dsp/QuadFilterUnit.cpp
  src/common/dsp/SampleAndHoldOscillator.cpp
  src/common/dsp/SineOscillator.cpp
  src/common/dsp/StringOscillator.cpp
  src/common/dsp/SurgeVoice.cpp
  src/common/dsp/TwistOscillator.cpp
  src/common/dsp/VectorizedSvfFilter.cpp
  src/common/dsp/Wavetable.cpp
  src/common/dsp/WavetableOscillator.cpp
  src/common/dsp/WindowOscillator.cpp
  src/common/util/FpuState.cpp
  src/common/vt_dsp/basic_dsp.cpp
  src/common/vt_dsp/halfratefilter.cpp
  src/common/vt_dsp/lipol.cpp
  src/common/vt_dsp/macspecific.cpp
  src/common/DebugHelpers.cpp
  # src/common/DeferredAssetLoader.cpp
  src/common/Parameter.cpp
  src/common/SurgePatch.cpp
  src/common/SurgeStorage.cpp
  src/common/UserDefaults.cpp
  src/common/WavSupport.cpp

  src/common/SkinModel.cpp
  src/common/SkinModelImpl.cpp
  src/common/SkinColors.cpp

  libs/strnatcmp/strnatcmp.cpp
  src/common/dsp/FastMath.h
  src/common/dsp/AdsrEnvelope.h
  src/common/dsp/OscillatorBase.h
  src/common/dsp/OscillatorCommonFunctions.h
  src/common/dsp/SurgeVoiceState.h
  )

set(SURGE_SYNTH_SOURCES
  src/common/SurgeSynthesizer.cpp
  src/common/SurgeSynthesizerIDManagement.cpp
  src/common/SurgeSynthesizerIO.cpp
  )

set(SURGE_GENERATED_SOURCES
  ${CMAKE_BINARY_DIR}/geninclude/version.cpp
  )

set(SURGE_GUI_SOURCES
  src/common/gui/CAboutBox.cpp
  src/common/gui/CDIBitmap.cpp
  src/common/gui/CEffectLabel.h
  src/common/gui/CEffectSettings.cpp
  src/common/gui/CHSwitch2.cpp
  src/common/gui/CLFOGui.cpp
  src/common/gui/CMenuAsSlider.cpp
  src/common/gui/CModulationSourceButton.cpp
  src/common/gui/CNumberField.cpp
  src/common/gui/COscillatorDisplay.cpp
  src/common/gui/CParameterTooltip.h
  src/common/gui/CPatchBrowser.cpp
  src/common/gui/CScalableBitmap.cpp
  src/common/gui/CSnapshotMenu.cpp
  src/common/gui/CSurgeHyperlink.cpp
  src/common/gui/CSurgeSlider.cpp
  src/common/gui/CSurgeVuMeter.cpp
  src/common/gui/CSwitchControl.cpp
  src/common/gui/CTextButtonWithHover.cpp
  src/common/gui/CursorControlGuard.cpp
  src/common/gui/CVerticalLabel.cpp
  src/common/gui/guihelpers.cpp
  src/common/ModulatorPresetManager.cpp
  src/common/gui/MSEGEditor.cpp
  src/common/gui/RuntimeFont.cpp
  src/common/gui/SkinFontLoader.cpp
  src/common/gui/SkinImageMaps.h
  src/common/gui/SkinSupport.cpp
  src/common/gui/SurgeBitmaps.cpp
  src/common/gui/SurgeGUIEditor.cpp
  src/common/gui/SurgeGUIEditorHtmlGenerators.cpp
  src/common/gui/UIInstrumentation.cpp)

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

set(SURGE_VST2_SOURCES
  src/vst2/Vst2PluginInstance.cpp
  )

set(SURGE_VST2_LIBRARY_SOURCES
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/audioeffect.cpp
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/audioeffectx.cpp
  ${VST2SDK_DIR}/public.sdk/source/vst2.x/vstplugmain.cpp
  vstgui.surge/vstgui/plugin-bindings/aeffguieditor.cpp
  )

set(SURGE_LV2_SOURCES
  src/lv2/SurgeLv2Export.cpp
  src/lv2/SurgeLv2Ui.cpp
  src/lv2/SurgeLv2Util.cpp
  src/lv2/SurgeLv2Vstgui.cpp
  src/lv2/SurgeLv2Wrapper.cpp
  vstgui.surge/vstgui/plugin-bindings/plugguieditor.cpp
  )

# Includes and Compiler Flags
set(SURGE_COMMON_INCLUDES
  libs/
  libs/strnatcmp
  libs/tuning-library/include
  src/common
  src/common/dsp
  src/common/thread
  src/common/vt_dsp
  )

set(SURGE_GUI_INCLUDES
  src/common/gui
  )

set(SURGE_PREJUCE_GUI_INCLUDES
        vstgui.surge
        libs/nanosvg/src
        )
### OS SPECIFIC SECTION
if( APPLE )
  set(SURGE_OS_SOURCES
      src/mac/objc_utils.mm
      )

  set(SURGE_OS_GUI_SOURCES
    src/mac/DisplayInfoMac.mm
    src/mac/RuntimeFontMac.cpp
    src/mac/UserInteractionsMac.mm
    vstgui.surge/vstgui/vstgui_mac.mm
    vstgui.surge/vstgui/vstgui_uidescription_mac.mm
    )

  set(OS_INCLUDE_DIRECTORIES
    src/mac
    libs/simde
    )
  set(OS_COMPILE_DEFINITIONS
    MAC=1
    MAC_COCOA=1
    COCOA=1
    OBJC_OLD_DISPATCH_PROTOTYPES=1
    )
  set(OS_LINK_LIBRARIES_NOGUI
      "-framework CoreServices"
      "-framework CoreFoundation"
      "-framework Foundation"
    )
  set(OS_LINK_LIBRARIES
    ${OS_LINK_LIBRARIES_NOGUI}
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

elseif( UNIX AND NOT APPLE )
  file(GLOB PIGGY_INPUTS ${CMAKE_SOURCE_DIR}/assets/SurgeClassic/exported/*svg)
  add_custom_command( OUTPUT ${CMAKE_BINARY_DIR}/lintemp/ScalablePiggy.S
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${PIGGY_INPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lintemp
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/linux/emit-vector-piggy ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/lintemp
    )

  file(GLOB LINUX_VSTGUI_GLOB vstgui.surge/vstgui/lib/platform/linux/*.cpp )

  # Our friends at steinberg have this maddening habig of using #warning DEPRECATED in linux code
  set_source_files_properties(${SURGE_VST3_GLOB} PROPERTIES COMPILE_FLAGS -Wno-cpp)
  set_source_files_properties(${LINUX_VSTGUI_GLOB} PROPERTIES COMPILE_FLAGS -Wno-cpp)

  set(SURGE_OS_GUI_SOURCES
    ${CMAKE_BINARY_DIR}/lintemp/ScalablePiggy.S
    ${LINUX_VSTGUI_GLOB}
    src/linux/DisplayInfoLinux.cpp
    src/linux/RuntimeFontLinux.cpp
    src/linux/UserInteractionsLinux.cpp
    vstgui.surge/vstgui/lib/platform/common/genericoptionmenu.cpp
    vstgui.surge/vstgui/lib/platform/common/generictextedit.cpp
    vstgui.surge/vstgui/vstgui.cpp
    vstgui.surge/vstgui/vstgui_uidescription.cpp
    )

  find_package(PkgConfig REQUIRED)
  if( NOT LINUX_ON_ARM )
    # pkg_check_modules(CURL REQUIRED libcurl)
  endif()
  pkg_check_modules(CAIRO REQUIRED cairo)
  pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
  pkg_check_modules(X11 REQUIRED x11)
  pkg_check_modules(XCB REQUIRED xcb)
  pkg_check_modules(XCBCURSOR REQUIRED xcb-cursor)
  pkg_check_modules(XCBKEYSYMS REQUIRED xcb-keysyms)
  pkg_check_modules(XCBUTIL REQUIRED xcb-util)
  pkg_check_modules(XCBXKB REQUIRED xcb-xkb)
  pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
  pkg_check_modules(XKBX11COMMON REQUIRED xkbcommon-x11)

  if( LINUX_ON_ARM )
    set(ARCH_COMPILE_DEFINITIONS ARM_NEON=1 NOCURL=1)
    set(ARCH_INCLUDE_DIRECTORIES libs/simde)
  endif()

  set(OS_COMPILE_DEFINITIONS
    ${ARCH_COMPILE_DEFINITIONS}
    LINUX=1
    )

  set(OS_INCLUDE_DIRECTORIES
    src/linux

    ${CAIRO_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIRS}
    ${XCBCURSOR_INCLUDE_DIRS}
    ${XCBKEYSYMS_INCLUDE_DIRS}
    ${XCBUTIL_INCLUDE_DIRS}
    ${XCBXKB_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${XKBX11COMMON_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}

    ${CMAKE_BINARY_DIR}/lintemp

    ${ARCH_INCLUDE_DIRECTORIES}
    )

  set(OS_LINK_LIBRARIES_NOGUI
    pthread
    dl
    -Wl,--no-undefined
    ${CURL_LDFLAGS}
    )
  set(OS_LINK_LIBRARIES
    ${OS_LINK_LIBRARIES_NOGUI}
    ${CAIRO_LDFLAGS}
    ${FONTCONFIG_LDFLAGS}
    ${X11_LDFLAGS}
    ${XCB_LDFLAGS}
    ${XCBCURSOR_LDFLAGS}
    ${XCBKEYSYMS_LDFLAGS}
    ${XCBUTIL_LDFLAGS}
    ${XCBXKB_LDFLAGS}
    ${XKBCOMMON_LDFLAGS}
    ${XKBX11COMMON_LDFLAGS}
    )

    if (CMAKE_SYSTEM_NAME MATCHES "BSD")
      set(OS_LINK_LIBRARIES ${OS_LINK_LIBRARIES} execinfo)
    endif()

elseif( WIN32 )

  set(SURGE_OS_GUI_SOURCES
    src/windows/DisplayInfoWin.cpp
    src/windows/RuntimeFontWin.cpp
    src/windows/UserInteractionsWin.cpp
    src/windows/scalableresource.h
    src/windows/surge.rc
    src/windows/svgresources.rc
    vstgui.surge/vstgui/vstgui_win32.cpp
    vstgui.surge/vstgui/vstgui_uidescription_win32.cpp
  )

  set(OS_COMPILE_DEFINITIONS
    WINDOWS=1
    NOMINMAX=1
    WIN32
    _USE_MATH_DEFINES
    _WIN32_WINNT=0x0601
    _USRDLL
    VA_SUBTRACTIVE_EXPORTS
    USE_LIBPNG
    _CRT_SECURE_NO_WARNINGS=1
    UNICODE
    _UNICODE
    )

  if( ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
    set( WIN_DLL_BASENAME "Surge_x86" )
    set( PNG_LINK_DIR "libs/libpng/win/x86/lib" )
  else()
    set( WIN_DLL_BASENAME "Surge" )
    set( PNG_LINK_DIR "libs/libpng/win/x64/lib" )
  endif()
  message( STATUS "Configuring PNG linkage from ${PNG_LINK_DIR}" )

  set(OS_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    src/windows
    ${CMAKE_BINARY_DIR}/geninclude
    )

  set(OS_LINK_LIBRARIES_NOGUI
    shell32
    user32
    )   # winhttp

  set(OS_LINK_LIBRARIES
    ${OS_LINK_LIBRARIES_NOGUI}
    winmm
    gdi32
    gdiplus
    comdlg32
    comctl32
    ${CMAKE_SOURCE_DIR}/${PNG_LINK_DIR}/libpng16_static.lib
  )

  if(MINGW)
    list(APPEND OS_LINK_LIBRARIES
      d2d1
      dwmapi
      dwrite
      opengl32
      shlwapi
      windowscodecs
    )
  endif()

  if( ${BUILD_DLLCHECK} )
    add_executable( dllcheck
      utils/dllcheck/dllcheck.cpp
      ${CMAKE_BINARY_DIR}/geninclude/version.cpp
      )
    target_include_directories( dllcheck PRIVATE src/common )
  endif()
else()
  message(FATAL_ERROR "UNKNOWN OS. Please use lin mac or win" )
endif()

# Source Groups
source_group( "Libraries" REGULAR_EXPRESSION "libs/" )
source_group( "AirWindows" REGULAR_EXPRESSION "libs/airwindows/" )
source_group( "Surge Core" REGULAR_EXPRESSION "src/common/.*\.cpp" )
source_group( "Surge DSP" REGULAR_EXPRESSION "src/common/(vt_dsp|dsp)" )
source_group( "Surge FX" REGULAR_EXPRESSION "src/common/dsp/effect" )
source_group( "Surge GUI" REGULAR_EXPRESSION "src/common/gui" )
source_group( "Generated Code" REGULAR_EXPRESSION "version.cpp" )
source_group( "AU Plugin" REGULAR_EXPRESSION "src/au" )
source_group( "Headless" REGULAR_EXPRESSION "src/headless" )
source_group( "Linux" REGULAR_EXPRESSION "src/linux" )
source_group( "LV2 Plugin" REGULAR_EXPRESSION "src/lv2" )
source_group( "macOS" REGULAR_EXPRESSION "src/mac" )
source_group( "Windows" REGULAR_EXPRESSION "src/windows" )
source_group( "VST2 Plugin" REGULAR_EXPRESSION "src/vst2" )
source_group( "VST3 Plugin" REGULAR_EXPRESSION "src/vst3" )
source_group( "VST3 SDK" REGULAR_EXPRESSION "vst3sdk" )
source_group( "VSTGUI" REGULAR_EXPRESSION "vstgui.surge" )

#
# SURGE Core
#
target_sources(surge-shared PRIVATE ${SURGE_SHARED_SOURCES}  ${SURGE_OS_SHARED_SOURCES})
target_include_directories(surge-shared
        PRIVATE
        ${SURGE_COMMON_INCLUDES}
        ${OS_INCLUDE_DIRECTORIES}
        )
target_compile_definitions(surge-shared PRIVATE ${OS_COMPILE_DEFINITIONS} )

#
# AUDIO UNIT
#
if( BUILD_AU )
  add_library(surge-au-dll
    SHARED
    ${SURGE_SYNTH_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_AU_SOURCES}
    ${SURGE_AU_LIBRARY_SOURCES}
    )

  target_compile_definitions(surge-au-dll
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
    TARGET_AUDIOUNIT=1
    )

  target_include_directories(surge-au-dll
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES} ${SURGE_PREJUCE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/au
    libs/AUPublic
    libs/PublicUtility
    )

  target_link_libraries(surge-au-dll
    PRIVATE
    surge-shared
    ${OS_LINK_LIBRARIES}
    )

  add_custom_target( Surge-AU-Packaged ALL )
  add_dependencies( Surge-AU-Packaged surge-au-dll )
  add_custom_command(
    TARGET Surge-AU-Packaged
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Packaging up AU component"
    COMMAND ./scripts/macOS/package-au.sh  $<TARGET_FILE:surge-au-dll> ${SURGE_PRODUCT_DIR}
    COMMAND codesign --force --sign - --timestamp=none ${SURGE_PRODUCT_DIR}/Surge.component/
    COMMAND codesign -v ${SURGE_PRODUCT_DIR}/Surge.component/
    )

endif()

# Currently this is MAC only VST3
if( BUILD_VST3 )
  if( UNIX AND NOT APPLE )
    # Sigh - vst3sdk ships with non-working code if you has it
    get_filename_component(full_path_test_cpp ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk/base/source/timer.cpp ABSOLUTE)
    list(REMOVE_ITEM SURGE_VST3_LIBRARY_SOURCES "${full_path_test_cpp}")
  endif()

  if(MINGW)
    # make Steinberg::FUnknownPrivate::atomicAdd() compile when Steinberg::int32 is int instead of long
    set(funknown_path vst3sdk/pluginterfaces/base/funknown.cpp)
    get_filename_component(funknown "${CMAKE_CURRENT_SOURCE_DIR}/${funknown_path}" ABSOLUTE)
    list(REMOVE_ITEM SURGE_VST3_LIBRARY_SOURCES "${funknown}")
    file(STRINGS "${funknown_path}" funknown NEWLINE_CONSUME NO_HEX_CONVERSION)
    string(REPLACE "InterlockedExchangeAdd" "__sync_fetch_and_add" funknown "${funknown}")
    string(REPLACE "#include \"" "#include \"pluginterfaces/base/" funknown "${funknown}")
    file(WRITE "${CMAKE_BINARY_DIR}/${funknown_path}" ${funknown})
    list(APPEND SURGE_VST3_LIBRARY_SOURCES "${CMAKE_BINARY_DIR}/${funknown_path}")
  endif()

  add_library(surge-vst3-dll
    SHARED
    ${SURGE_SYNTH_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_VST3_SOURCES}
    ${SURGE_VST3_LIBRARY_SOURCES}
    )

  if( APPLE )
    target_sources(surge-vst3-dll PRIVATE vst3sdk/public.sdk/source/main/macmain.cpp)
  elseif( UNIX AND NOT APPLE )
    target_sources(surge-vst3-dll PRIVATE
         vst3sdk/public.sdk/source/main/linuxmain.cpp
         src/linux/LinuxVST3Helpers.cpp)
  elseif( WIN32 )
    target_sources(surge-vst3-dll PRIVATE vst3sdk/public.sdk/source/main/dllmain.cpp resources/windows-vst3/surge.def)
  endif()

  target_compile_definitions(surge-vst3-dll
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_VST3=1
    $<IF:$<CONFIG:DEBUG>,DEVELOPMENT,RELEASE>=1
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
    DEBUG=$<CONFIG:DEBUG>
    )

  if(MINGW)
    target_compile_definitions(surge-vst3-dll PRIVATE _NATIVE_WCHAR_T_DEFINED __wchar_t=wchar_t)
  endif()

  target_include_directories(surge-vst3-dll
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES} ${SURGE_PREJUCE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/vst3
    vst3sdk/
    )

  target_link_libraries(surge-vst3-dll
    PRIVATE
    surge-shared
    ${OS_LINK_LIBRARIES}
    )

  if( APPLE )
    add_custom_target( Surge-VST3-Packaged ALL )
    add_dependencies( Surge-VST3-Packaged surge-vst3-dll )
    add_custom_command(
      TARGET Surge-VST3-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST3 component"
      COMMAND ./scripts/macOS/package-vst3.sh $<TARGET_FILE:surge-vst3-dll> ${SURGE_PRODUCT_DIR}
      COMMAND codesign --force --sign - --timestamp=none ${SURGE_PRODUCT_DIR}/Surge.vst3/
      COMMAND codesign -v ${SURGE_PRODUCT_DIR}/Surge.vst3/
      )
   elseif( UNIX )
    add_custom_target( Surge-VST3-Packaged ALL )
    add_dependencies( Surge-VST3-Packaged surge-vst3-dll )
    add_custom_command(
      TARGET Surge-VST3-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST3 component"
      COMMAND pwd
      COMMAND echo ./scripts/linux/package-vst3.sh  $<TARGET_FILE:surge-vst3-dll> ${SURGE_PRODUCT_DIR} $<IF:$<CONFIG:DEBUG>,--nostrip,>
      COMMAND ./scripts/linux/package-vst3.sh  $<TARGET_FILE:surge-vst3-dll> ${SURGE_PRODUCT_DIR} $<IF:$<CONFIG:DEBUG>,--nostrip,>
      )
    elseif( WIN32 )
      add_custom_target( Surge-VST3-Packaged ALL
        DEPENDS surge-vst3-dll
        COMMAND ${CMAKE_COMMAND} -E echo "copying $<TARGET_FILE:surge-vst3-dll> to ${SURGE_PRODUCT_DIR}/${WIN_DLL_BASENAME}.vst3"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:surge-vst3-dll> ${SURGE_PRODUCT_DIR}/${WIN_DLL_BASENAME}.vst3
        )
   endif()

endif()

if( BUILD_VST2 )
  add_library(surge-vst2-dll
    SHARED
    ${SURGE_SYNTH_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_VST2_SOURCES}
    ${SURGE_VST2_LIBRARY_SOURCES}
    )

  if( UNIX AND NOT APPLE )
    target_sources(surge-vst2-dll PRIVATE src/linux/linux-aeffguieditor.cpp )

    # Ideally we wouldn't define __cdecl on linux. It's not needed
    # Alas, the VST2 SDK has in aeffgui a _GNUC define which requires
    # it so without __cdecl blank defined on linux vst2, we don't get
    # reliable builds. We considered defining it just where used but
    # it is used also, indirectly, by the vst3 sdk, so the smallest
    # change is this diff plus this comment.
    target_compile_definitions( surge-vst2-dll PRIVATE "__cdecl=" )
  endif()
  if( WIN32 )
    target_sources(surge-vst2-dll PRIVATE resources/windows-vst2/surge.def)
  endif()

  target_compile_definitions(surge-vst2-dll
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_VST2=1
    VSTGUI_ENABLE_DEPRECATED_METHODS=0
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
  )

  target_include_directories(surge-vst2-dll
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES} ${SURGE_PREJUCE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/vst2
    ${VST2SDK_DIR}
    )

  target_link_libraries(surge-vst2-dll
    PRIVATE
    surge-shared
    ${OS_LINK_LIBRARIES}
    )

  if( APPLE )
    add_custom_target( Surge-VST2-Packaged ALL )
    add_dependencies( Surge-VST2-Packaged surge-vst2-dll )
    add_custom_command(
      TARGET Surge-VST2-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST2 component"
      COMMAND ./scripts/macOS/package-vst.sh $<TARGET_FILE:surge-vst2-dll> ${SURGE_PRODUCT_DIR}
      COMMAND codesign --force --sign - --timestamp=none ${SURGE_PRODUCT_DIR}/Surge.vst/
      COMMAND codesign -v ${SURGE_PRODUCT_DIR}/Surge.vst/
      )
   endif()
  if( UNIX AND NOT APPLE )
    add_custom_target( Surge-VST2-Packaged ALL )
    add_dependencies( Surge-VST2-Packaged surge-vst2-dll)
    add_custom_command(
      TARGET Surge-VST2-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/products
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:surge-vst2-dll> ${SURGE_PRODUCT_DIR}/Surge.so
      )
   endif()
   if( WIN32 )
      add_custom_target( Surge-VST2-Packaged ALL
        DEPENDS surge-vst2-dll
        COMMAND ${CMAKE_COMMAND} -E echo "copying $<TARGET_FILE:surge-vst2-dll> to ${SURGE_PRODUCT_DIR}/${WIN_DLL_BASENAME}.dll"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:surge-vst2-dll> ${SURGE_PRODUCT_DIR}/${WIN_DLL_BASENAME}.dll
        )
   endif()

endif()

if( BUILD_LV2 )
  add_library(surge-lv2-dll
    SHARED
    ${SURGE_SYNTH_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_LV2_SOURCES}
    ${SURGE_LV2_LIBRARY_SOURCES}
    )

  target_compile_definitions(surge-lv2-dll
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_LV2=1
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
  )

  target_include_directories(surge-lv2-dll
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES} ${SURGE_PREJUCE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/lv2
    libs/lv2
    )

  target_link_libraries(surge-lv2-dll
    PRIVATE
    surge-shared
    ${OS_LINK_LIBRARIES}
    )

  # FIXME - these rules have all sorts of problems with out-of-line
  add_custom_target( Surge-LV2-Packaged ALL )
  add_dependencies( Surge-LV2-Packaged surge-lv2-dll )
  if( APPLE )
    add_custom_command(
      TARGET Surge-LV2-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up LV2 component"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SURGE_PRODUCT_DIR}/Surge.lv2
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:surge-lv2-dll> ${SURGE_PRODUCT_DIR}/Surge.lv2/Surge.dylib
      COMMAND scripts/linux/generate-lv2-ttl ${SURGE_PRODUCT_DIR}/Surge.lv2/Surge.dylib
      )
  elseif( UNIX AND NOT APPLE )
    add_custom_command(
      TARGET Surge-LV2-Packaged
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up LV2 component"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SURGE_PRODUCT_DIR}/Surge.lv2
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:surge-lv2-dll> ${SURGE_PRODUCT_DIR}/Surge.lv2/Surge.so
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lintemp
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/resources/data ${CMAKE_BINARY_DIR}/lintemp/surge
      COMMAND XDG_DATA_HOME=${CMAKE_BINARY_DIR}/lintemp scripts/linux/generate-lv2-ttl ${SURGE_PRODUCT_DIR}/Surge.lv2/Surge.so
      )
  endif()

endif()

if( BUILD_HEADLESS )
  add_executable(surge-headless
    ${SURGE_SYNTH_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    ${LIB_MIDIFILE_SOURCES}
    src/headless/main.cpp
    src/headless/UserInteractionsHeadless.cpp
    src/headless/LinkFixesHeadless.cpp
    src/headless/HeadlessUtils.cpp
    src/headless/Player.cpp
    src/headless/UnitTests.cpp
    src/headless/UnitTestUtilities.cpp
    src/headless/UnitTestsDSP.cpp
    src/headless/UnitTestsFLT.cpp
    src/headless/UnitTestsFX.cpp
    src/headless/UnitTestsIO.cpp
    src/headless/UnitTestsMIDI.cpp
    src/headless/UnitTestsMOD.cpp
    src/headless/UnitTestsMSEG.cpp
    src/headless/UnitTestsPARAM.cpp
    src/headless/UnitTestsTUN.cpp
          src/headless/HeadlessNonTestFunctions.cpp src/headless/HeadlessNonTestFunctions.h)

  target_compile_definitions(surge-headless
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_HEADLESS=1
    LIBMIDIFILE=1
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
  )

  target_include_directories(surge-headless
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${LIB_MIDIFILE_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/headless
    )

  # Temporarily disable libsndfile which we don't use anyway, but we don't have a link for on arm
  # find_package(LibSndFile ${PACKAGE_OPTION})
  if(NOT LIBSNDFILE_FOUND)
    #message("-- LibSndFile not installed; building without wav support")
    #message("-- You can 'brew install libsndfile' or 'apt-get install libsndfile1-dev'")
  else()
    target_compile_definitions(surge-headless
      PRIVATE
      LIBSNDFILE=1
      )
    target_link_libraries(surge-headless PRIVATE ${LIBSNDFILE_LIBRARIES})
    target_include_directories(surge-headless PRIVATE ${LIBSNDFILE_INCLUDE_DIRS})
  endif()

  target_link_libraries(surge-headless
    PRIVATE
    surge-shared
    surge-tests
    surge::catch2
    ${OS_LINK_LIBRARIES_NOGUI}
    )

  if( UNIX AND NOT APPLE )
    find_package(Threads REQUIRED)
    target_link_libraries(surge-headless
      PRIVATE
      Threads::Threads
      )

    if (CMAKE_SYSTEM_NAME MATCHES "BSD")
      target_link_libraries(surge-headless PRIVATE execinfo)
    endif()
  endif()
endif()

add_custom_target( all-components )
add_custom_target( pipeline-components )
if( BUILD_VST2 )
  add_dependencies(all-components Surge-VST2-Packaged )
endif()
if( BUILD_VST3 )
  add_dependencies(all-components Surge-VST3-Packaged )
  add_dependencies(pipeline-components Surge-VST3-Packaged)
endif()
if( BUILD_AU )
  add_dependencies(all-components Surge-AU-Packaged )
  add_dependencies(pipeline-components Surge-AU-Packaged)
endif()
if( BUILD_LV2 )
  add_dependencies(all-components Surge-LV2-Packaged )
  # Since we dont' distro the LV2 don't build it in the pipeline
  # add_dependencies(pipeline-components Surge-LV2-Packaged)
endif()
if( BUILD_HEADLESS )
  add_dependencies(all-components surge-headless )
endif()

#
# INSTALL RULES - currently implemented on APPLE and LINUX only
#
if( APPLE )
  if( BUILD_HEADLESS )
    add_custom_target( run-headless )
    add_dependencies( run-headless install-resources-local )
    add_dependencies( run-headless surge-headless )
    add_custom_command(
      TARGET run-headless
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND $<TARGET_FILE:surge-headless>
      )
  endif()

  add_custom_target( install-resources-local )
  add_custom_command(
    TARGET install-resources-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing local resources"
    COMMAND rsync -r --delete "resources/data/" "\${HOME}/Library/Application Support/Surge/"
    )

  add_custom_target( install-resources-global )
  add_custom_command(
    TARGET install-resources-global
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing global resources"
    COMMAND rsync -r --delete "resources/data/" "/Library/Application Support/Surge/"
    )

  if( BUILD_AU )
    add_custom_target( install-au-local )
    add_dependencies( install-au-local Surge-AU-Packaged )
    add_dependencies( install-au-local install-resources-local )
    add_custom_command(
      TARGET install-au-local
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing audio unit locally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.component/" "\${HOME}/Library/Audio/Plug-Ins/Components/Surge.component/"
      )

    add_custom_target( install-au-global )
    add_dependencies( install-au-global Surge-AU-Packaged )
    add_dependencies( install-au-global install-resources-global )
    add_custom_command(
      TARGET install-au-global
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing audio unit globally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.component/" "/Library/Audio/Plug-Ins/Components/Surge.component/"
      )

    add_custom_target( validate-au )
    add_dependencies( validate-au install-au-local )
    add_custom_command(
      TARGET validate-au
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND auval -vt aumu VmbA
      )
  endif()

  if( BUILD_VST2 )
    add_custom_target( install-vst2-local )
    add_dependencies( install-vst2-local Surge-VST2-Packaged )
    add_dependencies( install-vst2-local install-resources-local )
    add_custom_command(
      TARGET install-vst2-local
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst2 unit locally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst/" "\${HOME}/Library/Audio/Plug-Ins/VST/Surge.vst/"
      )

    add_custom_target( install-vst2-global )
    add_dependencies( install-vst2-global Surge-VST2-Packaged )
    add_dependencies( install-vst2-global install-resources-global )
    add_custom_command(
      TARGET install-vst2-global
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst2 unit globally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst/" "/Library/Audio/Plug-Ins/VST/Surge.vst/"
      )
  endif()

  if( BUILD_VST3 )
    add_custom_target( install-vst3-local )
    add_dependencies( install-vst3-local Surge-VST3-Packaged )
    add_dependencies( install-vst3-local install-resources-local )
    add_custom_command(
      TARGET install-vst3-local
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst3 unit locally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst3/" "\${HOME}/Library/Audio/Plug-Ins/VST3/Surge.vst3/"
      )

    add_custom_target( install-vst3-global )
    add_dependencies( install-vst3-global Surge-VST3-Packaged )
    add_dependencies( install-vst3-global install-resources-global )
    add_custom_command(
      TARGET install-vst3-global
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst3 unit globally"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst3/" "/Library/Audio/Plug-Ins/VST3/Surge.vst3/"
      )
  endif()

  add_custom_target( install-everything-local )
  if( BUILD_AU )
    add_dependencies( install-everything-local install-au-local )
  endif()
  if( BUILD_VST3 )
    add_dependencies( install-everything-local install-vst3-local )
  endif()
  if( BUILD_VST2 )
    add_dependencies( install-everything-local install-vst2-local )
  endif()

  add_custom_target( install-everything-global )
  if( BUILD_AU )
    add_dependencies( install-everything-global install-au-global )
  endif()
  if( BUILD_VST3 )
    add_dependencies( install-everything-global install-vst3-global )
  endif()
  if( BUILD_VST2 )
    add_dependencies( install-everything-global install-vst2-global )
  endif()


elseif( UNIX AND NOT APPLE )
  add_custom_target( install-resources-local )
  add_custom_command(
    TARGET install-resources-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing local resources into ~/.local/share/surge"
    COMMAND rsync -r --delete "resources/data/" "\${HOME}/.local/share/surge/"
    )

  add_custom_target( install-resources-global )
  add_custom_command(
    TARGET install-resources-global
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing local resources into ${CMAKE_INSTALL_PREFIX}/share/surge"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/share/surge
    COMMAND rsync -r --delete "resources/data/" "${CMAKE_INSTALL_PREFIX}/share/surge/"
    )

  if( BUILD_VST2 )
    add_custom_target( install-vst2-local )
    add_dependencies( install-vst2-local Surge-VST2-Packaged )
    add_dependencies( install-vst2-local install-resources-local )
    add_custom_command(
      TARGET install-vst2-local
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst2 unit into ~/.vst"
      COMMAND ${CMAKE_COMMAND} -E make_directory "\${HOME}/.vst"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.so" "\${HOME}/.vst"
      )

    add_custom_target( install-vst2-global )
    add_dependencies( install-vst2-global Surge-VST2-Packaged )
    add_dependencies( install-vst2-global install-resources-global )
    add_custom_command(
      TARGET install-vst2-global
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst2 unit into ${CMAKE_INSTALL_PREFIX}/lib/vst"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_INSTALL_PREFIX}/lib/vst"
      COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.so" "${CMAKE_INSTALL_PREFIX}/lib/vst"
      )
  endif()

  add_custom_target( install-vst3-local )
  add_dependencies( install-vst3-local Surge-VST3-Packaged )
  add_dependencies( install-vst3-local install-resources-local )
  add_custom_command(
    TARGET install-vst3-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing vst3 unit locally into ~/.vst3"
    COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst3" "\${HOME}/.vst3"
    )

  add_custom_target( install-vst3-global )
  add_dependencies( install-vst3-global Surge-VST3-Packaged )
  add_dependencies( install-vst3-global install-resources-global )
  add_custom_command(
    TARGET install-vst3-global
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/lib/vst3
    COMMAND echo "Installing vst3 unit locally into ${CMAKE_INSTALL_PREFIX}/lib/vst3"
    COMMAND rsync -r --delete "${SURGE_PRODUCT_DIR}/Surge.vst3" "${CMAKE_INSTALL_PREFIX}/lib/vst3"
    )

  add_custom_target( install-everything-local )
  add_dependencies( install-everything-local install-vst3-local )
  if( BUILD_VST2 )
    add_dependencies( install-everything-local install-vst2-local )
  endif()

  add_custom_target( install-everything-global )
  add_dependencies( install-everything-global install-vst3-global )
  if( BUILD_VST2 )
    add_dependencies( install-everything-global install-vst2-global )
  endif()


endif()

# Extra packages and utils
if( ${BUILD_SVGSHOW} )
  add_subdirectory( utils/svgshow )
endif()

#
# Various JUCE targets we build in the shared source environment
#
if( ${BUILD_SURGE_JUCE_PLUGINS} )
  include( cmake/get-juce.cmake )

  add_custom_target(surge-juce-pipeline-targets)

  message( STATUS "Building JUCE targets with ${JUCE_LIB_DIR}" )
  add_subdirectory( ${JUCE_LIB_DIR} )

  # LinkOrder on Linux matters so make a little lib which contains UIJUCE so I can link
  # it AFTER surge-shared (since the libs come after the source in the JUCE build of course)
  # FIXME of course that this should go away in surge-xt. These are also split now for
  # XT vs FX since FX is production code in 19 and XT is not
  add_library(surge-juce-userint-fx src/surge_effects_bank/UserInteractionsJUCE.cpp )
  target_include_directories(surge-juce-userint-fx PRIVATE ${SURGE_COMMON_INCLUDES})

  set(JUCE_ASIO_SUPPORT FALSE)

  if (DEFINED ENV{VST2SDK_DIR})
    file(TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" JUCE_VST2_DIR)
    juce_set_vst2_sdk_path(${JUCE_VST2_DIR})
    set(SURGE_JUCE_FORMATS AU VST3 VST Standalone)
    message(STATUS "JUCE VST2 SDK Path is $ENV{VST2SDK_DIR}")
  else()
    set(SURGE_JUCE_FORMATS AU VST3 Standalone)
  endif()

  message(STATUS "Building Surge JUCE as ${SURGE_JUCE_FORMATS}")
  if (DEFINED ENV{ASIOSDK_DIR})
    file(TO_CMAKE_PATH "$ENV{ASIOSDK_DIR}" ASIOSDK_DIR)
    message(STATUS "ASIO SDK found at ${ASIOSDK_DIR}")
    set(JUCE_ASIO_SUPPORT TRUE)
  endif()

  if( BUILD_SURGE_EFFECTS_BANK )
    juce_add_plugin(surge-fx
            PRODUCT_NAME "SurgeEffectsBank"
            COMPANY_NAME "Surge Synth Team"
            BUNDLE_ID "org.surge-synth-team.surge-fx"
            PLUGIN_MANUFACTURER_CODE VmbA
            PLUGIN_CODE SFXB

            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE

            FORMATS ${SURGE_JUCE_FORMATS}
            )

    juce_generate_juce_header( surge-fx )

    file(GLOB SURGE_FX_BANK_RESOURCES_GLOB
            resources/surge_effects_bank/*.svg
            resources/surge_effects_bank/icons/*.svg
            )

    juce_add_binary_data( surge-fx-binary
            SOURCES ${SURGE_FX_BANK_RESOURCES_GLOB}
            )
    set_target_properties(surge-fx-binary PROPERTIES
            POSITION_INDEPENDENT_CODE TRUE
            )

    target_compile_definitions(surge-fx PUBLIC
            JUCE_ALLOW_STATIC_NULL_VARIABLES=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1

            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CAMERA=disabled

            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0

            JUCE_ALSA=1
            JUCE_JACK=1

            TARGET_HEADLESS=1
            )

    if (JUCE_ASIO_SUPPORT)
      target_compile_definitions(surge-fx PUBLIC
        JUCE_ASIO=1
        )
    endif()

    target_sources(surge-fx PRIVATE
            src/surge_effects_bank/SurgeFXEditor.cpp
            src/surge_effects_bank/SurgeFXProcessor.cpp
            src/surge_effects_bank/SurgeLookAndFeel.h
            ${SURGE_GENERATED_SOURCES}
            ${SURGE_OS_SOURCES}
            )

    target_link_libraries(surge-fx PRIVATE
            surge::airwindows
            surge::filesystem
            surge::tinyxml

            surge-shared
            surge-juce-userint-fx
            surge-fx-binary

            juce::juce_audio_utils
            juce::juce_audio_processors
            )

    target_include_directories(surge-fx PRIVATE
            src/surge_effects_bank
            ${SURGE_COMMON_INCLUDES}
            ${OS_INCLUDE_DIRECTORIES}
            )

    if (JUCE_ASIO_SUPPORT)
      target_include_directories(surge-fx PRIVATE
        ${ASIOSDK_DIR}/common
        )
    endif()

    target_compile_definitions(surge-fx PRIVATE ${OS_COMPILE_DEFINITIONS} )

    get_target_property( SURGE_FX_OUTPUT_DIR surge-fx RUNTIME_OUTPUT_DIRECTORY )

    add_custom_target( Surge-Effects-Bank-Packaged ALL )
    add_dependencies( Surge-Effects-Bank-Packaged surge-fx_All )
    add_dependencies( Surge-Effects-Bank-Packaged surge-fx_VST3 )
    add_dependencies( Surge-Effects-Bank-Packaged surge-fx_Standalone )

    add_dependencies(surge-juce-pipeline-targets surge-fx_Standalone)
    add_custom_command(
            TARGET Surge-Effects-Bank-Packaged
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND echo "Re-locating VST3 and Standalone components"
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_FX_OUTPUT_DIR}/VST3 ${SURGE_PRODUCT_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_FX_OUTPUT_DIR}/Standalone ${SURGE_PRODUCT_DIR}/
    )

    if( APPLE )
      add_dependencies( Surge-Effects-Bank-Packaged surge-fx_AU )

      add_custom_command(
            TARGET Surge-Effects-Bank-Packaged
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND echo "Re-locating AU components"
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_FX_OUTPUT_DIR}/AU ${SURGE_PRODUCT_DIR}/
      )
    endif()

    add_dependencies( pipeline-components Surge-Effects-Bank-Packaged )
  endif() # build surge effects bank

  if( BUILD_SURGE_XT )
    juce_add_plugin(surge-xt
            PRODUCT_NAME "Surge XT"
            COMPANY_NAME "Surge Synth Team"
            BUNDLE_ID "org.surge-synth-team.surge-xt"
            PLUGIN_MANUFACTURER_CODE VmbA
            PLUGIN_CODE SgXT

            ICON_BIG "resources/surge-xt/SurgeLogo.png"

            IS_SYNTH TRUE
            NEEDS_MIDI_INPUT TRUE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE

            FORMATS ${SURGE_JUCE_FORMATS}
            )

    juce_generate_juce_header( surge-xt )

    file(GLOB SURGE_SYNTH_JUCE_RESOURCES_GLOB
            assets/SurgeClassic/exported/*svg
            resources/fonts/Lato*ttf
            )

    juce_add_binary_data( surge-xt-binary
            SOURCES ${SURGE_SYNTH_JUCE_RESOURCES_GLOB}
            )
    set_target_properties(surge-xt-binary PROPERTIES
            POSITION_INDEPENDENT_CODE TRUE
            )

    target_compile_definitions(surge-xt PUBLIC
            JUCE_ALLOW_STATIC_NULL_VARIABLES=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1

            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CAMERA=disabled

            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0

            JUCE_ALSA=1
            JUCE_JACK=1

            TARGET_JUCE_SYNTH=1
            TARGET_HEADLESS=1
            TARGET_JUCE_UI=1

            $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
            )

    if (JUCE_ASIO_SUPPORT)
      target_compile_definitions(surge-xt PUBLIC
        JUCE_ASIO=1
        )
    endif()

    # LinkOrder on Linux matters so make a little lib which contains UIJUCE so I can link
    # it AFTER surge-shared (since the libs come after the source in the JUCE build of course)
    add_library(surge-xt-userint src/surge_synth_juce/UserInteractionsJUCE_Synth_XT.cpp)
    target_include_directories(surge-xt-userint PRIVATE ${SURGE_COMMON_INCLUDES})

    set(SURGE_SYNTH_JUCE_GUI_SOURCES
           ${SURGE_GUI_SOURCES}
            )

    target_sources(surge-xt PRIVATE
            src/surge_synth_juce/SurgeSynthEditor.cpp
            src/surge_synth_juce/SurgeSynthProcessor.cpp
            ${SURGE_GENERATED_SOURCES}
            ${SURGE_OS_SOURCES}
            ${SURGE_SYNTH_SOURCES}

            ${SURGE_SYNTH_JUCE_GUI_SOURCES}
            )

    target_sources(surge-xt_VST3 PRIVATE
            src/surge_synth_juce/SurgeSynthVST3Extensions.cpp
            )

    target_sources(surge-xt_Standalone PRIVATE
            src/surge_synth_juce/SurgeSynthStandaloneExtensions.cpp
            )

    if( DEFINED ENV{VST2SDK_DIR} )
      target_sources(surge-xt_VST PRIVATE
              src/surge_synth_juce/SurgeSynthVST2Extensions.cpp
              )
    endif()

    if( APPLE )
      target_sources(surge-xt_AU PRIVATE
              src/surge_synth_juce/SurgeSynthAUExtensions.cpp
              )
    endif()

    target_link_libraries(surge-xt PUBLIC
            surge::airwindows
            surge::filesystem
            surge::tinyxml

            surge-shared
            surge-xt-userint

            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_audio_plugin_client

            escape-from-vstgui

            surge-xt-binary
            )

    target_include_directories(surge-xt PUBLIC
            src/surge_synth_juce
            src/headless
            ${SURGE_COMMON_INCLUDES}
            ${SURGE_GUI_INCLUDES}
            ${OS_INCLUDE_DIRECTORIES}
            )

    if (JUCE_ASIO_SUPPORT)
      target_include_directories(surge-xt PUBLIC
        ${ASIOSDK_DIR}/common
        )
    endif()

    target_compile_definitions(surge-xt PUBLIC ${OS_COMPILE_DEFINITIONS} DONT_SET_USING_JUCE_NAMESPACE=1)

    target_compile_definitions(surge-xt_Standalone PUBLIC
            TARGET_JUCE_SYNTH=1
            TARGET_HEADLESS=1
            TARGET_JUCE_UI=1)

    add_dependencies(surge-juce-pipeline-targets surge-xt_Standalone)

    add_custom_target(surge-xt-distribution)
    add_dependencies(surge-xt-distribution surge-xt_All)
    if( APPLE )
      set( SXT_ZIP AU VST3 Standalone)
    else()
      set( SXT_ZIP VST3 Standalone)
    endif()

    if( DEFINED ENV{SURGE_VERSION} )
      set( SXTVER $ENV{SURGE_VERSION})
    else()
      set( SXTVER "LOCAL")
    endif()

    if( APPLE )
      set( SXTOS "macOS" )
    elseif( UNIX)
      set( SXTOS "linux" )
    else()
      set( SXTOS "win64" )
    endif()

    add_custom_command(TARGET surge-xt-distribution
            POST_BUILD
            WORKING_DIRECTORY $<GENEX_EVAL:$<TARGET_PROPERTY:surge-xt,LIBRARY_OUTPUT_DIRECTORY>>
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/resources/surge-xt/README.txt .
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/surge-xt-zip
            COMMAND ${CMAKE_COMMAND} -E tar cvf ${CMAKE_BINARY_DIR}/surge-xt-zip/surge-xt-alpha-${SXTVER}-${SXTOS}.zip --format=zip ${SXT_ZIP} README.txt
            )

    if (APPLE)  # Just because BP is too lazy to code these up on the other OSes yet
      add_custom_target(surge-xt-vst3-local)
      add_dependencies(surge-xt-vst3-local surge-xt_VST3)
      add_custom_command(TARGET surge-xt-vst3-local
              POST_BUILD
              WORKING_DIRECTORY $<GENEX_EVAL:$<TARGET_PROPERTY:surge-xt,LIBRARY_OUTPUT_DIRECTORY>>
              COMMAND ls -l
              COMMAND ${CMAKE_COMMAND} -E copy_directory VST3 ~/Library/Audio/Plug-Ins/VST3
              )

      add_custom_target(surge-xt-au-local)
      add_dependencies(surge-xt-au-local surge-xt_AU)
      add_custom_command(TARGET surge-xt-au-local
              POST_BUILD
              WORKING_DIRECTORY $<GENEX_EVAL:$<TARGET_PROPERTY:surge-xt,LIBRARY_OUTPUT_DIRECTORY>>
              COMMAND ls -l
              COMMAND ${CMAKE_COMMAND} -E copy_directory AU ~/Library/Audio/Plug-Ins/Components
              )
    endif()
  endif()
endif()

if( ${BUILD_SURGE_PYTHON_BINDINGS} )
  message( STATUS "Building Surge Python bindings with pybind11" )
  add_subdirectory(libs/pybind11)
  set( PYSRCD src/python_bindings )
  set( PYSRC )
  pybind11_add_module(surgepy )

  target_sources( surgepy PRIVATE
          ${SURGE_SYNTH_SOURCES}
          ${SURGE_OS_SOURCES}
          ${SURGE_GENERATED_SOURCES}

          ${PYSRCD}/surgepy.cpp
          src/headless/HeadlessUtils.cpp
          src/headless/UserInteractionsHeadless.cpp
          )

  target_compile_definitions(surgepy
          PRIVATE
          ${OS_COMPILE_DEFINITIONS}
          TARGET_HEADLESS=1
          LIBMIDIFILE=1
          $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
          )

  target_include_directories(surgepy
          PRIVATE
          ${SURGE_COMMON_INCLUDES}
          ${LIB_MIDIFILE_INCLUDES}
          ${OS_INCLUDE_DIRECTORIES}
          src/headless
          ${PYSRCD}
          )

  target_link_libraries(surgepy
          PRIVATE
          surge-shared
          ${OS_LINK_LIBRARIES_NOGUI}
          )

  if( UNIX AND NOT APPLE )
    message( STATUS "Manually adding ${PYTHON_LIBRARIES} to surgepy target" )
    find_package(Threads REQUIRED)
    target_link_libraries(surgepy
            PRIVATE
            Threads::Threads
            ${PYTHON_LIBRARIES}
            )

    if (CMAKE_SYSTEM_NAME MATCHES "BSD")
      target_link_libraries(surgepy PRIVATE execinfo)
    endif()
  endif()
else()
  message( STATUS "Python bindings disabled" )
endif()

# We have a special target here which the PR pipeline uses to make sure things
# are of the appropriate code quality. This allows us to write CMAKE rules which
# become linters and stuff. The code will run on macos in the pipeline. I suppose
# you could run it locally to, but really, you should know what you are doing if you
# do that. And I'll document it so you can know that when I use it earnest in 1.9
add_custom_target(code-quality-pipeline-checks)

# Check 1: The extra content is properly specified.
add_dependencies(code-quality-pipeline-checks download-extra-content)

# Coming in 1.9: CLang Format checks and others
set(CLANG_FORMAT_DIRS libs/escape-from-vstgui src)
set(CLANG_FORMAT_EXTS cpp h)
foreach(dir ${CLANG_FORMAT_DIRS})
  foreach(ext ${CLANG_FORMAT_EXTS})
    list(APPEND CLANG_FORMAT_GLOBS "':(glob)${dir}/**/*.${ext}'")
  endforeach()
endforeach()
add_custom_command(TARGET code-quality-pipeline-checks
  POST_BUILD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E echo About to check clang-format
  COMMAND git ls-files -- ${CLANG_FORMAT_GLOBS} | xargs clang-format --dry-run --Werror
)
