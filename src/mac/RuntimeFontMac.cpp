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
    if (surge_minifont != NULL || surge_patchfont != NULL)
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
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> patchfont = new VSTGUI::CFontDesc("Lato", 14);
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> lfofont = new VSTGUI::CFontDesc("Lato", 8);
    surge_minifont = minifont;
    surge_patchfont = patchfont;
    surge_lfofont = lfofont;
}

}
}
