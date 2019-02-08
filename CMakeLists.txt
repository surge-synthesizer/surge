cmake_minimum_required(VERSION 3.5)
project(Surge VERSION 1.0.0 LANGUAGES CXX)

set(SURGE_COMMON_SOURCES
    src/common/dsp/effect/ConditionerEffect.cpp
    src/common/dsp/effect/DistortionEffect.cpp
    src/common/dsp/effect/DualDelayEffect.cpp
    src/common/dsp/effect/Effect.cpp
    src/common/dsp/effect/FreqshiftEffect.cpp
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
    src/common/Sample.cpp
    src/common/SampleLoadRiffWave.cpp
    src/common/SurgeError.cpp
    src/common/SurgePatch.cpp
    src/common/SurgeStorage.cpp
    src/common/SurgeStorageLoadWavetable.cpp
    src/common/SurgeSynthesizer.cpp
    src/common/SurgeSynthesizerIO.cpp
    src/common/UserDefaults.cpp
    libs/xml/tinyxml.cpp
    libs/xml/tinyxmlerror.cpp
    libs/xml/tinyxmlparser.cpp
    libs/filesystem/filesystem.cpp
)

set(SURGE_COMMON_INCLUDES
    libs/
    libs/filesystem
    libs/xml
    src/common
    src/common/dsp
    src/common/gui
    src/common/thread
    src/common/vt_dsp
    vst3sdk
    vstgui.surge
)

add_executable(surge-headless
    ${SURGE_COMMON_SOURCES}
   src/headless/main.cpp
   src/headless/DisplayInfoHeadless.cpp
   src/headless/UserInteractionsHeadless.cpp
   src/headless/LinkFixesHeadless.cpp

   src/headless/HeadlessApps.cpp
   src/headless/HeadlessTools.cpp
)

target_compile_features(surge-headless
    PRIVATE
        cxx_std_17
)

target_compile_definitions(surge-headless
    PRIVATE
	    TARGET_HEADLESS=1
)

target_include_directories(surge-headless
    PRIVATE
        ${SURGE_COMMON_INCLUDES}
        src/headless
)

if (APPLE)
    target_compile_definitions(surge-headless
        PRIVATE
            MAC=1
            MAC_COCOA=1
            COCOA=1
            OBJC_OLD_DISPATCH_PROTOTYPES=1
    )

    target_compile_options(surge-headless
        PRIVATE
            -msse2
            "-D_aligned_malloc(x,a)=malloc(x)"
            "-D_aligned_free(x)=free(x)"
    )

    target_include_directories(surge-headless
        PRIVATE
            ${SURGE_COMMON_INCLUDES}
            src/mac
    )

    target_link_libraries(surge-headless
        "-framework ApplicationServices"
        "-framework CoreFoundation"
    )
endif()
