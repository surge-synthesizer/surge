#include "UserInteractions.h"
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
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

// @jpcima: string quoting for shell commands to use with popen.
static std::string escapeForPosixShell(const std::string &str)
{
   std::string esc;
   esc.reserve(2 * str.size());

   // 1. if it's a newline, write it verbatim enclosed in single-quotes;
   // 2. if it's a letter or digit, write it verbatim;
   // 3. otherwise, prefix it with a backward slash.

   for (char c : str)
   {
      if (c == '\n')
         esc.append("'\n'");
      else
      {
         bool isAlphaNum = (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
         if (!isAlphaNum)
            esc.push_back('\\');
         esc.push_back(c);
      }
   }

   return esc;
}

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
         _exit(0);
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

void promptInfo(const std::string &message, const std::string &title,
                SurgeGUIEditor *guiEditor)
{
   if (vfork()==0)
   {
      if (execlp("zenity", "zenity",
                 "--info",
                 "--text", message.c_str(),
                 "--title", title.c_str(),
                 (char*)nullptr) < 0)
      {
         _exit(0);
      }
   }
   std::cerr << "Surge Error\n"
             << title << "\n"
             << message << "\n" << std::flush;
}

MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
   size_t pid = vfork();

   if (pid < 0)
   {
      std::cerr << "Surge Error: vfork has failed\n";
      return UserInteractions::CANCEL;
   }
   else if (pid == 0)
   {
      execlp("zenity", "zenity",
                 "--question",
                 "--text", message.c_str(),
                 "--title", title.c_str(),
                 (char*)nullptr);
      _exit(65);
   }

   int wret;
   int wstatus;
   while ((wret = waitpid(pid, &wstatus, 0)) == -1 && errno == EINTR)
      /* wait interrupted by signal, try again */;

   if (wret == -1)
   {
      std::cerr << "Surge Error: waitpid has failed, \"" << strerror(errno) << "\"\n";
      return UserInteractions::CANCEL;
   }

   auto status = WEXITSTATUS(wstatus);
   switch( status )
   {
      case 0:
      case 65:
         // zenity worked or was not installed
         return UserInteractions::OK;
      default:
         return UserInteractions::CANCEL;
   }
}

void openURL(const std::string &url)
{
   if (vfork()==0)
   {
      if (execlp("xdg-open", "xdg-open", url.c_str(), (char*)nullptr) < 0)
      {
         _exit(0);
      }
   }
}

void showHTML( const std::string &html )
{
    // FIXME - there's proper APIs for this that crash on MacOS 
    std::ostringstream fns;
    fns << "/tmp/surge-tuning." << rand() << ".html";

    FILE *f = fopen(fns.str().c_str(), "w" );
    if( f )
    {
        fprintf( f, "%s", html.c_str());
        fclose(f);
        std::string url = std::string("file://") + fns.str();
        openURL(url);
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
                          bool canSelectDirectories,
                          bool canCreateDirectories,
                          SurgeGUIEditor* guiEditor)
{
   /*
   ** This is a blocking model which will cause us problems I am sure
   */

   std::string zenityCommand;
   zenityCommand.reserve(1024);

   zenityCommand.append("zenity --file-selection");

   if (!initialDirectory.empty())
   {
      zenityCommand.append(" --filename=");
      zenityCommand.append(escapeForPosixShell(initialDirectory));
      zenityCommand.push_back('/');  // makes it handled as directory
   }

   if (!filterSuffix.empty())
   {
      zenityCommand.append(" --file-filter=");
      zenityCommand.append(escapeForPosixShell("*" + filterSuffix));
   }

   if (canSelectDirectories)
      zenityCommand.append(" --directory");

   // option not implemented
   (void)canCreateDirectories;

   FILE *z = popen( zenityCommand.c_str(), "r" );
   if( ! z )
   {
      return;
   }

   std::string result; // not forcing a fixed limit on length, depends on fs
   result.reserve(1024);

   char buffer[ 1024 ];
   for (size_t count; (count = fread(buffer, 1, sizeof(buffer), z)) > 0;)
      result.append(buffer, count);

   pclose(z);

   // eliminate the final newline
   if (!result.empty() && result.back() == '\n')
      result.pop_back();

   if (!result.empty()) // it's empty if the user cancelled
      callbackOnOpen(result);
}
};

};
