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
#include "filesystem/directory_entry.h"

#include <iterator>
#include <memory>
#include <system_error>

namespace Surge { namespace filesystem {

class directory_iterator //                                            [fs.class.directory_iterator]
{
public:
   using iterator_category = std::input_iterator_tag;
   using value_type = directory_entry;
   using difference_type = ptrdiff_t;
   using pointer = const value_type*;
   using reference = const value_type&;

   // member functions                                                          [fs.dir.itr.members]
   directory_iterator() = default;
   explicit directory_iterator(const path& p);
   directory_iterator(const path& p, std::error_code& ec);

   const value_type& operator*() const noexcept;
   const value_type* operator->() const noexcept { return &**this; }

   directory_iterator& operator++();

private:
   struct Impl;
   std::shared_ptr<Impl> d;

   explicit directory_iterator(const path::string_type& p);

   friend bool operator==(const directory_iterator& lhs, const directory_iterator& rhs) noexcept
   { return lhs.d == rhs.d; }

   friend bool operator!=(const directory_iterator& lhs, const directory_iterator& rhs) noexcept
   { return !(lhs == rhs); }
};

// non-member functions                                                      [fs.dir.itr.nonmembers]
inline directory_iterator begin(directory_iterator it) noexcept { return it; }
inline directory_iterator end(const directory_iterator&) noexcept { return directory_iterator(); }

} // namespace filesystem

} // namespace Surge
