# vi:set sw=2 et:
project(surge-platform)

add_library(${PROJECT_NAME}
  include/platform/Paths.h
  src/Paths.cpp
  )
if(APPLE)
  target_sources(${PROJECT_NAME} PRIVATE
    macos/cpp17-aligned-new.cpp
    unix/Paths.cpp
    )
elseif(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE windows/Paths.cpp)
elseif(UNIX)
  target_sources(${PROJECT_NAME} PRIVATE unix/Paths.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC sst-plugininfra::filesystem)
target_include_directories(${PROJECT_NAME} PUBLIC include)
