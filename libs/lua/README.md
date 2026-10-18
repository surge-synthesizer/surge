# libs/lua

This directory owns which Lua implementation Surge XT is built against. Everything
else in the tree links `surge::lua` and does not need to know what is behind it.

```
-DSURGE_LUA_IMPL=luajit   LuaJIT, built out of band by libs/luajitlib (the default)
-DSURGE_LUA_IMPL=puc      PUC-Rio Lua 5.2.4 plus Lua BitOp, built here from source
-DSURGE_LUA_IMPL=none     no Lua; Formula Modulator and Wavetable Script compile out
```

`SURGE_SKIP_LUA=TRUE` still works as a deprecated alias for `none`, because the
`SURGE_BUILD_RS` branch in the top level `CMakeLists.txt` sets it.

## Why a second Lua exists

LuaJIT cannot be built for Windows on ARM. Its interpreter is hand-written DynASM
assembly, and ARM64EC needs entry thunks and a different ABI mapping than the
existing `arm64` backend provides. That is VM-level porting work on LuaJIT's
assembly, not a configuration flag, so it is not something Surge can carry.

Before this directory existed, the Windows ARM64EC and ARM64 targets were built
with `-DSURGE_SKIP_LUA=TRUE` and shipped with no Formula Modulator and no
Wavetable Script at all. Running stock Lua on those two targets trades interpreter
speed for having the features exist, which is a much better deal than losing them.

**LuaJIT remains the default on every platform where it builds.** It is faster and
there is no reason to give that up; `puc` is a fallback, not a replacement.

## The language contract

**LuaJIT's language is the one formulas are written against.** The point of this
backend is that a patch authored on any other build keeps working here, so the
vendored version is chosen to accept everything LuaJIT accepts.

That is why this is Lua **5.2.4** rather than 5.1.5. LuaJIT 2.1 is Lua 5.1 plus a
handful of 5.2 adoptions, and 5.1.5 is missing exactly those - two of them
silently, which is the dangerous kind:

| | LuaJIT | 5.1.5 | 5.2.4 |
|---|---|---|---|
| `goto` / `::label::` | ok | parse error | ok |
| `"\z"` line continuation | ok | parse error | ok |
| `"\x41"` hex escape | `"A"` | silently `"x41"` | ok |
| `math.log(x, base)` | honours base | silently ignores it | ok |

Picking 5.2 fixes all four by construction instead of by patching the vendored
parser, which is why it beats staying on 5.1.5 and backporting.

5.2 does also bring a few things LuaJIT has no equivalent of - `table.pack`,
`table.unpack`, `_ENV`, `rawlen`, and `__len` honoured on tables. These are left in
place deliberately rather than stripped out. The asymmetry is the harmless
direction: a formula written against LuaJIT works everywhere, and the vast majority
of users are on a LuaJIT build. **Formulas should not use them**, because a formula
that does will fail on every non-ARM build.

`bit32` is the exception that *is* stripped: `lbitlib.c` is left out of the drop
entirely, since `bit` comes from Lua BitOp and LuaJIT has no `bit32`.

One cosmetic difference remains and cannot be reconciled: 5.2 stopped quoting the
`<eof>` token in syntax errors, so a truncated formula reports `near <eof>` here
and `near '<eof>'` on LuaJIT. The "Doesn't Parse" case in `UnitTestsLUA.cpp`
asserts both spellings.

## What is vendored

| Directory | Upstream | Version | Licence |
|---|---|---|---|
| `lua-5.2.4/` | <https://www.lua.org/ftp/lua-5.2.4.tar.gz> | 5.2.4 | MIT (notice at the end of `src/lua.h`, full text in `doc/readme.html`) |
| `luabitop-1.0.4/` | <https://bitop.luajit.org/download/LuaBitOp-1.0.4.tar.gz> | 1.0.4 | MIT (`luabitop-1.0.4/README`) |

```
lua-5.2.4.tar.gz      sha256  b9e2e4aad6789b3b63a056d442f7b39f0ecfca3ae0f1fc0ae4e9614401b69f4b
LuaBitOp-1.0.4.tar.gz sha256  adc32785579fc62f7736881332f70f50fe168b2ae8d0ddb741ff018a76f48c80
```

Both drops are byte-for-byte upstream except that files have been removed. Nothing
has been edited, so re-extracting either tarball and diffing is enough to verify
this directory. Note that this repository sets `* text=auto`, so compare against
what git stores rather than a working copy that may have been checked out CRLF.

### Files left out of the drop

From `lua-5.2.4/src`:

- `lua.c`, `luac.c` - the standalone interpreter and bytecode compiler. Not wanted
  in a plugin, and `lua.c` defines `main`.
- `linit.c` - replaced by `surge-lua-openlibs.c`, see below.
- `lbitlib.c` - Lua 5.2's `bit32`, which LuaJIT has no equivalent of.
- `lua.hpp` - a C++ convenience header; `LuaSupport.h` does its own `extern "C"`.

