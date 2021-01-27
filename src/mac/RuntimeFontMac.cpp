#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include "RuntimeFont.h"

#include "vstgui/lib/platform/mac/macglobals.h"
#include "UserInteractions.h"

namespace Surge
{
namespace GUI
{

bool initializeRuntimeFontForOS()
{
    /*
    ** MacFonts use the CoreText registry so we can just find them in the bundle
    ** and register them using the CoreText Font Manager API
    */
    CFURLRef url = nullptr;
    url = CFBundleCopyResourceURL(VSTGUI::getBundleRef(), CFSTR("Lato-Regular"), CFSTR("ttf"),
                                  CFSTR("fonts"));
    if (url)
    {
        CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, NULL);
        CFRelease(url);
    }
    else
    {
        Surge::UserInteractions::promptError("Cannot load lato font from bundle", "Software Error");
        return false;
    }
    return true;
}

} // namespace GUI
} // namespace Surge
