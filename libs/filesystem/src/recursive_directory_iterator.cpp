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

#include "filesystem/recursive_directory_iterator.h"
#include "filesystem/filesystem.h"
#include "filesystem/directory_iterator.h"

#include <stack>

namespace Surge { namespace filesystem {

struct recursive_directory_iterator::Impl
{
   std::stack<directory_iterator> stack;

   // If p refers to a non-empty directory, put it on the stack and return true.
   bool descend(const path& p)
   {
      std::error_code ec;
      directory_iterator it{p, ec};
      if (it != directory_iterator{})
      {
         stack.emplace(std::move(it));
         return true;
      }
      if (!ec || ec == std::errc::not_a_directory)
         return false;
      throw filesystem_error{"recursive directory iterator cannot open directory \"" + p.native() + '\"', ec};
   }

   explicit Impl(directory_iterator&& it)
   {
      stack.push(std::move(it));
   }
};

recursive_directory_iterator::recursive_directory_iterator(const path& p)
{
   std::error_code ec;
   directory_iterator it{p, ec};
   if (ec)
      throw filesystem_error{"recursive directory iterator cannot open directory \"" + p.native() + '\"', ec};
   if (it != directory_iterator{})
      d = std::make_shared<Impl>(std::move(it));
}

recursive_directory_iterator::recursive_directory_iterator(const path& p, std::error_code& ec)
{
   directory_iterator it{p, ec};
   if (!ec && it != directory_iterator{})
      d = std::make_shared<Impl>(std::move(it));
}

const recursive_directory_iterator::value_type& recursive_directory_iterator::operator*() const noexcept
{
   return *d->stack.top();
}

recursive_directory_iterator& recursive_directory_iterator::operator++()
{
   auto& stack = d->stack;

   if (stack.top() != directory_iterator{})
   {
      if (d->descend(stack.top()->path()))
         return *this;
      do
      {
         if (++stack.top() != directory_iterator{})
            return *this;
         stack.pop();
      } while (!stack.empty());
   }

   d.reset();
   return *this;
}

} // namespace filesystem

} // namespace Surge
