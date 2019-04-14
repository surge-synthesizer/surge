#include <Windows.h>
#include <Commdlg.h>
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

void promptFileOpenDialog(const std::string& initialDirectory,
                          const std::string& filterSuffix,
                          std::function<void(std::string)> callbackOnOpen,
                          SurgeGUIEditor* guiEditor)
{
   // With many thanks to
   // https://www.daniweb.com/programming/software-development/code/217307/a-simple-getopenfilename-example
   char szFile[1024];
   OPENFILENAME ofn;
   ZeroMemory(&ofn, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = NULL;
   ofn.lpstrFile = szFile;
   ofn.lpstrFile[0] = '\0';
   ofn.nMaxFile = sizeof(szFile);
   ofn.lpstrFilter = "All\0*.*\0";
   ofn.nFilterIndex = 0;
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = NULL;
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   if (GetOpenFileName(&ofn))
   {
      callbackOnOpen(ofn.lpstrFile);
   }
}
};

};

