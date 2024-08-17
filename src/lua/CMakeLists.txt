# vi:set sw=2 et:
project(surge-lua-src)

# This library embeds Lua sources into the binary (without JUCE)
#
# To add a new Lua script:
# 1. Create myscript.lua
# 2. Add it to the list below
# 3. myscript.lua is now a std::string at Surge::LuaSources::myscript

set(lua_sources
  formula_prelude.lua
  formula_prelude_test.lua
  wtse_prelude.lua
)

set(generated_sources
  ${CMAKE_CURRENT_BINARY_DIR}/LuaSources.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/include/lua/LuaSources.h
)

add_library(${PROJECT_NAME} ${generated_sources})

add_custom_command(OUTPUT ${generated_sources}
  COMMAND ${CMAKE_COMMAND} -P lua2cpp.cmake ${CMAKE_CURRENT_BINARY_DIR} LuaSources.cpp lua/LuaSources.h Surge::LuaSources ${lua_sources}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS lua2cpp.cmake ${lua_sources}
)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
