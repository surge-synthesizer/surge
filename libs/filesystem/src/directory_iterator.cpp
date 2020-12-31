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

#include "filesystem/directory_iterator.h"
#include "filesystem/filesystem.h"

#include <cerrno>
#include <climits>

#include <sys/types.h>
#include <dirent.h>

#include "util.h"

namespace Surge { namespace filesystem {

namespace {
struct DirDeleter
{
   void operator()(DIR* dirp)
   {
      if (dirp)
         ::closedir(dirp);
   }
};
} // anonymous namespace

using dir_unique_ptr = std::unique_ptr<DIR, DirDeleter>;

struct directory_iterator::Impl
{
   value_type entry;
   dir_unique_ptr dirp;
   const path::string_type::size_type pathlen_with_slash;
   const path::string_type::size_type pathlen_user;

   Impl(dir_unique_ptr&& dirp, const path::string_type& p)
   : dirp{std::move(dirp)}
   , pathlen_with_slash{p.size() + (p.back() != path::preferred_separator)}
   , pathlen_user{p.size()}
   {
      entry.pth.pth.reserve(pathlen_with_slash + NAME_MAX + 1);
      entry.pth.pth.append(p);
      if (pathlen_user != pathlen_with_slash)
         entry.pth.pth.push_back(path::preferred_separator);
   }
};

directory_iterator::directory_iterator(const path::string_type& p)
{
   if (dir_unique_ptr dirp{::opendir(p.c_str())})
      d = std::make_shared<Impl>(std::move(dirp), p);
}

directory_iterator::directory_iterator(const path& p)
: directory_iterator{p.native()}
{
   if (!d)
      throw filesystem_error{errno,
                             "directory iterator cannot open directory \"" + p.native() + '\"'};
   ++*this;
}

directory_iterator::directory_iterator(const path& p, std::error_code& ec)
: directory_iterator{p.native()}
{
   if (d)
      ++*this;
   else
      ec = std::error_code{errno, std::system_category()};
}

const directory_iterator::value_type& directory_iterator::operator*() const noexcept
{
   return d->entry;
}

directory_iterator& directory_iterator::operator++()
{
   errno = 0;
   while (auto* const dirent = ::readdir(d->dirp.get()))
   {
      if (is_dot_or_dotdot(dirent->d_name))
         continue;
      d->entry.pth.pth.resize(d->pathlen_with_slash);
      d->entry.pth.pth.append(dirent->d_name);
      return *this;
   }

   if (const auto errno_copy{errno})
   {
      d->entry.pth.pth.resize(d->pathlen_user);
      const auto impl{std::move(d)};
      throw filesystem_error{errno_copy,
                             "directory iterator cannot read directory \"" + impl->entry.pth.pth + '\"'};
   }

   d.reset();
   return *this;
}

} // namespace filesystem

} // namespace Surge
