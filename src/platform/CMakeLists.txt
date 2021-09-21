# vi:set sw=2 et:
project(surge-platform)

if(APPLE)
  add_library(${PROJECT_NAME})
  target_sources(${PROJECT_NAME} PRIVATE macos/cpp17-aligned-new.cpp)
else()
  # no sources yet on other platforms
  add_library(${PROJECT_NAME} INTERFACE)
endif()
