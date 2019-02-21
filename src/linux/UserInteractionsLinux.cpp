#include "UserInteractions.h"
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <unistd.h>

namespace Surge
{
    namespace UserInteractions
    {
        void promptError(const Surge::Error &e)
        {
            promptError(e.getMessage(), e.getTitle());
        }

        void promptError(const std::string &message,
                         const std::string &title)
        {
            std::cerr << "Surge Error\n"
                      << title << "\n"
                      << message << "\n" << std::flush;
        }

        UserInteractions::MessageResult promptOKCancel(const std::string &message,
                                                       const std::string &title)
        {
            std::cerr << "Surge OkCancel\n"
                      << title << "\n"
                      << message << "\n" 
                      << "Returning CANCEL" << std::flush;
            return UserInteractions::CANCEL;
        }

        void openURL(const std::string &url)
        {
           if (vfork()==0)
           {
              if (execlp("xdg-open", "xdg-open", url.c_str(), (char*)nullptr) < 0)
              {
                 exit(0);
              }
           }
        }
    };
};

