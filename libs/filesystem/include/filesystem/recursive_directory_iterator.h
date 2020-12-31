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

#include "filesystem/directory_entry.h"

#include <memory>
#include <system_error>

namespace Surge { namespace filesystem {

class recursive_directory_iterator //                                         [fs.class.rec.dir.itr]
{
public:
   using iterator_category = std::input_iterator_tag;
   using value_type = directory_entry;
   using difference_type = ptrdiff_t;
   using pointer = const value_type*;
   using reference = const value_type&;

   // constructors and destructor                                           [fs.rec.dir.itr.members]
   recursive_directory_iterator() = default;
   explicit recursive_directory_iterator(const path& p);
   recursive_directory_iterator(const path& p, std::error_code& ec);

   // observers
   const value_type& operator*() const noexcept;
   const value_type* operator->() const noexcept { return &**this; }

   // modifiers
   recursive_directory_iterator& operator++();

private:
   struct Impl;
   std::shared_ptr<Impl> d;

   friend bool operator==(const recursive_directory_iterator& lhs,
                          const recursive_directory_iterator& rhs) noexcept
   { return lhs.d == rhs.d; }

   friend bool operator!=(const recursive_directory_iterator& lhs,
                          const recursive_directory_iterator& rhs) noexcept
   { return !(lhs == rhs); }
};

// non-member functions                                                  [fs.rec.dir.itr.nonmembers]
inline recursive_directory_iterator begin(recursive_directory_iterator it) noexcept
{ return it; }

inline recursive_directory_iterator end(const recursive_directory_iterator&) noexcept
{ return recursive_directory_iterator(); }

} // namespace filesystem

} // namespace Surge
