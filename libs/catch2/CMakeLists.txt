project(catch2 VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE include/${PROJECT_NAME}/catch2.hpp)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_definitions(${PROJECT_NAME} INTERFACE $<$<BOOL:${APPLE}>:CATCH_CONFIG_NO_CPP17_UNCAUGHT_EXCEPTIONS>)
