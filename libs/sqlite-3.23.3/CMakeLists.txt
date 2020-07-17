project(sqlite VERSION 3.23.3 LANGUAGES C)

message(STATUS "Including local sqlite" )
add_library(${PROJECT_NAME} sqlite3.c sqlite3.h)
add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE .)
