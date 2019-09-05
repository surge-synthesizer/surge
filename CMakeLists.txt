cmake_minimum_required(VERSION 3.10)
project(Surge VERSION 1.0.0 LANGUAGES CXX ASM)
#set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
find_package(LibMidiFile ${PACKAGE_OPTIONS})

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

set(SURGE_COMMON_INCLUDES
    libs/
    libs/filesystem
    libs/xml
    libs/strnatcmp
    src/common
    src/common/dsp
    src/common/thread
    src/common/vt_dsp
)

add_executable(surge-headless
    ${SURGE_COMMON_SOURCES}
    ${LIB_MIDIFILE_SOURCES}
   src/headless/main.cpp
   src/headless/UserInteractionsHeadless.cpp
   src/headless/LinkFixesHeadless.cpp
   src/headless/HeadlessUtils.cpp
   src/headless/Player.cpp
   src/headless/Stress.cpp   
)

target_compile_features(surge-headless
    PRIVATE
        cxx_std_17
)

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
