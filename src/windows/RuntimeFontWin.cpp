#include <windows.h>
#include <dwrite.h>
#include <stdio.h>

#include <filesystem>

#include "RuntimeFont.h"
#include "UserInteractions.h"

#include "resource.h"
#include "vstgui/lib/platform/win32/win32support.h"

#include <list>

using namespace VSTGUI;

namespace Surge
{
namespace GUI
{

bool initializeRuntimeFontForOS()
{
#if !TARGET_JUCE_UI
    /*
    ** In Feb 2019 we removed our ability to load the font from the bundle and instead
    ** test if it is installed; if it is we use it; if it is not we don't.
    ** We also modified the innosetup installer to install it.
    */
    std::list<std::string> fontNames;
    if (IPlatformFont::getAllPlatformFontFamilies(fontNames))
    {
        if (std::find(fontNames.begin(), fontNames.end(), "Lato") == fontNames.end())
        {
            /*
            ** Lato is not installed; bail out
            */
            return false;
        }
    }
#endif
    return true;
}

} // namespace GUI
} // namespace Surge
