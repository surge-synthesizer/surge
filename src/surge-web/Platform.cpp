// SPDX-License-Identifier: GPL-3.0-or-later
#include <sst/plugininfra/paths.h>
#include <sst/plugininfra/misc_platform.h>
#include <sst/plugininfra/cpufeatures.h>
#include <cerrno>
#include <cstring>
namespace sst::plugininfra
{
namespace paths
{
fs::path homePath() { return "/user"; }
fs::path sharedLibraryBinaryPath() { return "/surge.js"; }
fs::path bestDocumentsVendorFolderPathFor(const std::string &, const std::string &)
{
    return "/user";
}
fs::path bestLibrarySharedVendorFolderPathFor(const std::string &, const std::string &, bool user)
{
    return user ? "/user/config" : "/factory";
}
} // namespace paths
namespace misc_platform
{
bool isDarkMode() { return true; }
void allocateConsole() {}
std::string toOSCase(const std::string &s) { return s; }
std::string stackTraceToString(int)
{
    return "See the Chrome developer console for the WebAssembly stack trace.";
}
std::string getLastSystemError() { return std::strerror(errno); }
} // namespace misc_platform
namespace cpufeatures
{
std::string brand() { return "WebAssembly SIMD128"; }
bool isArm() { return false; }
bool isX86() { return false; }
bool hasSSE2() { return true; } // SIMDe implements the DSP's SSE API.
bool hasAVX() { return false; }
FPUStateGuard::FPUStateGuard() = default;
FPUStateGuard::~FPUStateGuard() = default;
} // namespace cpufeatures
} // namespace sst::plugininfra
