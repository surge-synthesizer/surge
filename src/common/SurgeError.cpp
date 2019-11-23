//-------------------------------------------------------------------------------------------------------
//  Copyright 2005-2020, various authors, as described in AUTHORS.
//  Released under the terms of the Gnu General Public License version 3.0 or greater as described in LICENSE.md
//-------------------------------------------------------------------------------------------------------
#include "SurgeError.h"


namespace Surge
{
    const std::string Error::DEFAULT_TITLE = "An error has occured";

    Error::Error(const std::string &message, const std::string &title)
        : message(message), title(title)
    {
    }
    
    
    const std::string &Error::getMessage() const
    {
        return message;
    }
    
    const std::string &Error::getTitle() const
    {
        return title;
    }
};
