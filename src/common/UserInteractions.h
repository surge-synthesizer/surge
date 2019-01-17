/* 
**  SurgeMessage provides basic functions for the synth to report messages back to the user
**  with a platform-neutral API. A platform-specific version of each implementation is
**  registered at startup.
**
**  You can call this with
**
**    Surge::UserInteractions::reportProblem()
**
**  or what not. 
*/

#pragma once

#include <string>
#include <atomic>
#include "SurgeError.h"

namespace Surge
{
    namespace UserInteractions
    {
        // Show the user an error dialog with an OK button; and wait for them to press it
        void promptError(const std::string &message, const std::string &title);

        // And a convenience version which does the same from a Surge::Error
        void promptError(const Surge::Error &e);

        // Prompt the user with an OK/Cance
        typedef enum MessageResult
        {
            OK,
            CANCEL
        } MessageResult;
        MessageResult promptOKCancel(const std::string &message, const std::string &title);

        // Open a URL in a user-appropriate fashion
        void openURL(const std::string &url);
    };
};


