# Surge CMake file
# Exports targets for each of our builds
#

cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "Build for 10.9")
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

  # Add AVX support
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("
#if defined(__x86_64__) || defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #ifndef __AVX__
  #error
  #endif
#endif
    int main() {}" COMPILER_HAS_AVX_OR_IS_ARM)
  if (NOT COMPILER_HAS_AVX_OR_IS_ARM)
    message(STATUS "Holding off on AVX support. See #4479 for the strategy" )
    # add_compile_options("-mavx")
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
        -Wno-return-local-addr # squelch sqlite3 error: function may return address of local variable
    )
    add_link_options("-Wl,-z,noexecstack" )
  endif()
endif()

if(MSVC)
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

    # /arch:AVX

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

if( NOT SURGE_COPY_AFTER_BUILD )
  set( SURGE_COPY_AFTER_BUILD False )
else()
  message( STATUS "Surge Copy After Build activated" )
endif()

if( SURGE_ALTERNATE_JUCE )
  message( STATUS "Using *Alternate* JUCE from ${SURGE_ALTERNATE_JUCE}" )
  add_subdirectory( ${SURGE_ALTERNATE_JUCE} ${CMAKE_BINARY_DIR}/alternate-juce  EXCLUDE_FROM_ALL)
  set( SURGE_JUCE_LOCATION ${SURGE_ALTERNATE_JUCE} )
else()
  message( STATUS "Using JUCE from submodule libs/JUCE" )
  add_subdirectory( libs/JUCE EXCLUDE_FROM_ALL )
  set( SURGE_JUCE_LOCATION ${CMAKE_SOURCE_DIR}/libs/JUCE )
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
add_subdirectory(libs/tuning-library EXCLUDE_FROM_ALL)
add_subdirectory(libs/sqlite-3.23.3)

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
  surge::sqlite
  surge::filesystem
  surge::tinyxml
  surge::oddsound-mts
  samplerate
  surge-shared-binary
  tuning-library
)

if( NOT LINUX_ARM_CROSSCOMPILE )
  add_subdirectory(libs/LuaJitLib)
  target_link_libraries(surge-shared PUBLIC luajit-5.1)
