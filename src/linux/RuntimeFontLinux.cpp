#include "RuntimeFont.h"
#include <list>
#include <iostream>

using namespace VSTGUI;

namespace Surge
{
namespace GUI
{

bool initializeRuntimeFontForOS()
{
#if !TARGET_JUCE_UI
    /*
    ** If Lato isn't installed, bail and use defaults
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
