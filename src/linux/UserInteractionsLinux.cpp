#include "UserInteractions.h"
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <unistd.h>

namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge Error\n"
              << title << "\n"
              << message << "\n" << std::flush;
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge OkCancel\n"
              << title << "\n"
              << message << "\n"
              << "Returning OK" << std::flush;
    return UserInteractions::OK;
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
   UserInteractions::promptError("OpenFileDialog is unimplemented in this version of Surge. Sorry!",
                                 "Unimplemented Function", guiEditor);
}
};

};
