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

#include <iosfwd>
#include <string>
#include <utility>

namespace Surge { namespace filesystem {

class path //                                                                        [fs.class.path]
{
public:
   using value_type = char;
   using string_type = std::basic_string<value_type>;

   static constexpr value_type preferred_separator = '/';

   // constructors and destructor                                                [fs.path.construct]
   path() = default;
   explicit path(string_type p) noexcept // Hint: Did you mean to use string_to_path()?
   : pth(std::move(p)) {}

   // appends                                                                       [fs.path.append]
   path& operator/=(const path& rhs);

   // modifiers                                                                  [fs.path.modifiers]
   void clear() noexcept { pth.clear(); }
   path& make_preferred() noexcept { return *this; }
   path& remove_filename();
   path& replace_filename(const path& replacement);
   path& replace_extension(const path& replacement = path{});

   // concatenation                                                                 [fs.path.concat]
   path& operator+=(const path& p) { pth += p.pth; return *this; }

   // native format observers                                                   [fs.path.native.obs]
   const string_type& native() const noexcept { return pth; }
   const value_type* c_str() const noexcept { return pth.c_str(); }
   operator string_type() const { return pth; }
   const std::string& string() const noexcept { return pth; }
   const std::string& u8string() const noexcept { return pth; }

   // generic format observers                                                 [fs.path.generic.obs]
   const std::string& generic_string() const noexcept { return pth; }
   const std::string& generic_u8string() const noexcept { return pth; }

   // decomposition                                                              [fs.path.decompose]
   path filename() const;
   path stem() const;
   path extension() const;

   // query                                                                          [fs.path.query]
   bool empty() const noexcept { return pth.empty(); }
   bool has_filename() const noexcept { return !pth.empty() && pth.back() != preferred_separator; }
   bool has_stem() const noexcept;
   bool has_extension() const noexcept;
   bool is_absolute() const noexcept { return !pth.empty() && pth.front() == preferred_separator; }
   bool is_relative() const noexcept { return !is_absolute(); }

   friend path operator/(const path& lhs, const path& rhs) { return path(lhs) /= rhs; }
   friend std::ostream& operator<<(std::ostream& os, const path& p);

private:
   string_type pth;

   friend class directory_iterator;
};

} // namespace filesystem

} // namespace Surge
