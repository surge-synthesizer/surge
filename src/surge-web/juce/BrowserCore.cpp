// SPDX-License-Identifier: GPL-3.0-or-later
// Embedded EM_ASM JavaScript must not be reformatted as C++.
// clang-format off
// Included in JUCE's core translation unit after its POSIX helpers.
#include "juce_core/native/juce_CommonFile_linux.cpp"
#include "juce_core/native/juce_Threads_linux.cpp"
namespace juce
{
File File::getSpecialLocation(SpecialLocationType type)
{
    switch (type)
    {
    case tempDirectory:
        return File("/tmp");
    case currentExecutableFile:
    case currentApplicationFile:
    case invokedExecutableFile:
        return File("/surge.js");
    case commonApplicationDataDirectory:
        return File("/factory");
    default:
        return File("/user");
    }
}
bool File::isOnCDRomDrive() const { return false; }
bool File::isOnHardDisk() const { return false; }
bool File::isOnRemovableDrive() const { return false; }
String File::getVersion() const { return {}; }
bool File::moveToTrash() const { return deleteFile(); }
void File::revealToUser() const {}
bool Process::openDocument(const String &url, const String &)
{
    if (!url.startsWith("https://") && !url.startsWith("http://"))
        return false;
    return EM_ASM_INT(
        { return window.open(UTF8ToString($0), '_blank', 'noopener') !== null; }, url.toRawUTF8());
}
} // namespace juce
// Direct UDP is a desktop-only integration. Fail explicitly instead of routing
// it through Emscripten's WebSocket socket emulation or a companion service.
namespace juce
{
DatagramSocket::DatagramSocket(bool, const SocketOptions &o) : options(o) {}
DatagramSocket::~DatagramSocket() = default;
bool DatagramSocket::bindToPort(int) { return false; }
bool DatagramSocket::bindToPort(int, const String &) { return false; }
int DatagramSocket::getBoundPort() const noexcept { return -1; }
int DatagramSocket::waitUntilReady(bool, int) { return -1; }
int DatagramSocket::read(void *, int, bool) { return -1; }
int DatagramSocket::read(void *, int, bool, String &, int &) { return -1; }
int DatagramSocket::write(const String &, int, const void *, int) { return -1; }
void DatagramSocket::shutdown() {}
bool DatagramSocket::joinMulticast(const String &) { return false; }
bool DatagramSocket::leaveMulticast(const String &) { return false; }
bool DatagramSocket::setMulticastLoopbackEnabled(bool) { return false; }
bool DatagramSocket::setEnablePortReuse(bool) { return false; }
} // namespace juce

// clang-format on
