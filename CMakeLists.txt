# Surge CMake file
# Exports targets for each of our builds
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

project(Surge-XT VERSION 1.0.0.0 LANGUAGES C CXX ASM)

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

if( SURGE_ALTERNATE_JUCE )
  message( STATUS "Using *Alternate* JUCE from ${SURGE_ALTERNATE_JUCE}" )
  add_subdirectory( ${SURGE_ALTERNATE_JUCE} ${CMAKE_BINARY_DIR}/alternate-juce )
else()
  message( STATUS "Using JUCE from submodule libs/JUCE" )
  add_subdirectory( libs/JUCE )
endif()

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

juce_add_binary_data(surge-shared-binary
        NAMESPACE SurgeCoreBinary
        HEADER_NAME SurgeCoreBinary.h
        SOURCES
        resources/data/configuration.xml
        resources/data/windows.wt
        resources/data/paramdocumentation.xml)
set_target_properties(surge-shared-binary PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE
        )

target_link_libraries(surge-shared PUBLIC
  surge::airwindows
  surge::eurorack
  surge::filesystem
  surge::tinyxml
  surge::oddsound-mts
  samplerate
  surge-shared-binary
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

set( BUILD_SURGE_XT TRUE )
set( BUILD_SURGE_EFFECTS_BANK TRUE )
set( BUILD_HEADLESS TRUE )

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

### OS SPECIFIC SECTION
if( APPLE )
  set(SURGE_OS_SOURCES
      src/mac/objc_utils.mm
      )

  set(SURGE_OS_GUI_SOURCES
    src/mac/UserInteractionsMac.mm
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
  set(SURGE_OS_GUI_SOURCES
    src/linux/UserInteractionsLinux.cpp
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
    src/windows/UserInteractionsWin.cpp
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
source_group( "macOS" REGULAR_EXPRESSION "src/mac" )
source_group( "Windows" REGULAR_EXPRESSION "src/windows" )
source_group( "Surge XT Juce" REGULAR_EXPRESSION "src/surge_synth_juce" )
source_group( "Surge FX Juce" REGULAR_EXPRESSION "src/surge_effects_bank" )

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
    src/headless/HeadlessNonTestFunctions.cpp
    src/headless/HeadlessNonTestFunctions.h)

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

if (DEFINED ENV{VST2SDK_DIR})
  file(TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" JUCE_VST2_DIR)
  juce_set_vst2_sdk_path(${JUCE_VST2_DIR})
  set(SURGE_JUCE_FORMATS AU VST3 VST Standalone)
  message(STATUS "JUCE VST2 SDK Path is $ENV{VST2SDK_DIR}")
else()
  set(SURGE_JUCE_FORMATS AU VST3 Standalone)
endif()

if(JUCE_SUPPORTS_LV2)
  list(APPEND SURGE_JUCE_FORMATS LV2)
  message( STATUS "Including JUCE LV2 support. You will need a different JUCE than the submodule" )
  message( STATUS "Turning off deprecation warning to error since JUCE LV2 uses deprecated APIs" )
  add_compile_options(
          -Wno-error=deprecated-declarations
  )
endif()

set(JUCE_ASIO_SUPPORT FALSE)
if( BUILD_USING_MY_ASIO_LICENSE )
  message(STATUS "** BUILD WITH YOUR ASIO LICENSE **" )
  message(STATUS "   Downloading ASIO SDK from Steinberg" )
  message(STATUS "   The resulting Surge Standalones are not licensed for distribution" )

  add_custom_target(surge-get-local-asio)
  add_custom_command(
          TARGET surge-get-local-asio
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/asio
          COMMAND ${CMAKE_COMMAND} -D ASIO_SDK_DESTINATION=${CMAKE_BINARY_DIR}/asio -P cmake/get-asio.cmake
  )
  set( ASIOSDK_DIR ${CMAKE_BINARY_DIR}/asio/asiosdk )
  set( JUCE_ASIO_SUPPORT TRUE )
  add_dependencies(surge-shared surge-get-local-asio)
endif()

message(STATUS "Building Surge JUCE as ${SURGE_JUCE_FORMATS}")
if (DEFINED ENV{ASIOSDK_DIR})
  file(TO_CMAKE_PATH "$ENV{ASIOSDK_DIR}" ASIOSDK_DIR)
  message(STATUS "ASIO SDK found at ${ASIOSDK_DIR}")
  set(JUCE_ASIO_SUPPORT TRUE)
endif()


set(SURGE_PRODUCT_DIR ${CMAKE_BINARY_DIR}/surge_xt_products)
file(MAKE_DIRECTORY ${SURGE_PRODUCT_DIR})
add_custom_target(surge-staged-assets)

if( BUILD_SURGE_EFFECTS_BANK )
  # LinkOrder on Linux matters so make a little lib which contains UIJUCE so I can link
  # it AFTER surge-shared (since the libs come after the source in the JUCE build of course)
  # FIXME of course that this should go away in surge-xt. These are also split now for
  # XT vs FX since FX is production code in 19 and XT is not
  add_library(surge-juce-userint-fx src/surge_effects_bank/UserInteractionsJUCE.cpp )
  target_include_directories(surge-juce-userint-fx PRIVATE ${SURGE_COMMON_INCLUDES})

  juce_add_plugin(surge-fx
          PRODUCT_NAME "SurgeEffectsBank XT"
          COMPANY_NAME "Surge Synth Team"
          BUNDLE_ID "org.surge-synth-team.surge-fx-xt"
          PLUGIN_MANUFACTURER_CODE VmbA
          PLUGIN_CODE SFXT

          IS_SYNTH FALSE
          NEEDS_MIDI_INPUT FALSE
          NEEDS_MIDI_OUTPUT FALSE
          IS_MIDI_EFFECT FALSE

          LV2_URI https://surge-synthesizer.github.io/surge-fx-xt

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

          JUCE_WASAPI=1
          JUCE_DIRECTSOUND=1

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

  add_dependencies(surge-staged-assets Surge-Effects-Bank-Packaged)
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

          LV2_URI https://surge-synthesizer.github.io/surge-xt

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

          JUCE_WASAPI=1
          JUCE_DIRECTSOUND=1

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

  if(JUCE_SUPPORTS_LV2)
    target_sources(surge-xt_LV2 PRIVATE
            src/surge_synth_juce/SurgeSynthLV2Extensions.cpp
            )
  endif()
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

  # add_dependencies(surge-juce-pipeline-targets surge-xt_Standalone)

  get_target_property( SURGE_XT_OUTPUT_DIR surge-xt RUNTIME_OUTPUT_DIRECTORY )

  add_custom_target( Surge-XT-Packaged ALL )
  add_dependencies( Surge-XT-Packaged surge-xt_All )
  add_dependencies( Surge-XT-Packaged surge-xt_VST3 )
  add_dependencies( Surge-XT-Packaged surge-xt_Standalone )

  add_custom_command(
          TARGET Surge-XT-Packaged
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "Re-locating VST3 and Standalone components"
          COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_XT_OUTPUT_DIR}/VST3 ${SURGE_PRODUCT_DIR}/
          COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_XT_OUTPUT_DIR}/Standalone ${SURGE_PRODUCT_DIR}/
  )

  if( APPLE )
    add_dependencies( Surge-XT-Packaged surge-xt_AU )

    add_custom_command(
            TARGET Surge-XT-Packaged
            POST_BUILD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND echo "Re-locating AU components"
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_XT_OUTPUT_DIR}/AU ${SURGE_PRODUCT_DIR}/
    )
  endif()
  add_dependencies(surge-staged-assets Surge-XT-Packaged)
endif()

add_custom_target(surge-staged-nonbuilt-assets)
add_custom_command(TARGET surge-staged-nonbuilt-assets
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/resources/surge-xt/README.txt ${SURGE_PRODUCT_DIR}
        )
add_dependencies(surge-staged-assets surge-staged-nonbuilt-assets)

add_custom_target(surge-xt-distribution)
add_dependencies(surge-xt-distribution surge-staged-assets)

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
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    set( SXTOS "win32" )
  else()
    set( SXTOS "win64" )
  endif()
endif()

set( SURGE_XT_DIST_TARGET_BASE surge-xt-alpha-${SXTVER}-${SXTOS})

add_custom_command(TARGET surge-xt-distribution
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_TARGET_BASE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_PRODUCT_DIR}/ ${SURGE_XT_DIST_TARGET_BASE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/surge-xt-zip
        COMMAND ${CMAKE_COMMAND} -E tar cvf ${CMAKE_BINARY_DIR}/surge-xt-zip/surge-xt-alpha-${SXTVER}-${SXTOS}.zip --format=zip ${SURGE_XT_DIST_TARGET_BASE}
        )

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

# This is the custom target for the CI pipeline
add_custom_target(ci-pull-req)
add_dependencies(ci-pull-req surge-xt_All)
add_dependencies(ci-pull-req surge-fx_All)


#
# INSTALL RULES - currently implemented on APPLE and LINUX only
#
if( APPLE )
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
