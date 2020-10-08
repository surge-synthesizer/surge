project(filesystem VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
  src/filesystem.cpp
  include/${PROJECT_NAME}/filesystem.h
)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
