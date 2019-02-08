#ifndef __surge_vst2__cocoa_utils__
#define __surge_vst2__cocoa_utils__

// FIXME: There is no reason for this to be a class. Move it to a Surge::Mac namespace (separately)
class CocoaUtils
{
public:
    static double getDoubleClickInterval();
    static void miniedit_text_impl( char *c, int maxchars );
    static void registerBundleFonts();
};


#endif /* defined(__surge_vst2__cocoa_utils__) */
