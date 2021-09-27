/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "platform/Paths.h"
#include <system_error>
#include <windows.h>
#include <shlobj.h>

namespace
{

fs::path knownFolderPath(REFKNOWNFOLDERID rfid)
{
    fs::path path;
    PWSTR pathStr{};
    if (::SHGetKnownFolderPath(rfid, 0, nullptr, &pathStr) == S_OK)
        path = pathStr;
    ::CoTaskMemFree(pathStr);
    if (path.empty())
        throw std::runtime_error{"Failed to retrieve known folder path"};
    return path;
}

} // anonymous namespace

namespace Surge::Paths
{

fs::path binaryPath()
{
    HMODULE hmodule;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&binaryPath), &hmodule))
    {
        throw std::system_error{int(::GetLastError()), std::system_category(),
                                "Failed to retrieve module handle"};
    }

    for (std::vector<WCHAR> buf{MAX_PATH};; buf.resize(buf.size() * 2))
    {
        const auto len = ::GetModuleFileNameW(hmodule, &buf[0], buf.size());
        if (!len)
            throw std::system_error{int(::GetLastError()), std::system_category(),
                                    "Failed to retrieve module file name"};
        if (len < buf.size())
            return fs::path{&buf[0]};
    }
}

fs::path homePath() { return knownFolderPath(FOLDERID_Profile); }

} // namespace Surge::Paths
