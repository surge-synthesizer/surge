# luajitlib

luajitlib is a collection of scripts and a CMake configuration that attempts to build LuaJIT using its makefile build system so it can be integrated as a submodule within Surge's CMake-based build automation system.

## Troubleshooting

CMake should output a series of debug messages and a general error message if a build fails. Examining these messages is the first step to troubleshoot why a build failed. See the sections below for more information on specific use scenarios and platforms and refer to the LuaJIT documentation [here](https://luajit.org/install.html) for more information.

Alternatively, a pre-built statically linked library can be placed directly in the build output directory at `/libs/luajitlib/bin` with the header files in `/libs/luajitlib/include`. If named either `libluajit.a` or `lua51.lib`, it should be automatically detected and used instead of compiling LuaJIT from source during the CMake configuration process.

Finally, if all other options fail, Lua support can be disabled altogether by adding `-DSURGE_SKIP_LUA=TRUE` in the CMake configuration options.

### Cross-compiling

LuaJIT's makefiles try to auto-detect the settings needed for the host operating system and compiler. Therefore, luajitlib does not support non-native cross-compiling without a custom script. See [here](https://luajit.org/install.html#cross) for more information.

### macOS

luajitlib attempts to build both `arm64` and `x86_64` binaries using different make commands. The `lipo` tool creates a universal binary by merging these together. Unlike the other scripts, LuaJIT is built from the top `src/` directory, as the top makefile also has Darwin specific configuration. LuaJIT requires the environment variable `MACOSX_DEPLOYMENT_TARGET`
to be set to a value supported by the toolchain. The CMake configuration does not recognize `CMAKE_OSX_ARCHITECTURES`.

### *nix

On *nix-like platforms (including macOS), the default is to use the `amalg` build target, which allows for better optimization at the cost of increased memory use during the compiling process. Removing the `amalg` option in the build scripts could help prevent memory use related issues on some systems.

### Windows + MSVC

luajitlib uses its own custom version of the `msvcbuild.bat` script included with LuaJIT to build with static linking using the compiler flag `/MT` instead of `/MD`. See [here](https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=msvc-170&viewFallbackFrom=vs-2019) for more information.

### MinGW

LuaJIT is best built using a shell-specific version of `mingw32-make` rather than the regular MSYS2 version of `make` to create a statically linked library. See [here](https://www.msys2.org/wiki/Porting/) for more information.

## Support

To request support, open an issue on Surge XT's GitHub page or hop into the #help channel of the Surge Discord. If you encounter an unsupported platform or have a CMake configuration and shell script that works for a specific build environment, please let us know on GitHub so we can attempt to add it.