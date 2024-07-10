# LuaJitLib

project(LuaJIT C ASM)

# Set path variables
set(LUAJIT_SRC ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/src/LuaJIT/src)
set(LUAJIT_BLD ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT)
set(LUAJIT_INC ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/include)

# Set to "1" to show additional debug messages
set(LUAJIT_DEBUG 1)

# Load cmake-FindVcvars
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(cmd_wrapper)
if(WIN32 AND MSVC)
  find_package(Vcvars REQUIRED)
  set(cmd_wrapper ${Vcvars_LAUNCHER})
endif()

# Define library filename based on platform
set(LUAJIT_LIB)
if(WIN32 AND MSVC)
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/lua51.lib)
else()
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/libluajit.a)
endif()

# Build LuaJIT if existing library is not found
if(NOT EXISTS ${LUAJIT_LIB})
  message(STATUS "Building LuaJIT at CMake time right now")
  if(APPLE)
    message(STATUS "./build-fat-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-fat-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(UNIX)
    message(STATUS "./build-linux-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-linux-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(MINGW)
    message(STATUS "sh ./build-mingw-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND sh ./build-mingw-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  elseif(WIN32 AND MSVC)
    message(STATUS "build-msvc-luajit.bat ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ${cmd_wrapper} build-msvc-luajit.bat ${LUAJIT_BLD}
      RESULT_VARIABLE LUAJIT_RESULT
      OUTPUT_VARIABLE LUAJIT_OUTPUT
      ERROR_VARIABLE LUAJIT_ERROR)
  else()
    # TODO: Add info on how to request support for unsupported platform/architecture
    # Add info on how to compile LuaJIT manually
    # Add info on how to disable Lua with -DSURGE_SKIP_LUA=TRUE
    message(FATAL_ERROR "Failed to build LuaJIT: Architecture not supported!")
  endif()

  # Check exit code of script
  if(${LUAJIT_RESULT} GREATER 0)
    message(WARNING "LUAJIT_RESULT: ${LUAJIT_RESULT}")
    message(WARNING "LUAJIT_OUTPUT: ${LUAJIT_OUTPUT}")
    message(WARNING "LUAJIT_ERROR: ${LUAJIT_ERROR}")
    message(FATAL_ERROR "Failed to build LuaJIT: Script error!")
  else()
    file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
    message(STATUS "Succesfully built LuaJIT: ${LUACONTENTS}")
  endif()

# Show contents of bin directory if existing library was found
else()
  file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
  message(STATUS "Using existing LuaJIT library: ${LUACONTENTS}")
endif()

# Debug messages 
if(LUAJIT_DEBUG)
  message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
  message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
  message(STATUS "LUAJIT_SRC: ${LUAJIT_SRC}")
  message(STATUS "LUAJIT_BLD: ${LUAJIT_BLD}")
  message(STATUS "LUAJIT_INC: ${LUAJIT_INC}")
  if(LUAJIT_RESULT)
    message(STATUS "LUAJIT_RESULT: ${LUAJIT_RESULT}")
  endif()
  if(LUAJIT_OUTPUT)
    message(STATUS "LUAJIT_OUTPUT: ${LUAJIT_OUTPUT}")
  endif()
  if(LUAJIT_ERROR)
    message(STATUS "LUAJIT_ERROR: ${LUAJIT_ERROR}")
  endif()
  file(GLOB_RECURSE LUAJIT_SRC_CONTENT "${LUAJIT_SRC}/*")
  message(STATUS "LUAJIT_SRC_CONTENT: ${LUAJIT_SRC_CONTENT}")
  file(GLOB_RECURSE LUAJIT_BIN_CONTENT "${LUAJIT_BLD}/bin/*")
  message(STATUS "LUAJIT_BIN_CONTENT: ${LUAJIT_BIN_CONTENT}")
  file(GLOB_RECURSE LUAJIT_INC_CONTENT "${LUAJIT_INC}/*")
  message(STATUS "LUAJIT_INC_CONTENT: ${LUAJIT_INC_CONTENT}")
endif()

# Create an INTERFACE library for LuaJIT
add_library(luajit-5.1 INTERFACE)
target_link_libraries(luajit-5.1 INTERFACE ${LUAJIT_LIB})
# Add the include directory
target_include_directories(luajit-5.1 INTERFACE ${LUAJIT_INC})