# vi:set sw=2 et:
project(surge-testrunner)

surge_add_lib_subdirectory(catch2_v3)

add_executable(${PROJECT_NAME}
  HeadlessNonTestFunctions.cpp
  HeadlessNonTestFunctions.h
  HeadlessPluginLayerProxy.h
  HeadlessUtils.cpp
  HeadlessUtils.h
  Player.cpp
  Player.h
  UnitTestUtilities.cpp
  UnitTestUtilities.h
  UnitTests.cpp
  UnitTestsDSP.cpp
  UnitTestsFLT.cpp
  UnitTestsFX.cpp
  UnitTestsINFRA.cpp
  UnitTestsIO.cpp
  UnitTestsLUA.cpp
  UnitTestsMIDI.cpp
  UnitTestsMOD.cpp
  UnitTestsMSEG.cpp
  UnitTestsNOTEID.cpp
  UnitTestsPARAM.cpp
  UnitTestsQUERY.cpp
  UnitTestsTUN.cpp
  UnitTestsVOICE.cpp
  main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge-lua-src
  surge::catch2_v3
  surge::surge-common
  juce::juce_audio_basics
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

# Set stack size to 16MB for MSVC Debug builds only to prevent stack overflow in "Basic Formula Evaluation" Lua tests
if (MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Increasing stack size for MSVC Debug build to accommodate Formula Evaluation Lua tests")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /STACK:0x1000000")
endif()

message(STATUS "Using CatchDiscoverTests on ${PROJECT_NAME}" )
catch_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${SURGE_SOURCE_DIR})