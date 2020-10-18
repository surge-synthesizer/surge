project(filesystem VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
  src/directory_iterator.cpp
  src/filesystem.cpp
  src/path.cpp
  src/recursive_directory_iterator.cpp
  src/util.h
  include/${PROJECT_NAME}/directory_entry.h
  include/${PROJECT_NAME}/directory_iterator.h
  include/${PROJECT_NAME}/filesystem.h
  include/${PROJECT_NAME}/filesystem_error.h
  include/${PROJECT_NAME}/path.h
  include/${PROJECT_NAME}/recursive_directory_iterator.h
)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC include)

target_sources(surge-tests INTERFACE src/tests.cpp)