endif()

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
  src/common/dsp/effects/BBDEnsembleEffect.cpp
  src/common/dsp/effects/ChorusEffectImpl.h
  src/common/dsp/effects/CombulatorEffect.cpp
  src/common/dsp/effects/ConditionerEffect.cpp
  src/common/dsp/effects/DistortionEffect.cpp
  src/common/dsp/effects/DelayEffect.cpp
  src/common/dsp/effects/FrequencyShifterEffect.cpp
  src/common/dsp/effects/FlangerEffect.cpp
  src/common/dsp/effects/GraphicEQ11BandEffect.cpp
  src/common/dsp/effects/NimbusEffect.cpp
  src/common/dsp/effects/ParametricEQ3BandEffect.cpp
  src/common/dsp/effects/PhaserEffect.cpp
  src/common/dsp/effects/ResonatorEffect.cpp
  src/common/dsp/effects/Reverb1Effect.cpp
  src/common/dsp/effects/Reverb2Effect.cpp
  src/common/dsp/effects/RingModulatorEffect.cpp
  src/common/dsp/effects/RotarySpeakerEffect.cpp
  src/common/dsp/effects/TreemonsterEffect.cpp
  src/common/dsp/effects/VocoderEffect.cpp
  src/common/dsp/effects/airwindows/AirWindowsEffect.cpp
  src/common/dsp/effects/airwindows/AirWindowsEffect.h
  src/common/dsp/effects/chowdsp/CHOWEffect.cpp
  src/common/dsp/effects/chowdsp/ExciterEffect.cpp
  src/common/dsp/effects/chowdsp/NeuronEffect.cpp
  src/common/dsp/effects/chowdsp/TapeEffect.cpp
  src/common/dsp/effects/chowdsp/bbd_utils/BBDDelayLine.cpp
  src/common/dsp/effects/chowdsp/exciter/LevelDetector.cpp
  src/common/dsp/effects/chowdsp/shared/DelayLine.cpp
  src/common/dsp/effects/chowdsp/tape/ChewProcessor.cpp
  src/common/dsp/effects/chowdsp/tape/DegradeProcessor.cpp
  src/common/dsp/effects/chowdsp/tape/HysteresisProcessing.cpp
  src/common/dsp/effects/chowdsp/tape/HysteresisProcessor.cpp
  src/common/dsp/effects/chowdsp/tape/LossFilter.cpp
  src/common/dsp/effects/chowdsp/tape/ToneControl.cpp
  src/common/dsp/filters/AllpassFilter.h
  src/common/dsp/filters/BiquadFilter.cpp
  src/common/dsp/filters/BiquadFilterSSE2.cpp
  src/common/dsp/filters/DiodeLadder.cpp
  src/common/dsp/filters/FilterCoefficientMaker.cpp
  src/common/dsp/filters/K35Filter.cpp
  src/common/dsp/filters/NonlinearFeedback.cpp
  src/common/dsp/filters/NonlinearStates.cpp
  src/common/dsp/filters/OBXDFilter.cpp
  src/common/dsp/filters/ThreelerFilter.cpp
  src/common/dsp/filters/VectorizedSVFilter.cpp
  src/common/dsp/filters/VintageLadders.cpp
  src/common/dsp/oscillators/AliasOscillator.cpp
  src/common/dsp/oscillators/AudioInputOscillator.cpp
  src/common/dsp/oscillators/ClassicOscillator.cpp
  src/common/dsp/oscillators/FM2Oscillator.cpp
  src/common/dsp/oscillators/FM3Oscillator.cpp
  src/common/dsp/oscillators/ModernOscillator.cpp
  src/common/dsp/oscillators/OscillatorBase.h
  src/common/dsp/oscillators/OscillatorCommonFunctions.h
  src/common/dsp/oscillators/SampleAndHoldOscillator.cpp
  src/common/dsp/oscillators/SineOscillator.cpp
  src/common/dsp/oscillators/StringOscillator.cpp
  src/common/dsp/oscillators/TwistOscillator.cpp
  src/common/dsp/oscillators/WavetableOscillator.cpp
  src/common/dsp/oscillators/WindowOscillator.cpp
  src/common/dsp/modulators/ADSRModulationSource.h
  src/common/dsp/modulators/FormulaModulationHelper.cpp
  src/common/dsp/modulators/LFOModulationSource.cpp
  src/common/dsp/modulators/MSEGModulationHelper.cpp
  src/common/dsp/utilities/DSPUtils.cpp
  src/common/dsp/utilities/FastMath.h
  src/common/dsp/utilities/SSEComplex.h
  src/common/dsp/utilities/SSESincDelayLine.h
  src/common/dsp/utilities/LanczosResampler.cpp
  src/common/dsp/vembertech/basic_dsp.cpp
  src/common/dsp/vembertech/halfratefilter.cpp
  src/common/dsp/vembertech/lipol.cpp
  src/common/dsp/Effect.cpp
  src/common/dsp/Oscillator.cpp
  src/common/dsp/SurgeVoice.cpp
  src/common/dsp/QuadFilterChain.cpp
  src/common/dsp/QuadFilterUnit.cpp
  src/common/dsp/SurgeVoiceState.h
  src/common/dsp/Wavetable.cpp
  src/common/dsp/WavetableScriptEvaluator.cpp
  src/common/CPUFeatures.cpp
  src/common/DebugHelpers.cpp
        src/common/FxPresetAndClipboardManager.cpp
  src/common/LuaSupport.cpp
  src/common/ModulatorPresetManager.cpp
  src/common/Parameter.cpp
  src/common/PatchDB.cpp
  src/common/SkinModel.cpp
  src/common/SkinModelImpl.cpp
  src/common/SkinColors.cpp
  src/common/SurgePatch.cpp
  src/common/SurgeStorage.cpp
  src/common/UserDefaults.cpp
  src/common/WAVFileSupport.cpp

  libs/strnatcmp/strnatcmp.cpp
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
  src/gui/CScalableBitmap.cpp
  src/gui/RuntimeFont.cpp
  src/gui/SkinFontLoader.cpp
  src/gui/SkinImageMaps.h
  src/gui/SkinSupport.cpp
  src/gui/SurgeBitmaps.cpp
  src/gui/SurgeGUIEditor.cpp
  src/gui/SurgeGUIEditorHtmlGenerators.cpp
  src/gui/SurgeGUIEditorInfowindow.cpp
  src/gui/SurgeGUIEditorValueCallbacks.cpp
  src/gui/SurgeGUIUtils.cpp
  src/gui/SurgeJUCELookAndFeel.cpp
  src/gui/UIInstrumentation.cpp

  src/gui/overlays/AboutScreen.cpp
  src/gui/overlays/LuaEditors.cpp
  src/gui/overlays/MiniEdit.cpp
  src/gui/overlays/ModulationEditor.cpp
  src/gui/overlays/MSEGEditor.cpp
  src/gui/overlays/PatchDBViewer.cpp
  src/gui/overlays/TypeinParamEditor.cpp
  src/gui/overlays/OverlayWrapper.cpp
  src/gui/overlays/PatchStoreDialog.cpp

  src/gui/widgets/EffectChooser.cpp
  src/gui/widgets/LFOAndStepDisplay.cpp
  src/gui/widgets/MainFrame.cpp
  src/gui/widgets/MenuForDiscreteParams.cpp
  src/gui/widgets/MultiSwitch.cpp
  src/gui/widgets/ModulationSourceButton.cpp
  src/gui/widgets/ModulatableSlider.cpp
  src/gui/widgets/NumberField.cpp
  src/gui/widgets/OscillatorWaveformDisplay.cpp
  src/gui/widgets/ParameterInfowindow.cpp
  src/gui/widgets/PatchSelector.cpp
  src/gui/widgets/Switch.cpp
  src/gui/widgets/VerticalLabel.cpp
  src/gui/widgets/VuMeter.cpp
  src/gui/widgets/XMLConfiguredMenus.cpp
        )

