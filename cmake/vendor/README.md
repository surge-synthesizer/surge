# Portable Lua sources

These are unmodified upstream source archives, used by `../portable-lua.cmake`.
The build verifies their SHA-256 hashes before extraction and uses local paths;
there is no network fallback for a missing or corrupt archive. Source snapshots
include both archives and these license notices.

| Archive | Upstream source | SHA-256 |
| --- | --- | --- |
| `lua-5.1.5.tar.gz` | https://www.lua.org/ftp/lua-5.1.5.tar.gz | `2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333` |
| `luabitop-81bb23b0e737805442033535de8e6d204d0e5381.tar.gz` | https://codeload.github.com/LuaDist/luabitop/tar.gz/81bb23b0e737805442033535de8e6d204d0e5381 | `d483d0241f907d26eb596ddb7ad5860686eb01974b32bcc8b566b4ec25811ce2` |

Lua 5.1.5 and Lua BitOp 1.0.2 use the MIT license. `Lua-LICENSE.txt` is copied
from the archive's `COPYRIGHT`; `BitOp-LICENSE.txt` is the license header from
the archive's `bit.c`. The original notices also remain inside the archives.
Surge's portable integration compiles these sources without modifying them.

This directory does not cover the licenses or source requirements of Surge's
other dependencies or its build toolchain.
