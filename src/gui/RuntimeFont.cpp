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
#include <JuceHeader.h>
#include <iostream>

namespace Surge
{
namespace GUI
{

DefaultFonts::DefaultFonts()
{
#define TEST_WITH_INDIE_FLOWER 0
#if TEST_WITH_INDIE_FLOWER
    latoRegularTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::IndieFlower_ttf,
                                                                  BinaryData::IndieFlower_ttfSize);
#else
    latoRegularTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::LatoRegular_ttf,
                                                                  BinaryData::LatoRegular_ttfSize);
#endif

    firaMonoRegularTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::FiraMonoRegular_ttf, BinaryData::FiraMonoRegular_ttfSize);
    displayFont = getLatoAtSize(9);
    patchNameFont = getLatoAtSize(13);
    lfoTypeFont = getLatoAtSize(8);
    aboutFont = getLatoAtSize(10);
}

DefaultFonts::~DefaultFonts(){};

juce::Font DefaultFonts::getLatoAtSize(float size, juce::Font::FontStyleFlags style) const
{
    return juce::Font(latoRegularTypeface).withPointHeight(size).withStyle(style);
}

juce::Font DefaultFonts::getFiraMonoAtSize(float size) const
{
    return juce::Font(firaMonoRegularTypeface).withPointHeight(size);
}

DefaultFonts *getFontManager()
{
    static DefaultFonts *fmi{nullptr};
    if (!fmi)
        fmi = new DefaultFonts();
    return fmi;
}

} // namespace GUI
} // namespace Surge