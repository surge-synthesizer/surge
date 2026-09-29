# Lua 5.1 preserves the patch scripting API (including setfenv). BitOp supplies
# the bit library exposed by LuaJIT, without requiring a JIT or executable memory.
include(FetchContent)
FetchContent_Declare(surge_lua
  URL "${CMAKE_CURRENT_LIST_DIR}/vendor/lua-5.1.5.tar.gz"
  URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333)
FetchContent_Declare(surge_bitop
  URL "${CMAKE_CURRENT_LIST_DIR}/vendor/luabitop-81bb23b0e737805442033535de8e6d204d0e5381.tar.gz"
  URL_HASH SHA256=d483d0241f907d26eb596ddb7ad5860686eb01974b32bcc8b566b4ec25811ce2)
FetchContent_GetProperties(surge_lua)
if(NOT surge_lua_POPULATED)
  FetchContent_Populate(surge_lua)
endif()
FetchContent_GetProperties(surge_bitop)
if(NOT surge_bitop_POPULATED)
  FetchContent_Populate(surge_bitop)
endif()
file(GLOB lua_sources CONFIGURE_DEPENDS ${surge_lua_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM lua_sources ${surge_lua_SOURCE_DIR}/src/lua.c ${surge_lua_SOURCE_DIR}/src/luac.c ${surge_lua_SOURCE_DIR}/src/print.c)
add_library(luajit-5.1 STATIC ${lua_sources} ${surge_bitop_SOURCE_DIR}/bit.c)
target_include_directories(luajit-5.1 PUBLIC ${surge_lua_SOURCE_DIR}/src)
target_compile_definitions(luajit-5.1 PUBLIC HAS_LUA=1 SURGE_PORTABLE_LUA=1)
# Upstream Lua 5.1 predates the project's warning policy.
target_compile_options(luajit-5.1 PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-error>)
