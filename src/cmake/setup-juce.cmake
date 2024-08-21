
if (${CMAKE_UNITY_BUILD})
    add_library(surge-juce STATIC)
    target_link_libraries(surge-juce PRIVATE
        juce::juce_dsp
        juce::juce_osc
        juce::juce_graphics
        juce::juce_audio_utils
        melatonin_inspector
    )

    target_compile_definitions(surge-juce
            INTERFACE
            $<TARGET_PROPERTY:surge-juce,COMPILE_DEFINITIONS>)

    target_include_directories(surge-juce
            INTERFACE
            $<TARGET_PROPERTY:surge-juce,INCLUDE_DIRECTORIES>)


    target_include_directories(surge-juce PUBLIC ${incl})
    target_compile_definitions(surge-juce PUBLIC
            JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1

            JUCE_ALLOW_STATIC_NULL_VARIABLES=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1

            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CAMERA=disabled

            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0

            JUCE_MODAL_LOOPS_PERMITTED=0

            JUCE_COREGRAPHICS_DRAW_ASYNC=1

            JUCE_ALSA=$<IF:$<BOOL:${SURGE_USE_ALSA}>,1,0>
            JUCE_JACK=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

            JUCE_WASAPI=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>
            JUCE_DIRECTSOUND=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

            JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
    )

    set_target_properties(surge-juce PROPERTIES UNITY_BUILD FALSE)

    if (WIN32)
        target_compile_definitions(surge-juce PUBLIC NOMINMAX=1)
    endif()
else()
    add_library(surge-juce INTERFACE)
    target_compile_definitions(surge-juce INTERFACE
            JUCE_ALLOW_STATIC_NULL_VARIABLES=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1

            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CAMERA=disabled

            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0

            JUCE_MODAL_LOOPS_PERMITTED=0

            JUCE_COREGRAPHICS_DRAW_ASYNC=1

            JUCE_ALSA=$<IF:$<BOOL:${SURGE_USE_ALSA}>,1,0>
            JUCE_JACK=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

            JUCE_WASAPI=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>
            JUCE_DIRECTSOUND=$<IF:$<NOT:$<BOOL:${SURGE_SKIP_STANDALONE}>>,1,0>

            JUCE_CATCH_UNHANDLED_EXCEPTIONS=0
    )

    if (WIN32)
        target_compile_definitions(surge-juce INTERFACE NOMINMAX=1)
    endif()
endif()


add_library(surge-juce-for-surge-common INTERFACE)
target_link_libraries(surge-juce-for-surge-common INTERFACE surge-juce)
if (NOT CMAKE_UNITY_BUILD)
    target_link_libraries(surge-juce-for-surge-common INTERFACE surge-juce juce::juce_dsp)
endif()


add_library(surge-juce-for-surge-xt INTERFACE)
target_link_libraries(surge-juce-for-surge-xt INTERFACE surge-juce)
if (NOT CMAKE_UNITY_BUILD)
    target_link_libraries(surge-juce-for-surge-xt INTERFACE surge-juce
            juce::juce_gui_basics
            juce::juce_osc
            juce::juce_audio_utils
            melatonin_inspector
    )
endif()

if(NOT SURGE_SKIP_VST3)
    list(APPEND SURGE_JUCE_FORMATS VST3)
endif()

if(NOT SURGE_SKIP_STANDALONE)
    list(APPEND SURGE_JUCE_FORMATS Standalone)
endif()

if(APPLE AND NOT CMAKE_CROSSCOMPILING)
    # AU requires the Rez resource compiler, which seems to be unavailable on osxcross
    list(APPEND SURGE_JUCE_FORMATS AU)
endif()

if(SURGE_XT_BUILD_AUV3)
    list(APPEND SURGE_JUCE_FORMATS AUv3)
endif()

if(DEFINED ENV{VST2SDK_DIR})
    file(TO_CMAKE_PATH "$ENV{VST2SDK_DIR}" JUCE_VST2_DIR)
    juce_set_vst2_sdk_path(${JUCE_VST2_DIR})
    list(APPEND SURGE_JUCE_FORMATS VST)
    message(STATUS "VST2 SDK path is $ENV{VST2SDK_DIR}")
    # VST2 headers are invalid UTF-8
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/wd4828>)
endif()

if(SURGE_BUILD_LV2)
    list(APPEND SURGE_JUCE_FORMATS LV2)
    message(STATUS "Including JUCE7 LV2 support.")
endif()


if(DEFINED ENV{ASIOSDK_DIR} OR BUILD_USING_MY_ASIO_LICENSE)
    if(BUILD_USING_MY_ASIO_LICENSE)
        message(STATUS "** BUILD USING OWN ASIO LICENSE **")
        message(STATUS "The resulting Surge standalone executable is not licensed for distribution!")
        message(STATUS "Fetching ASIO SDK...")

        set(ASIOSDK_DIR ${CMAKE_BINARY_DIR}/asio/asiosdk)
        add_custom_target(surge-get-local-asio)
        add_custom_command(
                TARGET surge-get-local-asio
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/asio
                COMMAND ${CMAKE_COMMAND} -D ASIO_SDK_DESTINATION=${CMAKE_BINARY_DIR}/asio -P cmake/get-asio.cmake
        )
        add_dependencies(surge-juce surge-get-local-asio)
    else()
        file(TO_CMAKE_PATH "$ENV{ASIOSDK_DIR}" ASIOSDK_DIR)
        message(STATUS "ASIO SDK found at ${ASIOSDK_DIR}")
        message(STATUS "The resulting Surge standalone executable is not licensed for distribution!")
    endif()

    target_compile_definitions(surge-juce INTERFACE JUCE_ASIO=1)
    target_include_directories(surge-juce INTERFACE ${ASIOSDK_DIR}/common)
    set(JUCE_ASIO_SUPPORT TRUE)
endif()

message(STATUS "Building Surge XT using the following JUCE wrappers: ${SURGE_JUCE_FORMATS}")
