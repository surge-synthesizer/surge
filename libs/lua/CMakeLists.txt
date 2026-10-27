# libs/lua
#
# The single owner of Surge XT's Lua backend choice. Everything else in the tree
# links surge::lua and does not care which implementation is behind it.
#
#   -DSURGE_LUA_IMPL=luajit   LuaJIT, built out of band by libs/luajitlib (default)
#   -DSURGE_LUA_IMPL=puc      PUC-Rio Lua 5.2.4 + Lua BitOp, built here from source
#   -DSURGE_LUA_IMPL=none     no Lua at all; Formula Modulator and WTSE compile out
#
# The puc backend exists because LuaJIT cannot be built for Windows on ARM: its
# interpreter is hand-written DynASM assembly with no ARM64EC entry thunk or ABI
# support, which is VM-level porting work rather than a configuration flag. Those
# targets used to pass -DSURGE_SKIP_LUA=TRUE and ship without Formula Modulator.
#
# LuaJIT remains the default everywhere it builds; it is faster and nobody wants
# to give that up. See README.md for what is vendored and how to update it.

set(SURGE_LUA_IMPL "" CACHE STRING
    "Lua backend for Surge XT: 'luajit', 'puc' or 'none'. Empty selects the default.")
set_property(CACHE SURGE_LUA_IMPL PROPERTY STRINGS "" luajit puc none)

if(SURGE_LUA_IMPL STREQUAL "")
  if(SURGE_SKIP_LUA)
    # SURGE_SKIP_LUA predates SURGE_LUA_IMPL and is still set by the
    # SURGE_BUILD_RS branch in the top level CMakeLists, so keep honouring it.
    set(_surge_lua_impl "none")
  else()
    set(_surge_lua_impl "luajit")
  endif()
else()
  string(TOLOWER "${SURGE_LUA_IMPL}" _surge_lua_impl)

  if(SURGE_SKIP_LUA AND NOT _surge_lua_impl STREQUAL "none")
    message(WARNING
      "Both SURGE_SKIP_LUA and SURGE_LUA_IMPL=${_surge_lua_impl} are set. "
      "SURGE_LUA_IMPL wins. SURGE_SKIP_LUA is deprecated; prefer -DSURGE_LUA_IMPL=none.")
  endif()
endif()

add_library(surge-lua INTERFACE)
add_library(surge::lua ALIAS surge-lua)

if(_surge_lua_impl STREQUAL "luajit")
  message(STATUS "Lua backend: LuaJIT")
  surge_add_lib_subdirectory(luajitlib)
  target_link_libraries(surge-lua INTERFACE luajit-5.1)
  target_compile_definitions(surge-lua INTERFACE HAS_LUA=1)

elseif(_surge_lua_impl STREQUAL "puc")
  message(STATUS "Lua backend: PUC-Rio Lua 5.2.4 with Lua BitOp 1.0.4")

  set(_lua_src ${CMAKE_CURRENT_LIST_DIR}/lua-5.2.4/src)

  # Listed rather than globbed so that adding a file to the vendored drop is a
  # deliberate act that shows up in review. This is upstream's src/ minus the
  # standalone interpreter (lua.c), the bytecode compiler (luac.c), linit.c
  # (replaced by surge-lua-openlibs.c) and lbitlib.c (bit32, which LuaJIT has no
  # equivalent of). See README.md.
  add_library(lua-5.2.4 STATIC
    ${_lua_src}/lapi.c
    ${_lua_src}/lauxlib.c
    ${_lua_src}/lbaselib.c
    ${_lua_src}/lcode.c
    ${_lua_src}/lcorolib.c
    ${_lua_src}/lctype.c
    ${_lua_src}/ldblib.c
    ${_lua_src}/ldebug.c
    ${_lua_src}/ldo.c
    ${_lua_src}/ldump.c
    ${_lua_src}/lfunc.c
    ${_lua_src}/lgc.c
    ${_lua_src}/liolib.c
    ${_lua_src}/llex.c
    ${_lua_src}/lmathlib.c
    ${_lua_src}/lmem.c
    ${_lua_src}/loadlib.c
    ${_lua_src}/lobject.c
    ${_lua_src}/lopcodes.c
    ${_lua_src}/loslib.c
    ${_lua_src}/lparser.c
    ${_lua_src}/lstate.c
    ${_lua_src}/lstring.c
    ${_lua_src}/lstrlib.c
    ${_lua_src}/ltable.c
    ${_lua_src}/ltablib.c
    ${_lua_src}/ltm.c
    ${_lua_src}/lundump.c
    ${_lua_src}/lvm.c
    ${_lua_src}/lzio.c

    ${CMAKE_CURRENT_LIST_DIR}/luabitop-1.0.4/bit.c
    ${CMAKE_CURRENT_LIST_DIR}/surge-lua-openlibs.c
  )

  target_include_directories(lua-5.2.4 PUBLIC ${_lua_src})

  # PUBLIC because it gates header-level things Surge itself relies on: the
  # lua_objlen macro, the global unpack the formula sandbox whitelists, and the
  # math.log10 both preludes call.
  target_compile_definitions(lua-5.2.4 PUBLIC LUA_COMPAT_ALL)

  # PUBLIC so that Surge's own translation units see the same luaconf.h as the
  # library was compiled with. On Windows luaconf.h picks up _WIN32 by itself.
  # LUA_USE_LINUX is deliberately avoided: it pulls in LUA_USE_DLOPEN (-ldl) and
  # LUA_USE_READLINE, and both only serve the standalone interpreter we omit.
  if(APPLE)
    target_compile_definitions(lua-5.2.4 PUBLIC LUA_USE_MACOSX)
  elseif(UNIX)
    target_compile_definitions(lua-5.2.4 PUBLIC LUA_USE_POSIX)
  endif()

  # Vendored C compiled under Surge's warning flags. The top level CMakeLists
  # turns on -Werror (non-Windows) and -WX (Windows, except arm64/arm64ec) before
  # libs are added, and _CRT_SECURE_NO_WARNINGS is set only on surge-common, so
  # this target has to opt out for itself or stock Lua fails the build on fopen.
  if(MSVC)
    target_compile_definitions(lua-5.2.4 PRIVATE _CRT_SECURE_NO_WARNINGS=1)
    target_compile_options(lua-5.2.4 PRIVATE /W0 /WX-)
  else()
    target_compile_options(lua-5.2.4 PRIVATE -w)
  endif()

  target_link_libraries(surge-lua INTERFACE lua-5.2.4)
  target_compile_definitions(surge-lua INTERFACE HAS_LUA=1)

elseif(_surge_lua_impl STREQUAL "none")
  message(STATUS "Lua backend: none - Formula Modulator and Wavetable Script are disabled")
  target_compile_definitions(surge-lua INTERFACE HAS_LUA=0)

else()
  message(FATAL_ERROR
    "Unknown SURGE_LUA_IMPL '${SURGE_LUA_IMPL}'. Valid values are luajit, puc and none.")
endif()
