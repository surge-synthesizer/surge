# Keep pinned upstream sources untouched. Mirror the include tree so sibling
# includes (e.g. SmoothingStrategies -> Lag.h) use the same implementation.
set(surge_sst_source "${CMAKE_SOURCE_DIR}/libs/sst/sst-basic-blocks/include")
set(surge_sst_overlay "${CMAKE_BINARY_DIR}/wasm-sst-basic-blocks/include")
set(surge_lag_relative "sst/basic-blocks/dsp/Lag.h")
file(SHA256 "${surge_sst_source}/${surge_lag_relative}" surge_lag_hash)
if(NOT surge_lag_hash STREQUAL "5d228510acb84af64565b1b3b8d41136d512139e905d3db844e42b26d2c5b739")
  message(FATAL_ERROR "SST Lag.h changed: review the Wasm arithmetic overlay before updating its digest")
endif()
file(COPY "${surge_sst_source}/" DESTINATION "${surge_sst_overlay}")
file(READ "${surge_sst_source}/${surge_lag_relative}" surge_lag_source)
# Apple ARM64 contracts this expression into a fused multiply-add. Separate
# Wasm operations drift at steady state for delay lengths such as 11019 samples,
# changing interpolation phase and audible output. Explicit fma preserves that
# reference recurrence without relaxing audio-comparison tolerances.
string(REPLACE
  "inline void process() { v = v * lpinv + target_v * lp; }"
  "inline void process() { v = std::fma(v, lpinv, target_v * lp); }"
  surge_lag_patched "${surge_lag_source}")
file(CONFIGURE OUTPUT "${surge_sst_overlay}/${surge_lag_relative}"
  CONTENT "${surge_lag_patched}" @ONLY)
target_include_directories(sst-basic-blocks BEFORE INTERFACE "${surge_sst_overlay}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${surge_sst_source}/${surge_lag_relative}")

# Native ARM64 FromDirect emits fmul followed by fmadd for coefficient smoothing.
# Keep that recurrence, including rounding order, in the browser filter bank.
set(surge_filter_source "${CMAKE_SOURCE_DIR}/libs/sst/sst-filters/include")
set(surge_filter_overlay "${CMAKE_BINARY_DIR}/wasm-sst-filters/include")
set(surge_filter_relative "sst/filters/FilterCoefficientMaker_Impl.h")
file(SHA256 "${surge_filter_source}/${surge_filter_relative}" surge_filter_hash)
if(NOT surge_filter_hash STREQUAL "9a43b93a373e81de396ffff3b13afc6571a9d9a891d7b2613b98c3424cb56fd6")
  message(FATAL_ERROR "SST coefficient maker changed: review the Wasm arithmetic overlay")
endif()
file(COPY "${surge_filter_source}/" DESTINATION "${surge_filter_overlay}")
file(READ "${surge_filter_source}/${surge_filter_relative}" surge_filter_source_text)
string(REPLACE
  "tC[i] = (1.f - smooth) * tC[i] + smooth * N[i];"
  "tC[i] = std::fma(1.f - smooth, tC[i], smooth * N[i]);"
  surge_filter_patched "${surge_filter_source_text}")
file(CONFIGURE OUTPUT "${surge_filter_overlay}/${surge_filter_relative}"
  CONTENT "${surge_filter_patched}" @ONLY)
target_include_directories(sst-filters BEFORE INTERFACE "${surge_filter_overlay}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${surge_filter_source}/${surge_filter_relative}")

# Tri-pole's nonlinear solver uses SIMDe's unrefined NEON rsqrt estimate on
# the ARM64 reference. Wasm's exact reciprocal square root changes its output.
# Scope the compatibility operation to this filter; other filters retain their
# existing arithmetic. Never modify the pinned upstream checkout.
set(surge_tripole_relative "sst/filters/TriPoleFilter.h")
file(SHA256 "${surge_filter_source}/${surge_tripole_relative}" surge_tripole_hash)
if(NOT surge_tripole_hash STREQUAL "6f88f5ea042780b13ae8d7f487ab4cf7e7e027792c6fd0722b51701feca614a4")
  message(FATAL_ERROR "SST TriPoleFilter.h changed: review the Wasm estimate overlay")
endif()
file(READ "${surge_filter_source}/${surge_tripole_relative}" surge_tripole_source)
string(REPLACE
  "#include \"QuadFilterUnit.h\""
  "#include \"QuadFilterUnit.h\"\n#include \"SurgeReciprocalSquareRootEstimate.h\""
  surge_tripole_patched "${surge_tripole_source}")
string(REPLACE
  "vtmp = SIMD_MM(rsqrt_ps)(vtmp2);"
  "alignas(16) float lanes[4];\n    SIMD_MM(store_ps)(lanes, vtmp2);\n    for (auto &lane : lanes)\n        lane = Surge::DSP::reciprocalSquareRootEstimate(lane);\n    vtmp = SIMD_MM(load_ps)(lanes);"
  surge_tripole_patched "${surge_tripole_patched}")
file(CONFIGURE OUTPUT "${surge_filter_overlay}/${surge_tripole_relative}"
  CONTENT "${surge_tripole_patched}" @ONLY)
configure_file(
  "${CMAKE_SOURCE_DIR}/src/common/dsp/vembertech/ReciprocalSquareRootEstimate.h"
  "${surge_filter_overlay}/sst/filters/SurgeReciprocalSquareRootEstimate.h" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${surge_filter_source}/${surge_tripole_relative}")
