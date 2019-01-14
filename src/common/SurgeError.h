#pragma once

#include <string>

namespace Surge
{

class Error {
public:
   static const std::string DEFAULT_TITLE;
   Error(const std::string &message, const std::string &title = DEFAULT_TITLE );
   Error() = delete;

   const std::string &getMessage() const;
   const std::string &getTitle() const;

private:
   std::string message;
   std::string title;
};

};
