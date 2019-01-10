#pragma once

#include <string>

class SurgeError {
public:
   SurgeError(const std::string &message);
   SurgeError() = delete;

   const std::string &getMessage();

private:
   std::string message;
};
