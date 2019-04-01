#include <Windows.h>
#include <string>
#include "UserInteractions.h"
#include "SurgeGUIEditor.h"

namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
    MessageBox(::GetActiveWindow(),
               message.c_str(),
               title.c_str(),
               MB_OK | MB_ICONERROR );
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
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

