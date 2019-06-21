#include "UserInteractions.h"
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <unistd.h>
#include <thread>

/*
** In June 2019, @baconpaul chose to implement these with an attempt
** to fork/exec a zenity. (He also did this for mini edit). This is not
** ideal. Properly you would somehow asynchronously interact with 
** vstgui and so on, which @jjs started. But we kinda gotta do something.
**
** If you want to come along and rip out these zenity calls and replace them
** with vstgui, please do so! This is purely tactical stuff to get us
** not silently failing.
*/
namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
   if (vfork()==0)
   {
      if (execlp("zenity", "zenity",
                 "--error",
                 "--text", message.c_str(),
                 "--title", title.c_str(),
                 (char*)nullptr) < 0)
      {
         exit(0);
      }
   }
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
   /*
   ** This is a blocking model which will cause us problems I am sure
   */
   FILE *z = popen( "zenity --file-selection", "r" );
   if( ! z )
   {
      return;
   }
   char buffer[ 1024 ];
   if (fgets(buffer, sizeof(buffer), z) == NULL)
   {
      return;
   }
   for( int i=0; i<1024; ++i )
      if( buffer[i] == '\n' )
         buffer[i] = 0;
   
   pclose(z);

   callbackOnOpen(buffer);
}
};

};
