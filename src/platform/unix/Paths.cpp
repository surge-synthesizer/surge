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
#include <cstdlib>
#include <stdexcept>
#include <dlfcn.h>

namespace Surge::Paths
{

fs::path binaryPath()
{
    Dl_info info;
    if (!dladdr(reinterpret_cast<const void *>(&binaryPath), &info) || !info.dli_fname[0])
    {
        // If dladdr(3) returns zero, dlerror(3) won't know why either
        throw std::runtime_error{"Failed to retrieve shared object file name"};
    }
    return fs::path{info.dli_fname};
}

fs::path homePath()
{
    const char *const path = std::getenv("HOME");
    if (!path || !path[0])
        throw std::runtime_error{"The environment variable HOME is unset or empty"};
    return fs::path{path};
}

} // namespace Surge::Paths
