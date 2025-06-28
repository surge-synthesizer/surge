# vi:set sw=2 et:
project(surge-platform)

if(APPLE)
  add_library(${PROJECT_NAME}
    macos/cpp17-aligned-new.cpp
    )
  target_include_directories(${PROJECT_NAME} PUBLIC .)
elseif(WIN32)
  add_library(${PROJECT_NAME} INTERFACE)
  target_include_directories(${PROJECT_NAME} INTERFACE .)
elseif(UNIX)
  add_library(${PROJECT_NAME} INTERFACE)
  target_include_directories(${PROJECT_NAME} INTERFACE .)
endif()