# Includes and Compiler Flags
set(SURGE_COMMON_INCLUDES
  libs/
  libs/strnatcmp

  src/common
  src/common/dsp
  src/common/dsp/effects
  src/common/dsp/filters
  src/common/dsp/oscillators
  src/common/dsp/modulators
  src/common/dsp/utilities
  src/common/dsp/vembertech
  )

set(SURGE_GUI_INCLUDES
  src/gui
  )

### OS SPECIFIC SECTION
if( APPLE )
  set(SURGE_OS_SOURCES)

  set(SURGE_OS_GUI_SOURCES)

  set(OS_INCLUDE_DIRECTORIES
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
    )

  find_package(PkgConfig REQUIRED)
  if( NOT LINUX_ON_ARM )
    # pkg_check_modules(CURL REQUIRED libcurl)
  endif()

  if( LINUX_ON_ARM )
    set(ARCH_COMPILE_DEFINITIONS ARM_NEON=1 NOCURL=1)
    set(ARCH_INCLUDE_DIRECTORIES libs/simde)
  endif()

  set(OS_COMPILE_DEFINITIONS
    ${ARCH_COMPILE_DEFINITIONS}
    LINUX=1
    )

  set(OS_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}/lintemp

    ${ARCH_INCLUDE_DIRECTORIES}
    )

  set(OS_LINK_LIBRARIES_NOGUI
    pthread
    dl
    -Wl,--no-undefined
    )
  set(OS_LINK_LIBRARIES
    ${OS_LINK_LIBRARIES_NOGUI}
    )

    if (CMAKE_SYSTEM_NAME MATCHES "BSD")
      set(OS_LINK_LIBRARIES ${OS_LINK_LIBRARIES} execinfo)
    endif()

