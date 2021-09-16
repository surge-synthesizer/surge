# vi:set sw=2 et:
project(surge-common)

surge_add_lib_subdirectory(airwindows)
surge_add_lib_subdirectory(eurorack)
surge_add_lib_subdirectory(filesystem)
surge_add_lib_subdirectory(fmt)
surge_add_lib_subdirectory(strnatcmp)
surge_add_lib_subdirectory(tinyxml)
surge_add_lib_subdirectory(oddsound-mts)
if(MINGW)
  set(HAVE_VISIBILITY 0 CACHE INTERNAL "Force-disable libsamplerate's visibility check on MinGW")
endif()
surge_add_lib_subdirectory(libsamplerate)
surge_add_lib_subdirectory(tuning-library)
surge_add_lib_subdirectory(sqlite-3.23.3)
surge_add_lib_subdirectory(LuaJitLib)

juce_add_binary_data(surge-common-binary
  NAMESPACE SurgeSharedBinary
  HEADER_NAME SurgeSharedBinary.h
  SOURCES
  ${CMAKE_SOURCE_DIR}/resources/surge-shared/configuration.xml
  ${CMAKE_SOURCE_DIR}/resources/surge-shared/README_UserArea.txt
  ${CMAKE_SOURCE_DIR}/resources/surge-shared/memoryWavetable.wt
  ${CMAKE_SOURCE_DIR}/resources/surge-shared/windows.wt
  ${CMAKE_SOURCE_DIR}/resources/surge-shared/paramdocumentation.xml
  ${CMAKE_SOURCE_DIR}/src/lua/surge_prelude.lua
  ${CMAKE_SOURCE_DIR}/src/lua/surge_prelude_test.lua
)

