# Surge CMake file
# Exports targets for each of our builds
#
# Current Status - this builds the whole shooting match
# See issue 1206 for our working list to get this finished
#

cmake_minimum_required(VERSION 3.10)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Build for 10.12")

project(Surge VERSION 1.7.0.0 LANGUAGES CXX ASM)

include( "cmake/versiontools.cmake" )
add_custom_target(surge-shared)


set(CMAKE_BUILD_TYPE Release)

# Set up external packages
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
find_package(LibMidiFile ${PACKAGE_OPTIONS})

# Things we can build
set( BUILD_HEADLESS true )
set( BUILD_VST3 true )

if( DEFINED ENV{VST2SDK_DIR} )
  set( BUILD_VST2 true )
  file( TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" VST2SDK_DIR )
  message( ERROR " Building VST2 using " ${VST2SDK_DIR} " - this is an unsupported configuration" )
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
  src/common/gui/guihelpers.cpp
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

set( SURGE_LV2_SOURCES
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
  libs/filesystem
  libs/xml
  libs/catch2
  libs/strnatcmp
  libs/tuning-library/include
  src/common
  src/common/dsp
  src/common/thread
  src/common/vt_dsp
  ${CMAKE_BINARY_DIR}/geninclude
  )

set(SURGE_GUI_INCLUDES
  src/common/gui
  vstgui.surge
  libs/nanosvg/src
  )

### OS SPECIFIC SECTION
if( APPLE )
  set(SURGE_OS_SOURCES
    src/mac/UserInteractionsMac.mm
    )

  set(SURGE_OS_GUI_SOURCES
    src/mac/DisplayInfoMac.mm
    src/mac/RuntimeFontMac.cpp
    src/mac/cocoa_utils.mm
    vstgui.surge/vstgui/vstgui_mac.mm
    vstgui.surge/vstgui/vstgui_uidescription_mac.mm
    )

  set(OS_COMPILE_OPTIONS
    -msse2
    "-D_aligned_malloc(x,a)=malloc(x)"
    "-D_aligned_free(x)=free(x)"
    -Wno-deprecated-declarations
    -Werror=inconsistent-missing-override
    -Werror=logical-op-parentheses
    -Werror=dynamic-class-memaccess
    -Werror=undefined-bool-conversion
    -Werror=bitwise-op-parentheses
    )
  set(OS_INCLUDE_DIRECTORIES
    src/mac
    )
  set(OS_COMPILE_DEFINITIONS
    MAC=1
    MAC_COCOA=1
    COCOA=1
    OBJC_OLD_DISPATCH_PROTOTYPES=1
    )
  set(OS_LINK_LIBRARIES
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
  file(GLOB PIGGY_INPUTS assets/original-vector/SVG/exported/*svg)
  add_custom_command( OUTPUT ${CMAKE_BINARY_DIR}/lintemp/ScalablePiggy.S 
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      DEPENDS ${PIGGY_INPUTS}
                      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lintemp
                      COMMAND python ${CMAKE_SOURCE_DIR}/scripts/linux/emit-vector-piggy.py ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/lintemp 
                    )

  set(SURGE_OS_SOURCES
    src/linux/UserInteractionsLinux.cpp
    src/linux/ConfigurationXml.S
    ${CMAKE_BINARY_DIR}/lintemp/ScalablePiggy.S
    )

  file(GLOB LINUX_VSTGUI_GLOB vstgui.surge/vstgui/lib/platform/linux/*.cpp )

  # Our friends at steinberg have this maddening habig of using #warning DEPRECATED in linux code
  set_source_files_properties(${SURGE_VST3_GLOB} PROPERTIES COMPILE_FLAGS -Wno-cpp)
  set_source_files_properties(${LINUX_VSTGUI_GLOB} PROPERTIES COMPILE_FLAGS -Wno-cpp)


  set(SURGE_OS_GUI_SOURCES
    src/linux/DisplayInfoLinux.cpp
    src/linux/RuntimeFontLinux.cpp
    vstgui.surge/vstgui/vstgui.cpp
    vstgui.surge/vstgui/vstgui_uidescription.cpp
    vstgui.surge/vstgui/lib/platform/common/genericoptionmenu.cpp
    vstgui.surge/vstgui/lib/platform/common/generictextedit.cpp
    ${LINUX_VSTGUI_GLOB}
    )

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CAIRO REQUIRED cairo)
  pkg_check_modules(XCB REQUIRED xcb)
  pkg_check_modules(XCBCURSOR REQUIRED xcb-cursor)
  pkg_check_modules(XCBUTIL REQUIRED xcb-util)
  pkg_check_modules(XCBXKB REQUIRED xcb-xkb)
  pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
  pkg_check_modules(XKBX11COMMON REQUIRED xkbcommon-x11)

  set(OS_COMPILE_OPTIONS
    -msse2
    "-D_aligned_malloc(x,a)=malloc(x)"
    "-D_aligned_free(x)=free(x)"
    -Werror
    -Wno-multichar
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fdata-sections
    -ffunction-sections

    ${CAIRO_CFLAGS_OTHER}
    )

  set(OS_COMPILE_DEFINITIONS
    LINUX=1
    RELEASE=1
    )

  set(OS_INCLUDE_DIRECTORIES
    src/linux

    ${CAIRO_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIRS}
    ${XCBCURSOR_INCLUDE_DIRS}
    ${XCBUTIL_INCLUDE_DIRS}
    ${XCBXKB_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${XKBX11COMMON_INCLUDE_DIRS}

    ${CMAKE_BINARY_DIR}/lintemp
    )

  set(OS_LINK_LIBRARIES
     pthread
     stdc++fs
     gcc_s
     gcc
     dl
     ${CAIRO_LDFLAGS}
     ${XCB_LDFLAGS}
     ${XCBCURSOR_LDFLAGS}
     ${XCBUTIL_LDFLAGS}
     ${XCBXKB_LDFLAGS}
     ${XKBCOMMON_LDFLAGS}
     ${XKBX11COMMON_LDFLAGS}
    )

elseif( WIN32 )

  set(SURGE_OS_SOURCES
    src/windows/UserInteractionsWin.cpp
    )

  set(SURGE_OS_GUI_SOURCES
    src/windows/DisplayInfoWin.cpp
    src/windows/RuntimeFontWin.cpp
    src/windows/surge.rc
    vstgui.surge/vstgui/vstgui_win32.cpp
    vstgui.surge/vstgui/vstgui_uidescription_win32.cpp
  )

  set(OS_COMPILE_DEFINITIONS
    WINDOWS=1
    NOMINMAX=1
    WIN32
    _WIN32_WINNT=0x0601
    _USRDLL
    VA_SUBTRACTIVE_EXPORTS
    TIXML_USE_STL
    USE_LIBPNG
    _CRT_SECURE_NO_WARNINGS=1
    )

  if( ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
    set( WIN_DLL_BASENAME "Surge_x86" )
    list( APPEND OS_COMPILE_DEFINITIONS WIN_X86=1 )
  else()
    set( WIN_DLL_BASENAME "Surge" )
  endif()


  set(OS_COMPILE_OPTIONS
    /wd4244   # float to double
    /wd4305   # truncation of variable
    /wd4267   # int and size_t
    /wd4018   # signed unsigned mismatch
    /wd4388   # signed unsigned mismatch in comparison
    /wd4477   # blah blah wide strings after we made it work
    /wd4065   # standalone default in a switch with no case
    /wd4702   # unreachable code. I generally do if( a ) return foo else return bar; return nullptr so don't warn on that
    
    /FIprecompiled.h
    /Zc:alignedNew
    /bigobj
    )

  set(OS_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    src/windows
    libs/wtl
    )

  set(OS_LINK_LIBRARIES
    Winmm
    gdi32
    gdiplus
    ComDlg32
    ComCtl32
    shell32
    user32
    ../libs/libpng/win/x64/lib/libpng16_static
  )
else()
  message(FATAL_ERROR "UNKNOWN OS. Please use lin mac or win" )
endif()


#
# AUDIO UNIT
#
if( BUILD_AU )
  add_library(surge-au
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_AU_SOURCES}
    ${SURGE_AU_LIBRARY_SOURCES}
    )
  add_dependencies(surge-au surge-shared)
  
  target_compile_features(surge-au PRIVATE cxx_std_17 )

  target_compile_definitions(surge-au
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_AUDIOUNIT=1
    )

  target_compile_options(surge-au
    PRIVATE
    ${OS_COMPILE_OPTIONS}
    )

  target_include_directories(surge-au
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/au
    libs/AUPublic
    libs/PublicUtility
    )

  target_link_libraries(surge-au
    PRIVATE
    ${OS_LINK_LIBRARIES}
    )

  add_custom_target( Surge.component ALL )
  add_dependencies( Surge.component surge-au )
  add_custom_command(
    TARGET Surge.component
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Packaging up AU component"
    COMMAND ./scripts/macOS/package-au.sh  ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libsurge-au.dylib
    )

endif()

# Currently this is MAC only VST3
if( BUILD_VST3 )
  if( UNIX AND NOT APPLE )
    # Sigh - vst3sdk ships with non-working code if you has it
    get_filename_component(full_path_test_cpp ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk/base/source/timer.cpp ABSOLUTE)
    list(REMOVE_ITEM SURGE_VST3_LIBRARY_SOURCES "${full_path_test_cpp}")
  endif()

  add_library(surge-vst3
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_VST3_SOURCES}
    ${SURGE_VST3_LIBRARY_SOURCES}
    )
  add_dependencies(surge-vst3  surge-shared)
  
  target_compile_features(surge-vst3 PRIVATE cxx_std_17 )

  if( APPLE )
    target_sources(surge-vst3 PRIVATE vst3sdk/public.sdk/source/main/macmain.cpp)
  elseif( UNIX AND NOT APPLE )
    target_sources(surge-vst3 PRIVATE
         vst3sdk/public.sdk/source/main/linuxmain.cpp
         src/linux/LinuxVST3Helpers.cpp)
  elseif( WIN32 )
    target_sources(surge-vst3 PRIVATE vst3sdk/public.sdk/source/main/dllmain.cpp)
    target_link_options( surge-vst3 PUBLIC  "/DEF:..\\resources\\windows-vst3\\surge.def" )
  endif()

  target_compile_definitions(surge-vst3
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_VST3=1
    )

  target_compile_options(surge-vst3
    PRIVATE
    ${OS_COMPILE_OPTIONS}
    )

  target_include_directories(surge-vst3
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/vst3
    vst3sdk/
    )

  target_link_libraries(surge-vst3
    PRIVATE
    ${OS_LINK_LIBRARIES}
    )

  if( APPLE )
    add_custom_target( Surge.vst3 ALL )
    add_dependencies( Surge.vst3 surge-vst3 )
    add_custom_command(
      TARGET Surge.vst3
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST3 component"
      COMMAND ./scripts/macOS/package-vst3.sh ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libsurge-vst3.dylib
      )
   elseif( UNIX )
    add_custom_target( Surge.vst3 ALL )
    add_dependencies( Surge.vst3 surge-vst3 )
    add_custom_command(
      TARGET Surge.vst3
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST3 component"
      COMMAND pwd
      COMMAND ./scripts/linux/package-vst3.sh  ${CMAKE_BINARY_DIR}/libsurge-vst3.so
      )
    elseif( WIN32 )
     add_custom_target( Surge.vst3 ALL )
     add_dependencies( Surge.vst3 surge-vst3 )
     add_custom_command(
       TARGET Surge.vst3
       POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/target/vst3/Release

       COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/Release/surge-vst3.dll
                                        ${CMAKE_CURRENT_SOURCE_DIR}/target/vst3/Release/${WIN_DLL_BASENAME}.vst3
     )
   endif()

endif()

if( BUILD_VST2 )
  add_library(surge-vst2
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_VST2_SOURCES}
    ${SURGE_VST2_LIBRARY_SOURCES}
    )
  add_dependencies(surge-vst2 surge-shared)
  
  if( UNIX AND NOT APPLE )
    target_sources(surge-vst2 PRIVATE src/linux/linux-aeffguieditor.cpp )

    # Ideally we wouldn't define __cdecl on linux. It's not needed
    # Alas, the VST2 SDK has in aeffgui a _GNUC define which requires
    # it so without __cdecl blank defined on linux vst2, we don't get
    # reliable builds. We considered defining it just where used but
    # it is used also, indirectly, by the vst3 sdk, so the smallest
    # change is this diff plus this comment.
    target_compile_definitions( surge-vst2 PRIVATE "__cdecl=" )
  endif()
  if( WIN32 )
    target_link_options( surge-vst2 PUBLIC  "/DEF:..\\resources\\windows-vst2\\surge.def" )
  endif()

  target_compile_features(surge-vst2 PRIVATE cxx_std_17 )

  target_compile_definitions(surge-vst2
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_VST2=1
    VSTGUI_ENABLE_DEPRECATED_METHODS=0
    )

  target_compile_options(surge-vst2
    PRIVATE
    ${OS_COMPILE_OPTIONS}
    )

  target_include_directories(surge-vst2
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/vst2
    ${VST2SDK_DIR}
    )

  target_link_libraries(surge-vst2
    PRIVATE
    ${OS_LINK_LIBRARIES}
    )

  if( APPLE )
    add_custom_target( Surge.vst2 ALL )
    add_dependencies( Surge.vst2 surge-vst2 )
    add_custom_command(
      TARGET Surge.vst2
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up VST2 component"
      COMMAND ./scripts/macOS/package-vst.sh   ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libsurge-vst2.dylib
      )
   endif()
  if( UNIX AND NOT APPLE )
    add_custom_target( Surge.vst2 ALL )
    add_dependencies( Surge.vst2 surge-vst2)
    add_custom_command(
      TARGET Surge.vst2
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/products
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libsurge-vst2.so ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.so
      )
   endif()
   if( WIN32 )
     add_custom_target( surgev2-dll ALL )
     add_dependencies( surgev2-dll surge-vst2 )
     add_custom_command(
       TARGET surgev2-dll
       POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/target/vst2/Release

       COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/Release/surge-vst2.dll
                                        ${CMAKE_CURRENT_SOURCE_DIR}/target/vst2/Release/${WIN_DLL_BASENAME}.dll
     )
   endif()

endif()

if( BUILD_LV2 )
  add_library(surge-lv2
    SHARED
    ${SURGE_COMMON_SOURCES}
    ${SURGE_GUI_SOURCES}
    ${SURGE_OS_SOURCES}
    ${SURGE_OS_GUI_SOURCES}
    ${SURGE_LV2_SOURCES}
    ${SURGE_LV2_LIBRARY_SOURCES}
    )
  add_dependencies(surge-lv2 surge-shared)
  
  target_compile_features(surge-lv2 PRIVATE cxx_std_17 )

  target_compile_definitions(surge-lv2
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_LV2=1
    )

  target_compile_options(surge-lv2
    PRIVATE
    ${OS_COMPILE_OPTIONS}
    )

  target_include_directories(surge-lv2
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${SURGE_GUI_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/lv2
    libs/lv2
    )

  target_link_libraries(surge-lv2
    PRIVATE
    ${OS_LINK_LIBRARIES}
    )

  # FIXME - these rules have all sorts of problems with out-of-line
  add_custom_target( Surge.lv2 ALL )
  add_dependencies( Surge.lv2 surge-lv2 )
  if( APPLE )
    add_custom_command(
      TARGET Surge.lv2
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up LV2 component"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.lv2
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libsurge-lv2.dylib ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.lv2/Surge.dylib
      COMMAND python scripts/linux/generate-lv2-ttl.py products/Surge.lv2/Surge.dylib
      )
  elseif( UNIX AND NOT APPLE )
    add_custom_command(
      TARGET Surge.lv2
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Packaging up LV2 component"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.lv2
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/libsurge-lv2.so ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.lv2/Surge.so
      COMMAND python scripts/linux/generate-lv2-ttl.py ${CMAKE_CURRENT_SOURCE_DIR}/products/Surge.lv2/Surge.so
      )
  endif()

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
    src/headless/UnitTestsPARAM.cpp
    src/headless/UnitTestsTUN.cpp
    )
  add_dependencies(surge-headless surge-shared)
  
  target_compile_features(surge-headless PRIVATE cxx_std_17 )

  target_compile_definitions(surge-headless
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_HEADLESS=1
    LIBMIDIFILE=1
    )

  target_include_directories(surge-headless
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
    ${LIB_MIDIFILE_INCLUDES}
    ${OS_INCLUDE_DIRECTORIES}
    src/headless
    )

  find_package(LibSndFile ${PACKAGE_OPTION})
  if(NOT LIBSNDFILE_FOUND)
    message("-- LibSndFile not installed; building without wav support")
    message("-- You can 'brew install libsndfile' or 'apt-get install libsndfile1-dev'")
  else()
    target_compile_definitions(surge-headless
      PRIVATE
      LIBSNDFILE=1
      )
    target_link_libraries(surge-headless PRIVATE ${LIBSNDFILE_LIBRARIES})
    include_directories(surge-headless ${LIBSNDFILE_INCLUDE_DIRS})
  endif()

  target_link_libraries(surge-headless
    PRIVATE
    ${OS_LINK_LIBRARIES}
    )

  target_compile_options(surge-headless
    PRIVATE
    ${OS_COMPILE_OPTIONS}
    )


  if( UNIX AND NOT APPLE )
    find_package(Threads REQUIRED)
    target_sources( surge-headless PRIVATE src/linux/ConfigurationXml.S )
    target_link_libraries(surge-headless
      PRIVATE
      stdc++fs
      Threads::Threads
      )
  endif()
endif()

#
# INSTALL RULES - currently implemented on APPLE only
#
if( APPLE )
  add_custom_target( all-components )
  if( BUILD_VST2 )
    add_dependencies(all-components Surge.vst2 )
  endif()
  add_dependencies(all-components Surge.vst3 )
  add_dependencies(all-components Surge.component )
  add_dependencies(all-components surge-headless )

  add_custom_target( run-headless )
  add_dependencies( run-headless install-resources-local )
  add_dependencies( run-headless surge-headless )
  add_custom_command(
    TARGET run-headless
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_BINARY_DIR}/Release/surge-headless
    )

  add_custom_target( install-resources-local )
  add_custom_command(
    TARGET install-resources-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing local resources"
    COMMAND rsync -r --delete "resources/data/" "\${HOME}/Library/Application Support/Surge/"
    )

  add_custom_target( install-au-local )
  add_dependencies( install-au-local Surge.component )
  add_dependencies( install-au-local install-resources-local )
  add_custom_command(
    TARGET install-au-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing audio unit locally"
    COMMAND rsync -r --delete "products/Surge.component/" "\${HOME}/Library/Audio/Plug-Ins/Components/Surge.component/"
    )

  add_custom_target( validate-au )
  add_dependencies( validate-au install-au-local )
  add_custom_command(
    TARGET validate-au
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND auval -vt aumu VmbA
    )

  if( BUILD_VST2 )
    add_custom_target( install-vst2-local )
    add_dependencies( install-vst2-local Surge.vst2 )
    add_dependencies( install-vst2-local install-resources-local )
    add_custom_command(
      TARGET install-vst2-local
      POST_BUILD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND echo "Installing vst2 unit locally"
      COMMAND rsync -r --delete "products/Surge.vst/" "\${HOME}/Library/Audio/Plug-Ins/VST/Surge.vst/"
      )
  endif()

  add_custom_target( install-vst3-local )
  add_dependencies( install-vst3-local Surge.vst3 )
  add_dependencies( install-vst3-local install-resources-local )
  add_custom_command(
    TARGET install-vst3-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing vst3 unit locally"
    COMMAND rsync -r --delete "products/Surge.vst3/" "\${HOME}/Library/Audio/Plug-Ins/VST3/Surge.vst3/"
    )

  add_custom_target( install-everything-local )
  add_dependencies( install-everything-local install-au-local )
  add_dependencies( install-everything-local install-vst3-local )
  if( BUILD_VST2 )
    add_dependencies( install-everything-local install-vst2-local )
  endif()



endif()
