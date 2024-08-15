# See README.md for more info

project(LuaJIT C ASM)

# Set path variables
set(LUAJIT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/LuaJIT")
set(LUAJIT_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/include")

# Define library filename based on platform
if(WIN32 AND NOT MINGW)
  set(LUAJIT_LIBRARY "${LUAJIT_BUILD_DIR}/bin/lua51.lib")
else()
  set(LUAJIT_LIBRARY "${LUAJIT_BUILD_DIR}/bin/libluajit.a")
endif()

# Load cmake-FindVcvars
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(cmd_wrapper)
if(WIN32 AND NOT MINGW)
  find_package(Vcvars REQUIRED)
  set(cmd_wrapper ${Vcvars_LAUNCHER})
endif()

# Debug info
function(print_debug_info)
  message(STATUS "Start LuaJIT debug info")
  message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
  message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
  message(STATUS "LUAJIT_BUILD_DIR: ${LUAJIT_BUILD_DIR}")
  message(STATUS "LUAJIT_INCLUDE_DIR: ${LUAJIT_INCLUDE_DIR}")
  message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
  message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
  message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
  message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
  message(STATUS "Build script exit code: ${LUAJIT_RESULT}")
  message(STATUS "Build script output: ${LUAJIT_OUTPUT}")
  message(STATUS "Build script error: ${LUAJIT_ERROR}")
endfunction()

# Error handling
function(handle_error message)
  # Start with displaying debug info
  print_debug_info()
  # Show error message
  message(WARNING "${message}")
  # Now what? Refer to logs and README.md
  message(WARNING "See the logs above and the README.md in the libs/luajitlib folder for more information")
  # Finally error out
  message(FATAL_ERROR "Failed to build LuaJIT!")
endfunction()

# Build LuaJIT if existing library is not found
if(NOT EXISTS "${LUAJIT_LIBRARY}")
  message(STATUS "Building LuaJIT at CMake time right now; configure with -DSURGE_SKIP_LUA=TRUE to disable")
  # Check APPLE before UNIX
  if(APPLE)
    message(STATUS "Running ./build-macos-luajit.sh \"${LUAJIT_BUILD_DIR}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      COMMAND ./build-macos-luajit.sh "${LUAJIT_BUILD_DIR}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(UNIX)
    message(STATUS "Running ./build-linux-luajit.sh \"${LUAJIT_BUILD_DIR}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      COMMAND ./build-linux-luajit.sh "${LUAJIT_BUILD_DIR}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  # Check MINGW before WIN32
  elseif(MINGW)
    message(STATUS "Running ./build-mingw-luajit.sh \"${LUAJIT_BUILD_DIR}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      COMMAND sh ./build-mingw-luajit.sh "${LUAJIT_BUILD_DIR}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(WIN32)
    message(STATUS "Running build-msvc-luajit.bat \"${LUAJIT_BUILD_DIR}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      COMMAND ${cmd_wrapper} build-msvc-luajit.bat "${LUAJIT_BUILD_DIR}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  else()
    # Unsupported platform or architecture
    handle_error("Failed to build LuaJIT: Unsupported platform or architecture!")
  endif()

  # Check build script exit code
  if(${LUAJIT_RESULT} GREATER 0)
    handle_error("Failed to build LuaJIT: Script error!")
  endif()

  # If we don't have a library at this point the build script failed silently
  if(NOT EXISTS "${LUAJIT_LIBRARY}")
    handle_error("Failed to build LuaJIT: Library not found after build attempt!")
  endif()

  # Show contents of bin directory if build was succesful
  file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BUILD_DIR}/bin/*")
  message(STATUS "Successfully built LuaJIT: ${LUACONTENTS}")

else()
  # Show contents of bin directory if existing library was found
  file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BUILD_DIR}/bin/*")
  message(STATUS "Using existing LuaJIT library: ${LUACONTENTS}")
endif()

# Create an INTERFACE library for LuaJIT
add_library(luajit-5.1 INTERFACE)
target_link_libraries(luajit-5.1 INTERFACE ${LUAJIT_LIBRARY})
target_include_directories(luajit-5.1 INTERFACE ${LUAJIT_INCLUDE_DIR})