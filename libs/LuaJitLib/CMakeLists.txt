# So what's all this then? LuaJitLib attempts to build LuaJIT using it's makefile only build system so it can be used within Surge's CMake build system

project(LuaJIT C ASM)

# Set path variables
set(LUAJIT_SRC ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/src/LuaJIT/src)
set(LUAJIT_BLD ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT)
set(LUAJIT_INC ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/include)

# Define library filename based on platform
set(LUAJIT_LIB)
if(WIN32 AND MSVC)
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/lua51.lib)
else()
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/libluajit.a)
endif()

# Load cmake-FindVcvars
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(cmd_wrapper)
if(WIN32 AND MSVC)
  find_package(Vcvars REQUIRED)
  set(cmd_wrapper ${Vcvars_LAUNCHER})
endif()

# Build LuaJIT if existing library is not found
if(NOT EXISTS ${LUAJIT_LIB})
  message(STATUS "Building LuaJIT at CMake time right now; configure with -DSURGE_SKIP_LUA=TRUE to disable")
  # TODO: Add custom script for CMAKE_CROSSCOMPILING
  if(APPLE)
    message(STATUS "./build-macos-luajit.sh \"${LUAJIT_BLD}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-macos-luajit.sh "${LUAJIT_BLD}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(UNIX)
    message(STATUS "./build-linux-luajit.sh \"${LUAJIT_BLD}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-linux-luajit.sh "${LUAJIT_BLD}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(MINGW)
    message(STATUS "sh ./build-mingw-luajit.sh \"${LUAJIT_BLD}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND sh ./build-mingw-luajit.sh "${LUAJIT_BLD}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(WIN32 AND MSVC)
    message(STATUS "build-msvc-luajit.bat \"${LUAJIT_BLD}\" in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ${cmd_wrapper} build-msvc-luajit.bat "${LUAJIT_BLD}"
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  else()
    # Unsupported platform or architecture, set flag for error section handling and show warning
    set(LUAJIT_UNSUPPORTED_BUILD 1)
    message(WARNING "Failed to build LuaJIT: Unsupported platform or architecture!")
  endif()
  # Check build script exit code
  if(${LUAJIT_RESULT} GREATER 0)
    # Build script error, set flag for error section handling and show warning
    set(LUAJIT_SCRIPT_ERROR 1)
    message(WARNING "Failed to build LuaJIT: Script error!")
  else()
    # Show contents of bin directory if build was succesful
    file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
    message(STATUS "Succesfully built LuaJIT: ${LUACONTENTS}")
  endif()
else()
  # Show contents of bin directory if existing library was found
  file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
  message(STATUS "Using existing LuaJIT library: ${LUACONTENTS}")
endif()

# Error section, if the library can't be found at this point the build script failed silently so we want to check for that too
if(LUAJIT_UNSUPPORTED_BUILD OR LUAJIT_SCRIPT_ERROR OR NOT EXISTS ${LUAJIT_LIB})
  # Start with some excessive debug spam
  message(STATUS "Start LuaJIT debug info")
  message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
  message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
  message(STATUS "LUAJIT_SRC: ${LUAJIT_SRC}")
  message(STATUS "LUAJIT_BLD: ${LUAJIT_BLD}")
  message(STATUS "LUAJIT_INC: ${LUAJIT_INC}")
  message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
  message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
  message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
  message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
  message(STATUS "Build script exit code: ${LUAJIT_RESULT}")
  message(STATUS "Build script output: ${LUAJIT_OUTPUT}")
  message(STATUS "Build script error: ${LUAJIT_ERROR}")
  # Show specific error info
  if(LUAJIT_UNSUPPORTED_BUILD)
    message(WARNING "The LuaJIT build script failed because the target platform or architecture is not supported. See the logs included above for more information")
  elseif (LUAJIT_SCRIPT_ERROR)
    message(WARNING "The LuaJIT build script failed because the build script encountered an error. See the logs included above for more information")
  elseif (NOT EXISTS ${LUAJIT_LIB})
    message(WARNING "The LuaJIT build script failed because of an unknown error. See the logs included above for more information")
  endif()
  # Now what? Refer to documentation and point out places to request support
  # TODO: Either add documentation in /doc/ or start wiki article
  message(WARNING "To request support open an issue on the Surge XT GitHub page at:
https://github.com/surge-synthesizer/surge/issues/
or join the Surge Discord and ask in the #help channel:
https://discord.gg/spGANHw")
  # Finally error out
  message(FATAL_ERROR "Failed to build LuaJIT!")
endif()

# Create an INTERFACE library for LuaJIT
add_library(luajit-5.1 INTERFACE)
target_link_libraries(luajit-5.1 INTERFACE ${LUAJIT_LIB})
target_include_directories(luajit-5.1 INTERFACE ${LUAJIT_INC})