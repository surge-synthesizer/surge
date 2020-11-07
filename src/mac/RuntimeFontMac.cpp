#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include "RuntimeFont.h"

#include "vstgui/lib/platform/mac/macglobals.h"
#include "UserInteractions.h"


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
    ** MacFonts use the CoreText registry so we can just find them in the bundle
    ** and register them using the CoreText Font Manager API
    */
    CFURLRef url = nullptr;
    url = CFBundleCopyResourceURL(VSTGUI::getBundleRef(), CFSTR("Lato-Regular"), CFSTR("ttf"), CFSTR("fonts"));
    if (url)
    {
        CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, NULL );
        CFRelease(url);
    }
    else
    {
        Surge::UserInteractions::promptError("Cannot load lato font from bundle", "Software Error");
    }

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
