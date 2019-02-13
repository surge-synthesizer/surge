#include <Windows.h>
#include <string>
#include "UserInteractions.h"


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
            MessageBox(::GetActiveWindow(), 
                       message.c_str(), 
                       title.c_str(), 
                       MB_OK | MB_ICONERROR );
        }

        UserInteractions::MessageResult promptOKCancel(const std::string &message,
                                                       const std::string &title)
        {
            if (MessageBox(::GetActiveWindow(), 
                           message.c_str(), 
                           title.c_str(),
                           MB_YESNO | MB_ICONQUESTION) != IDYES)
                return UserInteractions::CANCEL;
            
            return UserInteractions::OK;
        }

        void openURL(const std::string &url)
        {
            ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        void openFolderInFileBrowser(const std::string& folder)
        {
           std::string url = "file://" + folder;
           UserInteractions::openURL(url);
        }
    };
};

