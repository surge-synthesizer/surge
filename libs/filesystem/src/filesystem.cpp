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

#include "filesystem/filesystem.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Surge { namespace filesystem {

namespace {
bool is_type_set_errno(char const* p, mode_t typemask)
{
   errno = 0;
   struct stat statbuf;
   return !::stat(p, &statbuf) && ((statbuf.st_mode & S_IFMT) == typemask);
}

bool is_type(const path& p, mode_t typemask)
{
   if (is_type_set_errno(p.c_str(), typemask))
      return true;
   if (!errno || errno == ENOENT || errno == ENOTDIR)
      return false;
   throw filesystem_error{errno, std::string{"could not get type of path \""} + p.native() + '\"'};
}

bool do_create_directory(const char* p)
{
   if (!::mkdir(p, 0777))
      return true;
   if (errno == EEXIST)
      return false;
   throw filesystem_error{errno, std::string{"could not create directory \""} + p + '\"'};
}
} // anonymous namespace

bool create_directories(const path& p)
{
   auto pth{p.native()};
   auto* const begin = &*pth.begin();
   auto* end = begin;
   while (*++end == path::preferred_separator) {}
   while (end && (end = std::strchr(end, path::preferred_separator)))
   {
      *end = '\0';
      if (!do_create_directory(begin) && !is_type_set_errno(begin, S_IFDIR))
         throw filesystem_error{ENOTDIR, std::string{"could not create directories \""} + p.native() + '\"'};
      *end = path::preferred_separator;
      while (*++end == path::preferred_separator) {}
   }
   if (do_create_directory(begin))
      return true;
   if (is_type_set_errno(p.c_str(), S_IFDIR))
      return false;
   throw filesystem_error{EEXIST, std::string{"could not create directories \""} + p.native() + '\"'};
}

bool create_directory(const path& p)
{
   return do_create_directory(p.c_str());
}

bool exists(const path& p)
{
   if (!::access(p.c_str(), F_OK))
      return true;
   if (errno == ENOENT || errno == ENOTDIR)
      return false;
   throw filesystem_error{errno, std::string{"could not access \""} + p.native() + '\"'};
}

std::uintmax_t file_size(const path& p)
{
   std::error_code ec;
   const auto result = file_size(p, ec);
   if (!ec)
      return result;
   throw filesystem_error{std::string{"could not get size of file \""} + p.native() + '\"', ec};
}

std::uintmax_t file_size(const path& p, std::error_code& ec) noexcept
{
   struct stat statbuf;
   if (::stat(p.c_str(), &statbuf))
      ec.assign(errno, std::system_category());
   else if (S_ISREG(statbuf.st_mode))
   {
      ec.clear();
      return std::uintmax_t(statbuf.st_size);
   }
   else if (S_ISDIR(statbuf.st_mode))
      ec.assign(EISDIR, std::generic_category());
   else
      ec.assign(ENOTSUP, std::generic_category());
   return std::uintmax_t(-1);
}

bool is_directory(const path& p)
{
   return is_type(p, S_IFDIR);
}

bool is_regular_file(const path& p)
{
   return is_type(p, S_IFREG);
}

bool remove(const path& p)
{
   if (!std::remove(p.c_str()))
      return true;
   if (errno == ENOENT)
      return false;
   throw filesystem_error{errno, std::string{"could not remove path \""} + p.native() + '\"'};
}

std::uintmax_t remove_all(const path& p)
{
   std::uintmax_t result{};
   std::error_code ec;
   directory_iterator it{p, ec};
   if (it != directory_iterator{})
   {
      for (const path& subp : it)
         result += remove_all(subp);
   }
   // Ignoring permission_denied on the next line would delete empty directories just fine even
   // without permissions, but libc++ throws here, so we do too...
   else if (ec && ec != std::errc::not_a_directory && ec != std::errc::no_such_file_or_directory)
      throw filesystem_error{std::string{"could not remove path \""} + p.native() + '\"', ec};
   if (remove(p))
      ++result;
   return result;
}

} // namespace filesystem

} // namespace Surge
