#include "UserInteractions.h"
#include <iostream>
#include <iomanip>

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
}
    
};
};

