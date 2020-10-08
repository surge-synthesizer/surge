# This is the CMake file for the JUCE-special parts. It contains both the references to JUCE source and
# the stuff we use to get and maintain juce


message( STATUS "Building Surge JUCE Plugins" )

juce_add_plugin( surge-patch-player
  PRODUCT_NAME "SurgePatchPlayer"
  COMPANY_NAME "Surge Synth Team"
  BUNDLE_ID "org.surge-synth-team.surge-patch-player"
  PLUGIN_MANUFACTURER_CODE VmbA
  PLUGIN_CODE SgPt

  ICON_SMALL "../../resources/patch_player/SurgeLogo_64.png"
  ICON_SMALL "../../resources/patch_player/SurgeLogo_320.png"
  
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  MEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE

  FORMATS AU VST3 Standalone
  )

add_dependencies( surge-patch-player git-info )
set_property( TARGET surge-patch-player PROPERTY CXX_STANDARD 17 )

file(GLOB SURGE_PATCH_PLAYER_RESOURCES
  ../../resources/patch_player/*
  )

juce_add_binary_data( surge-patch-player-binary SOURCES ${SURGE_PATCH_PLAYER_RESOURCES} )
set_target_properties( surge-patch-player-binary PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

juce_generate_juce_header( surge-patch-player )

set( NON_JUCE_COMMON_SOURCES ${SURGE_COMMON_SOURCES} )
list( TRANSFORM NON_JUCE_COMMON_SOURCES PREPEND "../../" )

set( NON_JUCE_COMMON_INCLUDES ${SURGE_COMMON_INCLUDES} )
list( TRANSFORM NON_JUCE_COMMON_INCLUDES PREPEND "../../" )

add_dependencies( surge-patch-player git-info )

target_sources( surge-patch-player PRIVATE
  patch_player/SurgePatchPlayerEditor.cpp
  patch_player/SurgePatchPlayerProcessor.cpp
  shared/UserInteractionsJUCE.cpp
  
  ${NON_JUCE_COMMON_SOURCES}
  ${CMAKE_BINARY_DIR}/geninclude/version.cpp
  )

target_include_directories( surge-patch-player
  PRIVATE
  ${NON_JUCE_COMMON_INCLUDES}
  ../../src/juce_adapter
  ../../src/headless
  shared
  )

if( UNIX AND NOT APPLE )
  # Needed for configuration.xml
  target_include_directories( surge-patch-player
    PRIVATE
    ../../src/linux
    )
  target_sources( surge-patch-player
    PRIVATE
    ../../${cxml}
    )
endif()

target_compile_definitions( surge-patch-player PUBLIC
  JUCE_ALLOW_STATIC_NULL_VARIABLES=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  DONT_SET_USING_JUCE_NAMESPACE=1
  
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  
  JUCE_ALSA=1
  JUCE_JACK=1
  
  ${OS_COMPILE_DEFINITIONS}

  TARGET_HEADLESS=1
  TARGET_JUCE=1
  )

target_link_libraries(surge-patch-player PRIVATE
  surge-patch-player-binary
  surge-shared
  juce::juce_analytics
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_box2d
  juce::juce_dsp
  )

get_target_property( OUTPUT_DIR surge-patch-player RUNTIME_OUTPUT_DIRECTORY )
add_custom_target( well-named-juce-assets ALL )
if( APPLE )
  add_custom_target( well-named-surge-patch-player-au )
  add_dependencies( well-named-surge-patch-player-au surge-patch-player_AU )
  add_dependencies( well-named-juce-assets well-named-surge-patch-player-au )
endif()
add_custom_target( well-named-surge-patch-player-vst3 )
add_dependencies( well-named-surge-patch-player-vst3 surge-patch-player_VST3 )
add_dependencies( well-named-juce-assets well-named-surge-patch-player-vst3 )

add_custom_target( well-named-surge-patch-player-standalone )
add_dependencies( well-named-surge-patch-player-standalone surge-patch-player_Standalone )
add_dependencies( well-named-juce-assets well-named-surge-patch-player-standalone )

set( WELL_NAMED_ASSET_DIR ${CMAKE_BINARY_DIR}/surge_products )

add_custom_command(
  TARGET well-named-surge-patch-player-vst3
  POST_BUILD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory  ${WELL_NAMED_ASSET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${OUTPUT_DIR}/VST3 ${WELL_NAMED_ASSET_DIR}
  )

add_custom_command(
  TARGET well-named-surge-patch-player-standalone
  POST_BUILD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory  ${WELL_NAMED_ASSET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${OUTPUT_DIR}/Standalone  ${WELL_NAMED_ASSET_DIR}
  )

if( APPLE )
  add_custom_command(
    TARGET well-named-surge-patch-player-au
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory  ${WELL_NAMED_ASSET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${OUTPUT_DIR}/AU ${WELL_NAMED_ASSET_DIR}
  )
endif()