Also dropped: upstream's makefile, most of `doc/`, and Lua BitOp's makefiles, docs
and its benchmark and test scripts. `README` is kept in both, plus
`lua-5.2.4/doc/readme.html` because that is the file upstream's README points at
for the licence.

## Why `bit` needs Lua BitOp

Surge's formula sandbox whitelists `bit` alongside `math`, `string` and `table`
(see `sandboxLibraryTables` in `src/common/LuaSupport.cpp`). `bit` is built into
LuaJIT but is not part of stock Lua, so without it any formula calling `bit.band`
would break on exactly the platforms this backend serves.

Lua BitOp is the right source for it rather than a hand-rolled C++ implementation:
LuaJIT's built-in `bit` *is* Lua BitOp, same author and same semantics, so a
formula behaves identically on both backends instead of relying on Surge to
reproduce int32 wraparound, shift-count masking and `tobit`/`tohex`/`bswap`/`rol`
exactly. `UnitTestsLUA.cpp` already exercises `bit.tobit(0xffffffff)`, which makes
the existing test suite a real parity gate.

## `surge-lua-openlibs.c`

Upstream's `linit.c` with Lua BitOp's `bit` added to the libraries that
`luaL_openlibs` opens and `bit32` removed, so the set a formula sees matches
LuaJIT's. Doing it here means none of Surge's `lua_State` creation sites - three in
`src`, fourteen in the tests - need to know which backend they are talking to.

It is a separate file rather than a patch to `linit.c` so that every file under
`lua-5.2.4/` stays verifiably unmodified upstream.

## The sandbox on 5.2

`setSurgeFunctionEnvironment` builds a table of permitted globals and attaches it
as the function's environment. On 5.1 that is `lua_setfenv`; 5.2 removed it, and
the environment is the function's `_ENV` upvalue instead.

The 5.2 path cannot simply set upvalue 1. The function being sandboxed is an inner
named function pulled out by `parseStringDefiningFunction`, not a main chunk, so
`_ENV` is wherever the compiler happened to put it - and a formula that never reads
a global has no `_ENV` upvalue at all, in which case there is nothing to attach and
nothing to sandbox. So the 5.2 branch searches the upvalues by name.

## Invariant: Lua C functions must stay free of RAII

Both backends propagate errors out of a `lua_CFunction` with `longjmp`, so C++
destructors in that function's frame **do not run**. Surge's Lua C functions
already respect this - look at `pffftTransform` in `src/common/LuaSupport.cpp`,
which frees every buffer by hand before each `luaL_error` - and new ones must too.

Building the vendored sources as C++ would switch Lua to `throw`/`catch` and make
this moot, but it would also give the whole Lua API C++ linkage and break the
`extern "C"` include block in `LuaSupport.h`, so the sources are compiled as C
exactly as upstream intends.

## Performance

`Formula::valueAt` runs one `lua_pcall` per voice per block, so interpreter speed
is the real cost of this backend. Measured on x86_64 with both libraries built as
Surge builds them, calling a representative formula (two sines, a multiply and a
clamp) two million times, with the function and state table held in the registry
the way Surge holds them:

| | per evaluation |
|---|---|
| LuaJIT | ~125 ns |
| Lua 5.2.4 | ~485 ns |

So roughly **4x** slower per evaluation. Both produced bit-identical results over
the run, which is also a useful equivalence check. Lua 5.1.5 measured the same as
5.2.4 on the same harness, so moving up a version for the language parity above
cost nothing in speed.

What that means in practice: at 48 kHz with `BLOCK_SIZE` 32 there are 1500
evaluations per second per voice that is running a formula, so one such voice costs
about 0.07% of a core on this backend against 0.02% on LuaJIT. A patch with 96
simultaneous formula-driven voices is therefore nearer 7% of a core than 2%. The
multiplier is large but the base is small, so it only becomes interesting at high
voice and LFO counts.

Two caveats on those figures. They exclude Surge's own C++ marshalling around the
call, which is identical on both backends, so the end-to-end ratio in Surge is
lower than 4x while the absolute per-call cost is higher than shown. And they are
x86_64 numbers measured for want of ARM hardware; they describe the interpreters,
not the ARM targets this backend actually ships on.

## Updating

1. Download the new tarball and check its hash.
2. Replace the directory contents, keeping the omissions listed above, and rename
   the directory to the new version.
3. Update the paths and the source list in `CMakeLists.txt`. Sources are listed
   explicitly rather than globbed so that a new file is a deliberate, reviewable
   change.
4. Update the table and hashes above.
5. Build with `-DSURGE_LUA_IMPL=puc` and run `surge-testrunner`, then do the same
   with `-DSURGE_LUA_IMPL=luajit`. The suite passes unmodified on both, and that is
   the check that matters.
6. If the Lua version changes, re-check the language contract table above. A newer
   Lua may add syntax LuaJIT does not have, which widens the asymmetry rather than
   narrowing it.
