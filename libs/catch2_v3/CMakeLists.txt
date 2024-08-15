project(catch2_v3 VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC catch2/catch_amalgamated.cpp)

add_library(surge::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC .)
target_compile_definitions(${PROJECT_NAME} PUBLIC CATCH_AMALGAMATED_CUSTOM_MAIN)

if (APPLE)
    message(STATUS "OSX DEPLOYMENT IS ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if (${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS "10.12")
        message(STATUS "Disabling CATCH2_V3 exceptions for older OSX")
        target_compile_definitions(${PROJECT_NAME} PUBLIC CATCH_CONFIG_DISABLE_EXCEPTIONS)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CTest)
include(Catch)