elseif( WIN32 )

  set(SURGE_OS_GUI_SOURCES
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

else()
  message(FATAL_ERROR "UNKNOWN OS. Please use lin mac or win" )
endif()

# Source Groups
source_group( "Libraries" REGULAR_EXPRESSION "libs/" )
source_group( "AirWindows" REGULAR_EXPRESSION "libs/airwindows/" )
source_group( "Surge Core" REGULAR_EXPRESSION "src/common/.*\.cpp" )
source_group( "Surge DSP" REGULAR_EXPRESSION "src/common/dsp" )
source_group( "Surge FX" REGULAR_EXPRESSION "src/common/dsp/effects" )
source_group( "Surge GUI" REGULAR_EXPRESSION "src/gui" )
source_group( "Generated Code" REGULAR_EXPRESSION "version.cpp" )
source_group( "AU Plugin" REGULAR_EXPRESSION "src/au" )
source_group( "Headless" REGULAR_EXPRESSION "src/headless" )
source_group( "Surge XT Juce" REGULAR_EXPRESSION "src/surge_synth_juce" )
source_group( "Surge FX Juce" REGULAR_EXPRESSION "src/surge_effects_bank" )

#
# SURGE Core
#

target_sources(surge-shared PRIVATE ${SURGE_SHARED_SOURCES}  ${SURGE_SYNTH_SOURCES} ${SURGE_OS_SHARED_SOURCES})
target_include_directories(surge-shared
        PRIVATE
        ${SURGE_COMMON_INCLUDES}
        ${OS_INCLUDE_DIRECTORIES}
        )
target_compile_definitions(surge-shared PRIVATE ${OS_COMPILE_DEFINITIONS} )

if( BUILD_HEADLESS )
  add_executable(surge-headless
    ${SURGE_OS_SOURCES}
    ${SURGE_GENERATED_SOURCES}
    src/headless/main.cpp
    src/headless/HeadlessUtils.cpp
    src/headless/Player.cpp
    src/headless/UnitTests.cpp
    src/headless/UnitTestUtilities.cpp
    src/headless/UnitTestsDSP.cpp
    src/headless/UnitTestsFLT.cpp
    src/headless/UnitTestsFX.cpp
    src/headless/UnitTestsINFRA.cpp
    src/headless/UnitTestsIO.cpp
    src/headless/UnitTestsMIDI.cpp
    src/headless/UnitTestsMOD.cpp
    src/headless/UnitTestsMSEG.cpp
    src/headless/UnitTestsPARAM.cpp
    src/headless/UnitTestsTUN.cpp
    src/headless/UnitTestsLUA.cpp
    src/headless/HeadlessNonTestFunctions.cpp
    src/headless/HeadlessNonTestFunctions.h)

  target_compile_definitions(surge-headless
    PRIVATE
    ${OS_COMPILE_DEFINITIONS}
    TARGET_HEADLESS=1
    $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
  )

  target_include_directories(surge-headless
    PRIVATE
    ${SURGE_COMMON_INCLUDES}
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

if(SURGE_XT_BUILD_AUV3)
  list(APPEND SURGE_JUCE_FORMATS AUv3)
endif()

if(JUCE_SUPPORTS_LV2)
  list(APPEND SURGE_JUCE_FORMATS LV2)
  message( STATUS "Including JUCE LV2 support. You will need a different JUCE than the submodule" )
  message( STATUS "Turning off deprecation warning to error since JUCE LV2 uses deprecated APIs" )
  add_compile_options(
          -Wno-error=deprecated-declarations
  )
endif()

set(SURGE_JUCE_ACCESSIBLE FALSE)
if(EXISTS ${SURGE_JUCE_LOCATION}/modules/juce_gui_basics/accessibility/juce_AccessibilityHandler.h )
  set(SURGE_JUCE_ACCESSIBLE TRUE)
  message( STATUS "Including JUCE Accessibility Support")
else()
  message( STATUS "No JUCE Accessibility Support in this version" )
endif()

set(SURGE_JUCE_HOST_CONTEXT FALSE)
if(EXISTS ${SURGE_JUCE_LOCATION}/modules/juce_audio_processors/processors/juce_AudioProcessorEditorHostContext.h )
  set(SURGE_JUCE_HOST_CONTEXT TRUE)
  message( STATUS "Including JUCE Host Context Support")
else()
  message( STATUS "No JUCE Host Context Support in this version" )
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

  if( WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
    set( SURGE_FX_PRODUCT_NAME "Surge XT Effects (32-bit)" )
  else()
    set( SURGE_FX_PRODUCT_NAME "Surge XT Effects" )
  endif()

  juce_add_plugin(surge-fx
          PRODUCT_NAME ${SURGE_FX_PRODUCT_NAME}
          COMPANY_NAME "Surge Synth Team"
          COMPANY_WEBSITE "https://surge-synth-team.org/"
          BUNDLE_ID "org.surge-synth-team.surge-xt-fx"
          PLUGIN_MANUFACTURER_CODE VmbA
          PLUGIN_CODE SFXT

          ICON_BIG "resources/surge_effects_bank/SurgeFXLogo.png"

          IS_SYNTH FALSE
          NEEDS_MIDI_INPUT FALSE
          NEEDS_MIDI_OUTPUT FALSE
          IS_MIDI_EFFECT FALSE

          COPY_PLUGIN_AFTER_BUILD ${SURGE_COPY_AFTER_BUILD}

          VST3_CATEGORIES Fx
          VST2_CATEGORY kPlugCategEffect
          AU_MAIN_TYPE kAudioUnitType_Effect

          LV2_URI https://surge-synthesizer.github.io/lv2/surge-xt-fx
          LV2_SHARED_LIBRARY_NAME SurgeXT_FX

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

          JUCE_COREGRAPHICS_DRAW_ASYNC=1

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

  if( WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
    set( SURGE_XT_PRODUCT_NAME "Surge XT (32-bit)" )
  else()
    set( SURGE_XT_PRODUCT_NAME "Surge XT" )
  endif()

  juce_add_plugin(surge-xt
          PRODUCT_NAME ${SURGE_XT_PRODUCT_NAME}
          COMPANY_NAME "Surge Synth Team"
          COMPANY_WEBSITE "https://surge-synth-team.org/"
          BUNDLE_ID "org.surge-synth-team.surge-xt"
          PLUGIN_MANUFACTURER_CODE VmbA
          PLUGIN_CODE SgXT

          ICON_BIG "resources/surge-xt/SurgeLogo.png"

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

  juce_generate_juce_header( surge-xt )

  file(GLOB SURGE_SYNTH_JUCE_RESOURCES_GLOB
          resources/classic-skin-svgs/*.svg
          resources/fonts/Lato*ttf
          resources/fonts/IndieFlower.ttf
          resources/fonts/FiraMono-Regular.ttf
          )

  juce_add_binary_data( surge-xt-binary
          SOURCES ${SURGE_SYNTH_JUCE_RESOURCES_GLOB}
          )
  set_target_properties(surge-xt-binary PROPERTIES
          POSITION_INDEPENDENT_CODE TRUE
          )

  if (${SURGE_JUCE_ACCESSIBLE})
    set( SURGE_JUCE_ACCESSIBLE_DEFINE 1)
  else()
    set( SURGE_JUCE_ACCESSIBLE_DEFINE 0)
  endif()

  if (${SURGE_JUCE_HOST_CONTEXT})
    set( SURGE_JUCE_HOST_CONTEXT_DEFINE 1)
  else()
    set( SURGE_JUCE_HOST_CONTEXT_DEFINE 0)
  endif()

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

          JUCE_CATCH_UNHANDLED_EXCEPTIONS=$<IF:$<CONFIG:DEBUG>,1,0>

          SURGE_JUCE_ACCESSIBLE=${SURGE_JUCE_ACCESSIBLE_DEFINE}
          SURGE_JUCE_HOST_CONTEXT=${SURGE_JUCE_HOST_CONTEXT_DEFINE}

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

  set(SURGE_SYNTH_JUCE_GUI_SOURCES
         ${SURGE_GUI_SOURCES}
          )

  target_sources(surge-xt PRIVATE
          src/surge_synth_juce/SurgeSynthEditor.cpp
          src/surge_synth_juce/SurgeSynthProcessor.cpp
          ${SURGE_GENERATED_SOURCES}
          ${SURGE_OS_SOURCES}

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
    if(TARGET surge-xt_AUv3)
      target_sources(surge-xt_AUv3 PRIVATE
              src/surge_synth_juce/SurgeSynthAUv3Extensions.cpp
              )
    endif()
  endif()

  target_link_libraries(surge-xt PUBLIC
          surge::airwindows
          surge::filesystem
          surge::tinyxml

          surge-shared

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

  # Really just a little convnience for BP on lin and win
  add_custom_target( surge-xt-run-standalone )
  add_dependencies( surge-xt-run-standalone surge-xt_Standalone )
  add_custom_command( TARGET surge-xt-run-standalone
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND "${SURGE_XT_OUTPUT_DIR}/Standalone/Surge XT" )

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

set( SURGE_XT_DIST_OUTPUT_DIR surge-xt-dist )
set( SURGE_XT_DIST_TARGET_BASE surge-xt-alpha-${SXTVER}-${SXTOS})

add_custom_command(TARGET surge-xt-distribution
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_TARGET_BASE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURGE_PRODUCT_DIR}/ ${SURGE_XT_DIST_TARGET_BASE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}
        )

if( WIN32 )
  # Windows gets the zips but only windows
  add_custom_command(TARGET surge-xt-distribution
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E tar cvf ${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}/surge-xt-${SXTOS}-${SXTVER}-pluginsonly.zip --format=zip ${SURGE_XT_DIST_TARGET_BASE}
          )
endif()

# Build OS specific installers
if( APPLE )
  add_custom_command(TARGET surge-xt-distribution
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E echo ${CMAKE_SOURCE_DIR}/installer_mac/make_installer.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          COMMAND ${CMAKE_SOURCE_DIR}/scripts/installer_mac/make_installer.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          )
endif()

if( UNIX AND NOT APPLE )
  add_custom_command(TARGET surge-xt-distribution
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E echo ${CMAKE_SOURCE_DIR}/installer_linux/make_deb.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          COMMAND ${CMAKE_SOURCE_DIR}/scripts/installer_linux/make_deb.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          )

add_custom_target(surge-xt-rpm)
add_dependencies(surge-xt-rpm surge-staged-assets)

  add_custom_command(TARGET surge-xt-rpm
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}
          COMMAND ${CMAKE_COMMAND} -E echo ${CMAKE_SOURCE_DIR}/installer_linux/make_rpm.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          COMMAND ${CMAKE_SOURCE_DIR}/scripts/installer_linux/make_rpm.sh "${SURGE_PRODUCT_DIR}" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" "${SXTVER}"
          )
endif()

if( WIN32 )
  find_program(SURGE_NUGET_EXE nuget.exe PATHS ENV "PATH")
  if(NOT SURGE_NUGET_EXE)
    message(STATUS "Cant find NUGET; cmake won't make an installer image with inno")
  else()
    message(STATUS "Using NUGET from ${SURGE_NUGET_EXE}" )
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
      add_custom_command(TARGET surge-xt-distribution
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${SURGE_NUGET_EXE} install Tools.InnoSetup -version 6.1.2
        COMMAND ${CMAKE_COMMAND} -E echo Tools.InnoSetup.6.1.2/tools/iscc.exe /DSURGE_SRC="${CMAKE_SOURCE_DIR}" /DSURGE_BIN="${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/scripts/installer_win/surge.iss"
        COMMAND Tools.InnoSetup.6.1.2/tools/iscc.exe /O"${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" /DSURGE_SRC="${CMAKE_SOURCE_DIR}" /DSURGE_BIN="${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/scripts/installer_win/surge-x86.iss"
      )
    else()
      add_custom_command(TARGET surge-xt-distribution
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${SURGE_NUGET_EXE} install Tools.InnoSetup -version 6.1.2
          COMMAND ${CMAKE_COMMAND} -E echo Tools.InnoSetup.6.1.2/tools/iscc.exe /DSURGE_SRC="${CMAKE_SOURCE_DIR}" /DSURGE_BIN="${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/scripts/installer_win/surge.iss"
          COMMAND Tools.InnoSetup.6.1.2/tools/iscc.exe /O"${CMAKE_BINARY_DIR}/${SURGE_XT_DIST_OUTPUT_DIR}" /DSURGE_SRC="${CMAKE_SOURCE_DIR}" /DSURGE_BIN="${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/scripts/installer_win/surge.iss"
        )
    endif()
  endif()
endif()

# Add CMake Install rules for LINUX only right now (mac and win use an installer)
if( UNIX AND NOT APPLE)
  if( BUILD_SURGE_XT)
    install(TARGETS surge-xt_Standalone DESTINATION bin)
    # Alas you would think you could just say
    # install(TARGETS surge-xt_VST3 DESTINATION lib/vst3 )
    # but you can't because surge-xt_VST3 points to the .so inside the VST3 bundle so instead say
    install( DIRECTORY "${SURGE_XT_OUTPUT_DIR}/VST3/Surge XT.vst3" DESTINATION lib/vst3 )
    if( JUCE_SUPPORTS_LV2 )
      install(TARGETS surge-xt_LV2 DESTINATION lib/lv2 )
    endif()
  endif()
  if( BUILD_SURGE_EFFECTS_BANK )
    install(TARGETS surge-fx_Standalone DESTINATION bin)
    # and again
    # install(TARGETS surge-fx_VST3 DESTINATION lib/vst3 )
    install( DIRECTORY "${SURGE_FX_OUTPUT_DIR}/VST3/Surge XT Effects.vst3" DESTINATION lib/vst3 )

    if( JUCE_SUPPORTS_LV2 )
      install(TARGETS surge-fx_LV2 DESTINATION lib/lv2)
    endif()
  endif()
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/data/ DESTINATION share/surge-xt)
endif()


if( ${BUILD_SURGE_PYTHON_BINDINGS} )
  message( STATUS "Building Surge Python bindings with pybind11" )
  add_subdirectory(libs/pybind11)
  set( PYSRCD src/python_bindings )
  set( PYSRC )
  pybind11_add_module(surgepy )

  target_sources( surgepy PRIVATE
          ${SURGE_OS_SOURCES}
          ${SURGE_GENERATED_SOURCES}

          ${PYSRCD}/surgepy.cpp
          src/headless/HeadlessUtils.cpp
          )

  target_compile_definitions(surgepy
          PRIVATE
          ${OS_COMPILE_DEFINITIONS}
          TARGET_HEADLESS=1
          $<IF:$<CONFIG:DEBUG>,BUILD_IS_DEBUG,BUILD_IS_RELEASE>=1
          )

  target_include_directories(surgepy
          PRIVATE
          ${SURGE_COMMON_INCLUDES}
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
          COMMAND rsync -r --delete "resources/data/" "\${HOME}/Library/Application Support/Surge XT/"
  )

  add_custom_target( install-resources-global )
  add_custom_command(
          TARGET install-resources-global
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "Installing global resources"
          COMMAND rsync -r --delete "resources/data/" "/Library/Application Support/Surge XT/"
  )

elseif( UNIX AND NOT APPLE )
  add_custom_target( install-resources-local )
  add_custom_command(
          TARGET install-resources-local
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "Installing local resources into ~/.local/share/surge-xt"
          COMMAND ${CMAKE_COMMAND} -E make_directory "\${HOME}/.local/share/surge-xt/"
          COMMAND rsync -r --delete "resources/data/" "\${HOME}/.local/share/surge-xt/"
  )

  add_custom_target( install-resources-global )
  add_custom_command(
          TARGET install-resources-global
          POST_BUILD
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND echo "Installing local resources into ${CMAKE_INSTALL_PREFIX}/share/surge-xt"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_INSTALL_PREFIX}/share/surge-xt"
          COMMAND rsync -r --delete "resources/data/" "${CMAKE_INSTALL_PREFIX}/share/surge-xt/"
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
