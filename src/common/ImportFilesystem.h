#pragma once

/*
** This file imports filesystem and sets the fs namespace to the correct
** thing. In a distant future where we are compiling with all our compilers
** and sdks set up to have std::filesystem this will go away. But while still
** supporting VS2017 and macos 10.12 and stuff we need it.
*/


#if USE_STD_FILESYSTEM
#include <filesystem>
namespace fs = std::filesystem;
#elif USE_STD_EXPERIMENTAL_FILESYSTEM
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#elif USE_STD_EXPERIMENTAL_FILESYSTEM_FROM_FILESYSTEM
#include <filesystem>
namespace fs = std::experimental::filesystem;
#elif USE_HOMEGROWN_FILESYSTEM || TARGET_RACK
#include <filesystem.h>
namespace fs = std::experimental::filesystem;
#else
#error FILESYSTEM is not configured by build system
#endif
