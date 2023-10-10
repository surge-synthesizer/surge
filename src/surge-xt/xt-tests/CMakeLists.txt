# vi:set sw=2 et:
project(surge-xt-tests)

add_executable(${PROJECT_NAME}
        main.cpp
        XTTestOSC.cpp
  )

target_link_libraries(${PROJECT_NAME} PRIVATE
  surge-xt
  surge::catch2_v3

  # These are PRIVATE linked by the plugin for appropriate reason
  # but means we need to copy them here
        surge-common
        surge-platform
        surge-juce
        juce::juce_graphics
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_osc
        surge-xt-binary
        sst-filters-extras
)

if (${SURGE_INCLUDE_XT_TESTS_IN_CTEST})
    message(STATUS "Using CatchDiscoverTests on ${PROJECT_NAME}" )
    catch_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${SURGE_SOURCE_DIR})
endif()
