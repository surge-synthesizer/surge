project(surge-xt-cli VERSION ${CMAKE_PROJECT_VERSION})

add_subdirectory(../../../libs/CLI11 CLI11)

add_executable(${PROJECT_NAME} cli-main.cpp)
if (APPLE)
    target_sources(${PROJECT_NAME} PRIVATE cli-mac-helpers.mm)
endif()
get_target_property(output_dir surge-xt RUNTIME_OUTPUT_DIRECTORY)
set_property(TARGET ${PROJECT_NAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${output_dir}/CLI)
target_link_libraries(${PROJECT_NAME} PRIVATE
        surge-xt
        surge-xt-impl
        surge-common
        surge-juce-for-surge-xt
        CLI11)
# AppKit and unity are unhappy together
set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD FALSE)