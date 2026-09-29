find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(overlay ${CMAKE_BINARY_DIR}/juce-browser)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/web/scripts/prepare-juce.py
  ${CMAKE_SOURCE_DIR}/libs/JUCE ${overlay}
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Unable to prepare JUCE browser platform")
endif()
add_library(surge-juce-browser STATIC
  ${overlay}/modules/juce_core/juce_core.cpp
  ${overlay}/modules/juce_events/juce_events.cpp
  ${overlay}/modules/juce_data_structures/juce_data_structures.cpp
  ${overlay}/modules/juce_graphics/juce_graphics.cpp
  ${overlay}/modules/juce_graphics/juce_graphics_Harfbuzz.cpp
  ${overlay}/modules/juce_graphics/juce_graphics_Sheenbidi.c
  ${overlay}/modules/juce_gui_basics/juce_gui_basics.cpp)
target_include_directories(surge-juce-browser PUBLIC ${overlay}/modules PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(surge-juce-browser PUBLIC
  SUPPORT_AFFINITIES=0 JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
  JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_FREETYPE=1 JUCE_USE_FONTCONFIG=0
  JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0
  JUCE_USE_XSHM=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0 JUCE_JACK=0)
target_compile_options(surge-juce-browser PRIVATE -Wno-error -sUSE_FREETYPE=1)
target_link_options(surge-juce-browser INTERFACE -sUSE_FREETYPE=1)
add_executable(surge-juce-browser-check BrowserCheck.cpp)
target_link_libraries(surge-juce-browser-check PRIVATE surge-juce-browser)
target_link_options(surge-juce-browser-check PRIVATE --profiling-funcs -sALLOW_MEMORY_GROWTH=1
  -sPTHREAD_POOL_SIZE=4 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1
  --preload-file ${CMAKE_SOURCE_DIR}/resources/fonts@/fonts
  --shell-file ${CMAKE_SOURCE_DIR}/web/juce-shell.html
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/browser/platform.js" -lidbfs.js
  "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','stringToNewUTF8']")
set_target_properties(surge-juce-browser-check PROPERTIES SUFFIX .html RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)

# Build exactly the same processor, editor, widgets, and overlays as desktop.
foreach(module juce_audio_processors_headless juce_audio_basics juce_audio_formats juce_audio_devices juce_audio_processors juce_audio_utils juce_gui_extra juce_dsp juce_osc)
  target_sources(surge-juce-browser PRIVATE ${overlay}/modules/${module}/${module}.cpp)
endforeach()
target_compile_definitions(surge-juce-browser PUBLIC
  JUCE_PLUGINHOST_VST=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0
  JUCE_PLUGINHOST_LADSPA=0 JUCE_PLUGINHOST_LV2=0)
set(binary_dir ${CMAKE_CURRENT_BINARY_DIR}/binary)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/web/scripts/binary-data.py ${CMAKE_SOURCE_DIR} ${binary_dir} COMMAND_ERROR_IS_FATAL ANY)
add_library(surge-xt-browser-editor STATIC ${binary_dir}/SurgeXTBinary.cpp)
set(PROJECT_NAME surge-xt-browser-editor)
include(${CMAKE_SOURCE_DIR}/src/surge-xt/sources.cmake)
target_include_directories(surge-xt-browser-editor PUBLIC
  ${CMAKE_SOURCE_DIR}/src/surge-xt ${CMAKE_SOURCE_DIR}/src/surge-xt/gui
  ${CMAKE_SOURCE_DIR}/src/surge-xt/osc PRIVATE ${binary_dir})
target_compile_definitions(surge-xt-browser-editor PUBLIC
  JucePlugin_Name="Surge\ XT" BUILD_IS_RELEASE=1 HAS_CLAP_JUCE_EXTENSIONS=0)
target_link_libraries(surge-xt-browser-editor PUBLIC surge-common surge-platform surge-juce-browser sst-filters-extras)
add_executable(surge-xt-browser BrowserMain.cpp BrowserAudio.cpp BrowserWavetables.cpp BrowserAudioThread.c)
target_link_libraries(surge-xt-browser PRIVATE surge-xt-browser-editor)
# Isolated, version-checked retirement shim for the pinned SDK's hybrid worklet.
set_source_files_properties(BrowserAudioThread.c PROPERTIES INCLUDE_DIRECTORIES
  "${EMSCRIPTEN_ROOT_PATH}/system/lib/libc/musl/src/include;${EMSCRIPTEN_ROOT_PATH}/system/lib/libc/musl/src/internal;${EMSCRIPTEN_ROOT_PATH}/system/lib/libc/musl/arch/emscripten;${EMSCRIPTEN_ROOT_PATH}/system/lib/pthread")
target_link_options(surge-xt-browser PRIVATE "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/browser/factory-startup.js" -Wl,--error-limit=0 --profiling-funcs -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sINITIAL_MEMORY=536870912
  -sSTACK_SIZE=2097152 -sPTHREAD_POOL_SIZE=8 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1
  --preload-file ${CMAKE_SOURCE_DIR}/resources/fonts@/fonts
  --shell-file ${CMAKE_SOURCE_DIR}/web/juce-shell.html
  "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/browser/platform.js" -lidbfs.js
  "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','stringToNewUTF8']")
set_target_properties(surge-xt-browser PROPERTIES SUFFIX .html RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)

set_property(TARGET surge-xt-browser surge-juce-browser-check APPEND PROPERTY LINK_DEPENDS
  ${CMAKE_SOURCE_DIR}/web/browser/platform.js ${CMAKE_SOURCE_DIR}/web/juce-shell.html)

# Keep resources separately fetchable; never embed the factory library into startup data.
add_custom_target(surge-factory-library
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/web/scripts/factory-library.py
    ${CMAKE_SOURCE_DIR}/resources/data ${CMAKE_BINARY_DIR}/web/library
  VERBATIM)
add_dependencies(surge-xt-browser surge-factory-library)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/library.js ${CMAKE_BINARY_DIR}/web/library.js COPYONLY)

set_property(TARGET surge-xt-browser APPEND PROPERTY LINK_DEPENDS
  ${CMAKE_SOURCE_DIR}/web/browser/factory-startup.js)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/midi.js ${CMAKE_BINARY_DIR}/web/midi.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/transport.js ${CMAKE_BINARY_DIR}/web/transport.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/audio-input.js ${CMAKE_BINARY_DIR}/web/audio-input.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/audio-lifecycle.js ${CMAKE_BINARY_DIR}/web/audio-lifecycle.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/reports.js ${CMAKE_BINARY_DIR}/web/reports.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/user-files.js ${CMAKE_BINARY_DIR}/web/user-files.js COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/clipboard.js ${CMAKE_BINARY_DIR}/web/clipboard.js COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/text-input.js ${CMAKE_BINARY_DIR}/web/text-input.js COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/accessibility.js ${CMAKE_BINARY_DIR}/web/accessibility.js COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/web/browser/runtime.js ${CMAKE_BINARY_DIR}/web/runtime.js COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/tuning-library.js ${CMAKE_BINARY_DIR}/web/tuning-library.js COPYONLY)

configure_file(${CMAKE_SOURCE_DIR}/web/browser/archive-recovery.js ${CMAKE_BINARY_DIR}/web/archive-recovery.js COPYONLY)
