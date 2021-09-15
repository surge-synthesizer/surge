# vi:set sw=2 et:
project(strnatcmp)

add_library(${PROJECT_NAME}
  strnatcmp.cpp
  strnatcmp.h
)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
