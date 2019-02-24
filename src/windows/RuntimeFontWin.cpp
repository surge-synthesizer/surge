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

struct RemoveFontGlobal
{
    RemoveFontGlobal() {}
    ~RemoveFontGlobal() {
        for (auto f: fonts)
        {
            RemoveFontResource(f.c_str());
        }
    }

    std::list<std::string> fonts;
};
    
RemoveFontGlobal gRemove; // when this is destroyed at DLL unload time it should run the above constructor.
    
void initializeRuntimeFont()
{
    /*
    ** The windows direct2d API seems designed to make sure you never load from
    ** memory. Like seriously: check this out
    **
    ** https://docs.microsoft.com/de-de/previous-versions/technical-content/dd941710(v=vs.85)
    **
    ** So the best bet, albeit unpalletable, is to dump the TTF to a file and then
    ** AddFontResource it. At exit time, we need to RemoveFontResource it.
    ** We accomplish this with the global guard above.
    **
    */
    
    /*
    ** Someone may have already initialized the globals. Don't do it twice
    */
    if (displayFont != NULL || patchNameFont != NULL)
        return;

    /*
    ** Construct my temporary file name and create it if needed.
    */
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    std::string latoFileName = std::string(tmpPath) + "Surge-Delivered-Lato-SemiBold.ttf";

    if (!std::experimental::filesystem::exists(latoFileName))
    {
        void   *ttfData = NULL;
        size_t  ttfDataSize = 0;
        
        HRSRC rsrc = 0;
        HMODULE hResInstance = VSTGUI::GetInstance();
        rsrc = FindResourceA(hResInstance, MAKEINTRESOURCE(IDR_LATO_FONT), "BINARY");
        if (rsrc)
        {
            HGLOBAL mem = LoadResource(hResInstance, rsrc);
            ttfData = LockResource(mem);
            ttfDataSize = SizeofResource(hResInstance, rsrc);
        }

        FILE *fontFile = fopen(latoFileName.c_str(), "wb");
        if(fontFile)
        {
            fwrite(ttfData,sizeof(char),ttfDataSize,fontFile);
            fclose(fontFile);
        }
        /*
        ** There is a tiny chance that the file didn't exist when we asked and
        ** is non-writable because another proc is doing exactly this, so
        ** if the fontFile isn't openable, just continue anyway. That's
        ** probably the best choice
        */
    }

    /*
    ** Open the font and schedule it for removal at DLL exit
    */
    int nFonts = AddFontResource(latoFileName.c_str());
    ::SendMessage(HWND_BROADCAST, WM_FONTCHANGE, NULL, NULL);
    gRemove.fonts.push_back(latoFileName);

    /*
    ** Set up the global fonts
    */
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> minifont = new VSTGUI::CFontDesc("Lato", 9);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> patchfont = new VSTGUI::CFontDesc("Lato", 14);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> lfofont = new VSTGUI::CFontDesc("Lato", 8);
    displayFont = minifont;
    patchNameFont = patchfont;
    lfoTypeFont = lfofont;
}

}
}
