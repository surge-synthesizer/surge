# vi:set sw=2 et:
cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE STRING "Minimum macOS version")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if(NOT BUILD_TESTING)
  option(BUILD_TESTING "" OFF)
endif()

project(Surge VERSION 1.4.0 LANGUAGES C CXX ASM)

# Banner {{{
message(STATUS "It's Surge XT, folks! Version is ${PROJECT_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message(STATUS "CMake version is ${CMAKE_VERSION}")
message(STATUS "Compiler version is ${CMAKE_CXX_COMPILER_VERSION}")
math(EXPR SURGE_BITNESS "${CMAKE_SIZEOF_VOID_P} * 8" OUTPUT_FORMAT DECIMAL)
message(STATUS "Targeting ${SURGE_BITNESS}-bit configuration")
# }}}

# Make Sure the REpo is properly cloned {{{
set(SURGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if (NOT EXISTS "${SURGE_SOURCE_DIR}/libs/tuning-library/README.md")
  message(FATAL_ERROR "Cannot find the contents of the tuning-library submodule. This usually means"
          " that you haven't done a submodule update, so the repository will not build properly. "
          "To resolve this, please run `git submodule update --init --recursive` and re-run CMake!")
endif()

# }}}

# Fail on 32 bit except on windows {{{
option(SURGE_BUILD_32BIT_LINUX "Allow non-working surge build to run on 32bit lin" OFF)
if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  if (WIN32)
    message(STATUS "Windows 32 bit build is lightly tested")
  else()
    # If you hit this error and want to try and do a 32 bit build on your Linux variant
    # most likely you will need to conditionally ifdef out the Spring Reverb and a few
    # things. We would happily merge that conditional compilation. The straztegy is basically
    # add a 'is 32 bit linux' compile flag to surge common, use that to make the SpringReverb
    # a nullptr in Effects.cpp and skipped code in the C++, test. etc... The branch where you
    # can add this flag is where we add the -msse2 flag in the GNU/CLang branch below, to get
    # started
    message(WARNING "32 Bit Builds are only available on Windows.")
    message(WARNING "If you are building on an r-pi, install a 64 bit OS.")
    message(WARNING "If you want to help fix this, see the comment in CMakeLists.txt.")
    if (NOT ${SURGE_BUILD_32BIT_LINUX})
      message(FATAL_ERROR "Stopping compilation")
    else()
      message(WARNING "Compilation continuing anyway. Good luck!")
    endif()
  endif()
endif()
# }}}

# Global compiler/linker settings {{{
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND UNIX AND NOT APPLE AND NOT SURGE_SKIP_PIE_CHANGE)
  message(STATUS "Setting -no-pie on EXE flags; use SURGE_SKIP_PIE_CHANGE=TRUE to avoid" )
  set(CMAKE_EXE_LINKER_FLAGS "-no-pie")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  if (${SURGE_SANITIZE})
    message(STATUS "Sanitizer is on" )
  endif()
  # Any Clang or any GCC
  add_compile_options(
    -Wno-multichar
    # Targeting Windows with GCC/Clang is experimental
    $<$<NOT:$<OR:$<BOOL:${WIN32}>,$<BOOL:${SURGE_SKIP_WERROR}>>>:-Werror>

    # PE/COFF doesn't support visibility
    $<$<NOT:$<BOOL:${WIN32}>>:-fvisibility=hidden>
    # Inlines visibility is only relevant with C++
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<COMPILE_LANGUAGE:CXX>>:-fvisibility-inlines-hidden>

    # BP note: If you want to turn on llvm/gcc sanitize undo this and the link options below
    $<$<BOOL:${SURGE_SANITIZE}>:-fsanitize=address>
    $<$<BOOL:${SURGE_SANITIZE}>:-fsanitize=undefined>

    # Do *not* use the new, breaking char8_t UTF-8 bits in C++20.
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<VERSION_GREATER_EQUAL:${CMAKE_CXX_STANDARD},20>>:-fno-char8_t>
  )

  add_link_options(
    $<$<BOOL:${SURGE_SANITIZE}>:-fsanitize=address>
    $<$<BOOL:${SURGE_SANITIZE}>:-fsanitize=undefined>
  )

  # Enable SSE2 on x86-32 only. It's implied on x86-64 and N/A elsewhere.
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("#ifndef __i386__
    #error
    #endif
    int main() {}" SURGE_ARCH_I386)
    if(SURGE_ARCH_I386)
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
  if(NOT COMPILER_HAS_AVX_OR_IS_ARM)
    message(STATUS "Holding off on AVX support. See #4479 for the strategy")
    # add_compile_options("-mavx")
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Any Clang
    add_compile_options(
      -Wno-unused-command-line-argument
      -Wno-deprecated-declarations
      -Werror=inconsistent-missing-override
      -Werror=logical-op-parentheses
      -Werror=dynamic-class-memaccess
      -Werror=undefined-bool-conversion
      -Werror=bitwise-op-parentheses
      -Werror=pointer-bool-conversion
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "^AppleClang$")
      # Apple Clang only
      add_compile_options(
        -faligned-allocation
        -fasm-blocks
      )
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^GNU$")
    # GCC only
    add_compile_options(
      -Wformat-truncation=0 # squelch warning about snprintf truncating strings (see PR #3977)
      -Wno-free-nonheap-object # https://github.com/surge-synthesizer/surge/issues/4251
      -Wno-return-local-addr # squelch sqlite3 error: function may return address of local variable
      -Wno-error=restrict # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105651
      -Wno-date-time
    )
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
    # Set source and executable charsets to UTF-8
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    # Do *not* use the new, breaking char8_t UTF-8 bits in C++20.
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:char8_t->
    # amke msvc define __cplulsplus properly
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
  )
  add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()
# }}}
# Source Groups {{{
source_group("Libraries" REGULAR_EXPRESSION "libs/")
source_group("AirWindows" REGULAR_EXPRESSION "libs/airwindows/")
source_group("Surge Core" REGULAR_EXPRESSION "src/common/.*\.cpp")
source_group("Surge DSP" REGULAR_EXPRESSION "src/common/dsp/")
source_group("Surge FX" REGULAR_EXPRESSION "src/common/dsp/effects/")
source_group("Surge GUI" REGULAR_EXPRESSION "src/surge-xt/gui/")
source_group("Generated Code" REGULAR_EXPRESSION "version.cpp")
source_group("Headless" REGULAR_EXPRESSION "src/surge-testrunner/")
source_group("Surge XT Juce" REGULAR_EXPRESSION "src/surge-xt/")
source_group("Surge FX Juce" REGULAR_EXPRESSION "src/surge-fx/")
# }}}


option(SURGE_BUILD_TESTRUNNER "Build Surge unit test runner" ON)
if (${SURGE_BUILD_TESTRUNNER})
  message(STATUS "Enabling tests; compile with SURGE_BUILD_TESTRUNNER=OFF to skip")
  enable_testing()
endif()

add_subdirectory(resources)
add_subdirectory(src)

# CI Pipeline {{{
include(cmake/stage-extra-content.cmake)

# We have a special target here which the PR pipeline uses to make sure things
# are of the appropriate code quality. This allows us to write CMAKE rules which
# become linters and stuff. The code will run on macos in the pipeline. I suppose
# you could run it locally to, but really, you should know what you are doing if you
# do that. And I'll document it so you can know that when I use it earnest in 1.9
add_custom_target(code-quality-pipeline-checks)

# Check 1: The extra content is properly specified.
# add_dependencies(code-quality-pipeline-checks download-extra-content)

# Clang Format checks
set(CLANG_FORMAT_DIRS src)
set(CLANG_FORMAT_EXTS cpp h)
foreach(dir ${CLANG_FORMAT_DIRS})
  foreach(ext ${CLANG_FORMAT_EXTS})
    list(APPEND CLANG_FORMAT_GLOBS "':(glob)${dir}/**/*.${ext}'")
  endforeach()
endforeach()
add_custom_command(TARGET code-quality-pipeline-checks
  POST_BUILD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E echo About to check clang-format using clang-format-12
  COMMAND git ls-files -- ${CLANG_FORMAT_GLOBS} | xargs clang-format-12 --dry-run --Werror
  )
# }}}
