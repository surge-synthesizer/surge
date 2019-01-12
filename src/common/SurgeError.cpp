#include "SurgeError.h"

SurgeError::SurgeError(const std::string &message)
   : message(message)
{
}


const std::string &SurgeError::getMessage()
{
   return message;
}
