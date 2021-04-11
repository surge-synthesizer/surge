#include "UserInteractions.h"
#include "SurgeSynthInteractionsInterface.h"
#include <iostream>

#if MAC
#include <execinfo.h>
#endif

std::atomic<SurgeSynthInteractionsInterface *> SurgeSynthInteractionsInterface::impl(0);

void headlessStackDump()
{
#if MAC
    void *callstack[128];
    int i, frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);
    for (i = 1; i < 6; ++i)
    {
        std::cout << "StackTrace[" << i << "]" << strs[i] << std::endl;
    }
    free(strs);

#endif
}

namespace Surge
{
namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title, SurgeGUIEditor *guiEditor)
{
    if (SurgeSynthInteractionsInterface::impl.load())
    {
        SurgeSynthInteractionsInterface::impl.load()->promptError(message, title, guiEditor);
    }
    else
    {
        std::cerr << "SurgeXT Error[" << title << "]\n" << message << std::endl;
        headlessStackDump();
    }
}

void promptInfo(const std::string &message, const std::string &title, SurgeGUIEditor *guiEditor)
{
    if (SurgeSynthInteractionsInterface::impl.load())
    {
        SurgeSynthInteractionsInterface::impl.load()->promptInfo(message, title, guiEditor);
    }
    else
    {
        std::cerr << "SurgeXT Info\n" << title << "\n" << message << "\n" << std::flush;
        headlessStackDump();
    }
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
    if (SurgeSynthInteractionsInterface::impl.load())
    {
        SurgeSynthInteractionsInterface::impl.load()->openURL(url);
    }
}
void showHTML(const std::string &html)
{
    if (SurgeSynthInteractionsInterface::impl.load())
    {
        SurgeSynthInteractionsInterface::impl.load()->showHTML(html);
    }
}

void promptFileOpenDialog(const std::string &initialDirectory, const std::string &filterSuffix,
                          const std::string &filterDescription,
                          std::function<void(std::string)> callbackOnOpen, bool od, bool cd,
                          SurgeGUIEditor *guiEditor)
{
    if (SurgeSynthInteractionsInterface::impl.load())
    {
        SurgeSynthInteractionsInterface::impl.load()->promptFileOpenDialog(
            initialDirectory, filterSuffix, filterDescription, callbackOnOpen, od, cd, guiEditor);
    }
}

}; // namespace UserInteractions

}; // namespace Surge