add_library(${PROJECT_NAME}
  CPUFeatures.cpp
  CPUFeatures.h
  DebugHelpers.cpp
  DebugHelpers.h
  FilterConfiguration.h
  FxPresetAndClipboardManager.cpp
  FxPresetAndClipboardManager.h
  LuaSupport.cpp
  LuaSupport.h
  ModulationSource.h
  ModulatorPresetManager.cpp
  ModulatorPresetManager.h
  Parameter.cpp
  Parameter.h
  PatchDB.cpp
  PatchDB.h
  SkinColors.cpp
  SkinColors.h
  SkinFonts.cpp
  SkinFonts.h
  SkinModel.cpp
  SkinModel.h
  SkinModelImpl.cpp
  StringOps.h
  SurgeParamConfig.h
  SurgePatch.cpp
  SurgeStorage.cpp
  SurgeStorage.h
  SurgeSynthesizer.cpp
  SurgeSynthesizer.h
  SurgeSynthesizerIO.cpp
  UnitConversions.h
  UserDefaults.cpp
  UserDefaults.h
  WAVFileSupport.cpp
  dsp/Effect.cpp
  dsp/Effect.h
  dsp/Oscillator.cpp
  dsp/Oscillator.h
  dsp/QuadFilterChain.cpp
  dsp/QuadFilterChain.h
  dsp/QuadFilterUnit.cpp
  dsp/QuadFilterUnit.h
  dsp/QuadFilterWaveshapers.cpp
  dsp/SurgeVoice.cpp
  dsp/SurgeVoice.h
  dsp/SurgeVoiceState.h
  dsp/Wavetable.cpp
  dsp/Wavetable.h
  dsp/WavetableScriptEvaluator.cpp
  dsp/WavetableScriptEvaluator.h
  dsp/effects/BBDEnsembleEffect.cpp
  dsp/effects/BBDEnsembleEffect.h
  dsp/effects/ChorusEffect.h
  dsp/effects/ChorusEffectImpl.h
  dsp/effects/CombulatorEffect.cpp
  dsp/effects/CombulatorEffect.h
  dsp/effects/ConditionerEffect.cpp
  dsp/effects/ConditionerEffect.h
  dsp/effects/DelayEffect.cpp
  dsp/effects/DelayEffect.h
  dsp/effects/DistortionEffect.cpp
  dsp/effects/DistortionEffect.h
  dsp/effects/FlangerEffect.cpp
  dsp/effects/FlangerEffect.h
  dsp/effects/FrequencyShifterEffect.cpp
  dsp/effects/FrequencyShifterEffect.h
  dsp/effects/GraphicEQ11BandEffect.cpp
  dsp/effects/GraphicEQ11BandEffect.h
  dsp/effects/ModControl.h
  dsp/effects/NimbusEffect.cpp
  dsp/effects/NimbusEffect.h
  dsp/effects/ParametricEQ3BandEffect.cpp
  dsp/effects/ParametricEQ3BandEffect.h
  dsp/effects/PhaserEffect.cpp
  dsp/effects/PhaserEffect.h
  dsp/effects/ResonatorEffect.cpp
  dsp/effects/ResonatorEffect.h
  dsp/effects/Reverb1Effect.cpp
  dsp/effects/Reverb1Effect.h
  dsp/effects/Reverb2Effect.cpp
  dsp/effects/Reverb2Effect.h
  dsp/effects/RingModulatorEffect.cpp
  dsp/effects/RingModulatorEffect.h
  dsp/effects/RotarySpeakerEffect.cpp
  dsp/effects/RotarySpeakerEffect.h
  dsp/effects/TreemonsterEffect.cpp
  dsp/effects/TreemonsterEffect.h
  dsp/effects/VocoderEffect.cpp
  dsp/effects/VocoderEffect.h
  dsp/effects/WaveShaperEffect.cpp
  dsp/effects/WaveShaperEffect.h
  dsp/effects/airwindows/AirWindowsEffect.cpp
  dsp/effects/airwindows/AirWindowsEffect.h
  dsp/effects/chowdsp/CHOWEffect.cpp
  dsp/effects/chowdsp/CHOWEffect.h
  dsp/effects/chowdsp/ExciterEffect.cpp
  dsp/effects/chowdsp/ExciterEffect.h
  dsp/effects/chowdsp/NeuronEffect.cpp
  dsp/effects/chowdsp/NeuronEffect.h
  dsp/effects/chowdsp/TapeEffect.cpp
  dsp/effects/chowdsp/TapeEffect.h
  dsp/effects/chowdsp/bbd_utils/BBDCompander.h
  dsp/effects/chowdsp/bbd_utils/BBDDelayLine.cpp
  dsp/effects/chowdsp/bbd_utils/BBDDelayLine.h
  dsp/effects/chowdsp/bbd_utils/BBDFilterBank.h
  dsp/effects/chowdsp/bbd_utils/BBDNonlin.h
  dsp/effects/chowdsp/exciter/LevelDetector.cpp
  dsp/effects/chowdsp/exciter/LevelDetector.h
  dsp/effects/chowdsp/shared/BilinearUtils.h
  dsp/effects/chowdsp/shared/DelayInterpolation.h
  dsp/effects/chowdsp/shared/DelayLine.cpp
  dsp/effects/chowdsp/shared/DelayLine.h
  dsp/effects/chowdsp/shared/FIRFIlter.h
  dsp/effects/chowdsp/shared/Oversampling.h
  dsp/effects/chowdsp/shared/Shelf.h
  dsp/effects/chowdsp/shared/SmoothedValue.h
  dsp/effects/chowdsp/shared/chowdsp_IIR.h
  dsp/effects/chowdsp/shared/omega.h
  dsp/effects/chowdsp/shared/wdf.h
  dsp/effects/chowdsp/shared/wdf_sse.h
  dsp/effects/chowdsp/tape/ChewDropout.h
  dsp/effects/chowdsp/tape/ChewProcessor.cpp
  dsp/effects/chowdsp/tape/ChewProcessor.h
  dsp/effects/chowdsp/tape/DegradeFilter.h
  dsp/effects/chowdsp/tape/DegradeNoise.h
  dsp/effects/chowdsp/tape/DegradeProcessor.cpp
  dsp/effects/chowdsp/tape/DegradeProcessor.h
  dsp/effects/chowdsp/tape/HysteresisProcessing.cpp
  dsp/effects/chowdsp/tape/HysteresisProcessing.h
  dsp/effects/chowdsp/tape/HysteresisProcessor.cpp
  dsp/effects/chowdsp/tape/HysteresisProcessor.h
  dsp/effects/chowdsp/tape/LossFilter.cpp
  dsp/effects/chowdsp/tape/LossFilter.h
  dsp/effects/chowdsp/tape/ToneControl.cpp
  dsp/effects/chowdsp/tape/ToneControl.h
  dsp/filters/AllpassFilter.h
  dsp/filters/BiquadFilter.cpp
  dsp/filters/BiquadFilter.h
  dsp/filters/BiquadFilterSSE2.cpp
  dsp/filters/DiodeLadder.cpp
  dsp/filters/DiodeLadder.h
  dsp/filters/FilterCoefficientMaker.cpp
  dsp/filters/FilterCoefficientMaker.h
  dsp/filters/K35Filter.cpp
  dsp/filters/K35Filter.h
  dsp/filters/NonlinearFeedback.cpp
  dsp/filters/NonlinearFeedback.h
  dsp/filters/NonlinearStates.cpp
  dsp/filters/NonlinearStates.h
  dsp/filters/OBXDFilter.cpp
  dsp/filters/OBXDFilter.h
  dsp/filters/ThreelerFilter.cpp
  dsp/filters/ThreelerFilter.h
  dsp/filters/VectorizedSVFilter.cpp
  dsp/filters/VectorizedSVFilter.h
  dsp/filters/VintageLadders.cpp
  dsp/filters/VintageLadders.h
  dsp/modulators/ADSRModulationSource.cpp
  dsp/modulators/ADSRModulationSource.h
  dsp/modulators/FormulaModulationHelper.cpp
  dsp/modulators/FormulaModulationHelper.h
  dsp/modulators/LFOModulationSource.cpp
  dsp/modulators/LFOModulationSource.h
  dsp/modulators/MSEGModulationHelper.cpp
  dsp/modulators/MSEGModulationHelper.h
  dsp/oscillators/AliasOscillator.cpp
  dsp/oscillators/AliasOscillator.h
  dsp/oscillators/AudioInputOscillator.cpp
  dsp/oscillators/AudioInputOscillator.h
  dsp/oscillators/ClassicOscillator.cpp
  dsp/oscillators/ClassicOscillator.h
  dsp/oscillators/FM2Oscillator.cpp
  dsp/oscillators/FM2Oscillator.h
  dsp/oscillators/FM3Oscillator.cpp
  dsp/oscillators/FM3Oscillator.h
  dsp/oscillators/ModernOscillator.cpp
  dsp/oscillators/ModernOscillator.h
  dsp/oscillators/OscillatorBase.h
  dsp/oscillators/OscillatorCommonFunctions.h
  dsp/oscillators/SampleAndHoldOscillator.cpp
  dsp/oscillators/SampleAndHoldOscillator.h
  dsp/oscillators/SineOscillator.cpp
  dsp/oscillators/SineOscillator.h
  dsp/oscillators/StringOscillator.cpp
  dsp/oscillators/StringOscillator.h
  dsp/oscillators/TwistOscillator.cpp
  dsp/oscillators/TwistOscillator.h
  dsp/oscillators/WavetableOscillator.cpp
  dsp/oscillators/WavetableOscillator.h
  dsp/oscillators/WindowOscillator.cpp
  dsp/oscillators/WindowOscillator.h
  dsp/utilities/DSPUtils.cpp
  dsp/utilities/DSPUtils.h
  dsp/utilities/FastMath.h
  dsp/utilities/LanczosResampler.cpp
  dsp/utilities/LanczosResampler.h
  dsp/utilities/SSEComplex.h
  dsp/utilities/SSESincDelayLine.h
  dsp/vembertech/basic_dsp.cpp
  dsp/vembertech/basic_dsp.h
  dsp/vembertech/halfratefilter.cpp
  dsp/vembertech/halfratefilter.h
  dsp/vembertech/lipol.cpp
  dsp/vembertech/lipol.h
  dsp/vembertech/portable_intrinsics.h
  dsp/vembertech/shared.h
  dsp/vembertech/vt_dsp.h
  dsp/vembertech/vt_dsp_endian.h
  globals.h
  resource.h
  version.cpp.in
  version.h
)

