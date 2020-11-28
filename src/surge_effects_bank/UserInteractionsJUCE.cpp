#include "UserInteractions.h"
#include <iostream>
#include <iomanip>

#if MAC
#include <execinfo.h>
#endif

void headlessStackDump()
{
#if MAC
   void* callstack[128];
   int i, frames = backtrace(callstack, 128);
   char** strs = backtrace_symbols(callstack, frames);
   for (i = 1; i < 6; ++i) {
      fprintf( stderr, "StackTrace[%3d]: %s\n", i, strs[i] );
   }
   free(strs);

#endif
}

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
    headlessStackDump();
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

void promptInfo(const std::string &message, const std::string &title,
                SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge Info\n"
              << title << "\n"
              << message << "\n" << std::flush;
    headlessStackDump();
}


MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge OkCancel\n"
              << title << "\n"
              << message << "\n" 
              << "Returning CANCEL" << std::flush;
    headlessStackDump();
    return UserInteractions::CANCEL;
}

void openURL(const std::string &url)
{
}
void showHTML( const std::string &html)
{
    std::cerr << "SURGE HTML: " << html << std::endl;
}

void promptFileOpenDialog(const std::string& initialDirectory,
                          const std::string& filterSuffix,
                          const std::string& filterDescription,
                          std::function<void(std::string)> callbackOnOpen,
			  bool od, bool cd,
                          SurgeGUIEditor* guiEditor)
{
   UserInteractions::promptError("Open file dialog is not implemented in this version of Surge. Sorry!",
                                 "Unimplemented Function", guiEditor);
}
};

};
