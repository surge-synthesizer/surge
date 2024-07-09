# LuaJitLib

#project(LuaJIT C ASM)
project(luajit-5.1)

set(LUAJIT_DIR ${CMAKE_CURRENT_LIST_DIR}/LuaJIT)
set(LUAJIT_BLD ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT)
set(LUAJIT_INC ${CMAKE_CURRENT_BINARY_DIR}/LuaJIT/src/LuaJIT/src)

# Debugging info
message(STATUS "CMAKE_MAKE_PROGRAM = ${CMAKE_MAKE_PROGRAM}")
message(STATUS "CMAKE_COMMAND = ${CMAKE_COMMAND}")
message(STATUS "C_FLAGS = ${CMAKE_C_FLAGS}")
message(STATUS "CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
message(STATUS "CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CXX_FLAGS_RELWITHDEBINFO = ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CXX_FLAGS_MINSIZEREL = ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "DEPS_C_COMPILER = ${DEPS_C_COMPILER}")

message(STATUS "CMAKE_CURRENT_LIST_DIR = ${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "LUAJIT_DIR = ${LUAJIT_DIR}")
message(STATUS "LUAJIT_BLD = ${LUAJIT_BLD}")
message(STATUS "LUAJIT_INC = ${LUAJIT_INC}")

set(LUAJIT_LIB "")
# Define the library filename based on platform
if(WIN32 AND NOT MINGW)
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/lua51.lib)
else()
  set(LUAJIT_LIB ${LUAJIT_BLD}/bin/libluajit.a)
endif()

if(NOT EXISTS ${LUAJIT_LIB})
  message(STATUS "Building LuaJIT at CMake time right now")
  if(APPLE)
    message(STATUS "./build-fat-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-fat-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE SCRIPT_RESULT
      OUTPUT_VARIABLE SCRIPT_OUTPUT
      ERROR_VARIABLE SCRIPT_ERROR)
  elseif(UNIX)
    message(STATUS "./build-linux-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND ./build-linux-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE SCRIPT_RESULT
      OUTPUT_VARIABLE SCRIPT_OUTPUT
      ERROR_VARIABLE SCRIPT_ERROR)
  elseif(MINGW)
    message(STATUS "./build-mingw-luajit.sh ${LUAJIT_BLD} in ${CMAKE_CURRENT_LIST_DIR}")
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND sh ./build-mingw-luajit.sh ${LUAJIT_BLD}
      RESULT_VARIABLE SCRIPT_RESULT
      OUTPUT_VARIABLE SCRIPT_OUTPUT
      ERROR_VARIABLE SCRIPT_ERROR)
  elseif(WIN32)
    execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMAND build-msvc-luajit.bat ${LUAJIT_BLD}
      RESULT_VARIABLE SCRIPT_RESULT
      OUTPUT_VARIABLE SCRIPT_OUTPUT
      ERROR_VARIABLE SCRIPT_ERROR)
  else()
    # we are got knows what. Probably error out telling the person to deactivate lua
    message(FATAL_ERROR "Failed to build LuaJIT. Architecture not supported!")
  endif()
  if(${SCRIPT_RESULT} GREATER 0)
    message(WARNING "Failed to build LuaJIT:")
    message(WARNING "${SCRIPT_ERROR}")
    message(WARNING "${SCRIPT_OUTPUT}")
    message(FATAL_ERROR "LuaJIT build script error!")
  else()
    file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
    message(STATUS "Succesfully built LuaJIT: ${LUACONTENTS}")
  endif()
else()
  file(GLOB_RECURSE LUACONTENTS "${LUAJIT_BLD}/bin/*")
  message(STATUS "Using existing LuaJIT library: ${LUACONTENTS}")
endif()

# Create an INTERFACE library for LuaJIT
add_library(luajit-5.1 INTERFACE)
target_link_libraries(luajit-5.1 INTERFACE ${LUAJIT_LIB})
# Add the include directories
target_include_directories(luajit-5.1 INTERFACE ${LUAJIT_INC})