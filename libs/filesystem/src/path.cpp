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

#include "filesystem/path.h"

#include "util.h"

#include <iomanip>
#include <iostream>

namespace Surge { namespace filesystem {

namespace {
// No string_view in C++14 :(
struct slice
{
   const path::value_type* const p;
   const path::string_type::size_type len;

   bool empty() const noexcept { return !len; }

   path to_path() const
   {
      if (empty())
         return path{};
      return path{path::string_type{p, len}};
   }
};

const path::value_type* filename_internal(const path& p) noexcept
{
   const auto& s = p.native();
   const auto slash = s.find_last_of(path::preferred_separator);
   if (slash == path::string_type::npos)
      return s.c_str();
   return s.c_str() + slash + 1;
}

slice stem_internal(const path& p) noexcept
{
   const auto* filename = filename_internal(p);
   const auto& pth = p.native();
   if (!is_dot_or_dotdot(filename))
   {
      for (const auto* x = &*pth.end(); x > filename; --x)
         if (*x == '.')
            return slice{filename, path::string_type::size_type(x - filename)};
   }
   return slice{filename, path::string_type::size_type(&*pth.end() - filename)};
}

const path::value_type* extension_internal(const path& p) noexcept
{
   const auto* filename = filename_internal(p);
   const auto& pth = p.native();
   auto period_idx = pth.find_last_of('.');
   if (period_idx == path::string_type::npos)
      return nullptr;
   const auto* period = pth.c_str() + period_idx;
   if (period <= filename)
      return nullptr;
   if (is_dot_or_dotdot(filename))
      return nullptr;
   while (period[1] == '.')
      ++period;
   return period;
}
} // anonymous namespace

path& path::operator/=(const path& rhs)
{
   if (rhs.is_absolute())
      pth = rhs.pth;
   else
   {
      if (pth.back() != preferred_separator)
         pth.push_back(preferred_separator);
      pth.append(rhs.pth);
   }
   return *this;
}

path& path::remove_filename()
{
   pth.resize(filename_internal(*this) - pth.c_str());
   return *this;
}

path& path::replace_filename(const path& replacement)
{
   if (replacement.is_absolute())
      clear();
   else
      remove_filename();
   pth += replacement.pth;
   return *this;
}

path& path::replace_extension(const path& replacement)
{
   if (auto* const pext = extension_internal(*this))
      pth.resize(pext - pth.c_str());
   if (!replacement.empty())
   {
      if (replacement.pth.front() != '.')
         pth.push_back('.');
      pth += replacement.pth;
   }
   return *this;
}

path path::filename() const
{
   return path{filename_internal(*this)};
}

path path::stem() const
{
   return stem_internal(*this).to_path();
}

path path::extension() const
{
   if (auto* const pext = extension_internal(*this))
      return path{pext};
   return path{};
}

bool path::has_stem() const noexcept
{
   return !stem_internal(*this).empty();
}

bool path::has_extension() const noexcept
{
   return !!extension_internal(*this);
}

std::ostream& operator<<(std::ostream& os, const path& p)
{
   os << std::quoted(p.pth);
   return os;
}

} // namespace filesystem

} // namespace Surge
