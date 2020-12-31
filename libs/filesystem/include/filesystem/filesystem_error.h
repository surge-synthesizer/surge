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

#include <system_error>

namespace Surge { namespace filesystem {

class filesystem_error : public std::system_error //                     [fs.class.filesystem_error]
{
public:
   filesystem_error(const std::string& what_arg, std::error_code ec)
   : system_error(ec, std::string{"filesystem error: "} + what_arg) {}

   filesystem_error(int errnum, const std::string& what_arg) // non-standard convenience overload
   : filesystem_error(what_arg, std::error_code{errnum, std::generic_category()})
   {}
};

} // namespace filesystem

} // namespace Surge
