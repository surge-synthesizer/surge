project(tinyxml)

add_library(${PROJECT_NAME}
  src/tinyxmlparser.cpp
  src/tinyxml.cpp
  src/tinyxmlerror.cpp
  include/${PROJECT_NAME}/tinyxml.h
)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC surge::filesystem)
target_compile_definitions(${PROJECT_NAME} PUBLIC TIXML_USE_STL)
target_include_directories(${PROJECT_NAME} PUBLIC include)
