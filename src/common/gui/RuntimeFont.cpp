/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "RuntimeFont.h"
#if TARGET_JUCE_UI
#include <JuceHeader.h>
#endif

namespace Surge
{
namespace GUI
{

void initializeRuntimeFont()
{
    /*
     * Someone may have already initialized the globals. Don't do it twice
     */
    if (displayFont != NULL || patchNameFont != NULL)
        return;

#if !TARGET_JUCE_UI
    if (!initializeRuntimeFontForOS())
    {
        return;
    }
#endif

    displayFont = getLatoAtSize(9);
    patchNameFont = getLatoAtSize(13);
    lfoTypeFont = getLatoAtSize(8);
    aboutFont = getLatoAtSize(10);
}
VSTGUI::CFontRef getLatoAtSize(float size, int style)
{
#if TARGET_JUCE_UI
    /*
     * Optimize this later to cache the face but for now. Also I'm ignoring style and stuff.
     */
    auto tf = juce::Typeface::createSystemTypefaceFor(BinaryData::LatoRegular_ttf,
                                                      BinaryData::LatoRegular_ttfSize);
    auto lato = juce::Font(tf).withPointHeight(size);
    return std::make_shared<VSTGUI::CFontInternal>(lato);
#else
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> res = new VSTGUI::CFontDesc("Lato", size, style);
    return res;
#endif
}
} // namespace GUI
} // namespace Surge