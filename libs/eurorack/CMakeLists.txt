project(eurorack VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
        eurorack/clouds/dsp/correlator.cc
        eurorack/clouds/dsp/granular_processor.cc
        eurorack/clouds/dsp/mu_law.cc
        eurorack/clouds/dsp/pvoc/frame_transformation.cc
        eurorack/clouds/dsp/pvoc/phase_vocoder.cc
        eurorack/clouds/dsp/pvoc/stft.cc
        eurorack/clouds/resources.cc

        eurorack/stmlib/dsp/units.cc
        eurorack/stmlib/dsp/atan.cc

        eurorack/stmlib/utils/random.cc
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  TEST=1
  $<$<BOOL:${WIN32}>:_USE_MATH_DEFINES>
)

if(MSVC)
  # MSVC and pretenders
  target_compile_options(${PROJECT_NAME} PRIVATE /FImsvc_gcc_defines.h)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_NAME} special_include)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
