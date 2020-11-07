#include "RuntimeFont.h"
#include <list>
#include <iostream>

using namespace VSTGUI;

namespace Surge
{
namespace GUI
{

void initializeRuntimeFont()
{
    /*
    ** Someone may have already initialized the globals. Don't do it twice
    */
    if (displayFont != NULL || patchNameFont != NULL)
        return;

    /*
    ** If Lato isn't installed, bail and use defaults
    */
    std::list<std::string> fontNames;
    if (IPlatformFont::getAllPlatformFontFamilies (fontNames))
    {
        if (std::find(fontNames.begin(), fontNames.end(), "Lato") == fontNames.end())
        {
           /*
           ** Lato is not installed; bail out
           */
           return;
        }
    }

    /*
    ** Set up the global fonts
    */
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> minifont = new VSTGUI::CFontDesc("Lato", 9);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> patchfont = new VSTGUI::CFontDesc("Lato", 13);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> lfofont = new VSTGUI::CFontDesc("Lato", 8);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> aboutfont = new VSTGUI::CFontDesc("Lato", 10);
    displayFont = minifont;
    patchNameFont = patchfont;
    lfoTypeFont = lfofont;
    aboutFont = aboutfont;
}

}
}
