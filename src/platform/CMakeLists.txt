# vi:set sw=2 et:
project(surge-platform)

if(APPLE)
  add_library(${PROJECT_NAME}
    macos/cpp17-aligned-new.cpp
    )
elseif(WIN32)
  add_library(${PROJECT_NAME} INTERFACE)
elseif(UNIX)
  add_library(${PROJECT_NAME} INTERFACE)
endif()