if(APPLE)
  target_compile_definitions(${PROJECT_NAME} PUBLIC MAC=1)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC surge::simde
    PRIVATE
    "-framework CoreServices"
    "-framework CoreFoundation"
    "-framework Foundation"
  )
elseif(UNIX)
  target_compile_definitions(${PROJECT_NAME} PUBLIC LINUX=1)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    pthread
    dl
    -Wl,--no-undefined
  )
  if(CMAKE_SYSTEM_NAME MATCHES "BSD")
    target_link_libraries(${PROJECT_NAME} PUBLIC execinfo)
  endif()
  if(LINUX_ON_ARM)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ARM_NEON=1)
    target_link_libraries(${PROJECT_NAME} PUBLIC surge::simde)
  endif()
elseif(WIN32)
  target_compile_definitions(${PROJECT_NAME} PUBLIC
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
endif()

option(SURGE_RELIABLE_VERSION_INFO "Update version info on every build (off: generate only at configuration time)" ON)
if(SURGE_RELIABLE_VERSION_INFO)
  add_custom_target(version-info BYPRODUCTS ${CMAKE_BINARY_DIR}/geninclude/version.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -D CMAKE_PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
                             -D CMAKE_PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
                             -D SURGESRC=${CMAKE_SOURCE_DIR} -D SURGEBLD=${CMAKE_BINARY_DIR}
                             -D AZURE_PIPELINE=${AZURE_PIPELINE}
                             -D WIN32=${WIN32}
                             -D CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                             -D CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}
                             -D CMAKE_CXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}
                             -P ${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake
  )
  add_dependencies(${PROJECT_NAME} version-info)
else()
  set(SURGESRC ${CMAKE_SOURCE_DIR})
  set(SURGEBLD ${CMAKE_BINARY_DIR})
  include(${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake)
endif()
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/geninclude/version.cpp)

add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC
  fmt
  luajit-5.1
  samplerate
  surge::airwindows
  surge::eurorack
  surge::filesystem
  surge::oddsound-mts
  surge::sqlite
  surge::strnatcmp
  surge::tinyxml
  surge-common-binary
  tuning-library
)
target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  dsp
  dsp/effects
  dsp/filters
  dsp/modulators
  dsp/oscillators
  dsp/utilities
  dsp/vembertech
)
