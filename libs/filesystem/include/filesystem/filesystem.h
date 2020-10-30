/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include "filesystem/filesystem_error.h"
#include "filesystem/path.h"
#include "filesystem/directory_iterator.h"
#include "filesystem/recursive_directory_iterator.h"

#include <cstdint>

namespace Surge { namespace filesystem {

// path factory functions                                                          [fs.path.factory]
template<class Source>
inline path u8path(const Source& source) { return path{source}; }

// filesystem operations                                                               [fs.op.funcs]
bool create_directories(const path& p);
bool create_directory(const path& p);
bool exists(const path& p);
std::uintmax_t file_size(const path& p);
std::uintmax_t file_size(const path& p, std::error_code& ec) noexcept;
bool is_directory(const path& p);
bool is_regular_file(const path& p);
bool remove(const path& p);
std::uintmax_t remove_all(const path& p);

} // namespace filesystem

} // namespace Surge
