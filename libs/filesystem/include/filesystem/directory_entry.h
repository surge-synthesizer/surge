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

#include "filesystem/path.h"

namespace Surge { namespace filesystem {

class directory_entry //                                                  [fs.class.directory_entry]
{
public:
   // constructors and destructor                                                [fs.dir.entry.cons]
   directory_entry() = default;
   explicit directory_entry(const path& p) : pth(p) {}

   // modifiers                                                                  [fs.dir.entry.mods]
   const class path& path() const noexcept { return pth; }
   operator const class path& () const noexcept { return pth; }

private:
   class path pth;

   friend class directory_iterator;
};

} // namespace filesystem

} // namespace Surge
