#include "UserInteractions.h"
#include <iostream>
#include <iomanip>

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

void promptInfo(const std::string &message, const std::string &title,
                SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge Info\n"
              << title << "\n"
              << message << "\n" << std::flush;
}


MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
    std::cerr << "Surge OkCancel\n"
              << title << "\n"
              << message << "\n" 
              << "Returning CANCEL" << std::flush;
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
                          std::function<void(std::string)> callbackOnOpen,
			  bool od, bool cd,
                          SurgeGUIEditor* guiEditor)
{
   UserInteractions::promptError("OpenFileDialog is unimplemented in this version of Surge. Sorry!",
                                 "Unimplemented Function", guiEditor);
}
};

};
