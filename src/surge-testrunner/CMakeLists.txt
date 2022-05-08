# vi:set sw=2 et:
project(surge-testrunner)

surge_add_lib_subdirectory(catch2)

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
  main.cpp
  )

target_link_libraries(${PROJECT_NAME} PRIVATE
  samplerate
  surge-lua-src
  surge::catch2
  surge::surge-common
  )
