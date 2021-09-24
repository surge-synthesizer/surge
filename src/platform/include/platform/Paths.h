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

#pragma once

#include "filesystem/import.h"

namespace Surge::Paths
{

/// @return The path of the running Surge application / plugin itself. Equivalent to binaryPath(),
/// except on macOS, where it returns the bundle directory (.app, .component, ...)
fs::path appPath();

/// @return The path of the running Surge binary file itself (.exe, .dll, .so, ...)
fs::path binaryPath();

/// @return The directory where the running Surge application / plugin is installed
inline fs::path installPath() { return appPath().parent_path(); }

} // namespace Surge::Paths
