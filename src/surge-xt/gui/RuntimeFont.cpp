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
#include "SurgeXTBinary.h"

#include <iostream>

namespace Surge
{
namespace GUI
{

FontManager::FontManager()
{
#if WINDOWS
    /* On windows in memory fonts use GDI and OS fonts use D2D so if we have
     * LATO in the OS use that
     */
    auto ft = juce::Font::findAllTypefaceNames();
    for (const auto &q : ft)
        if (q == "Lato")
            useOSLato = true;

#endif

#define TEST_WITH_INDIE_FLOWER 0
#if TEST_WITH_INDIE_FLOWER
    latoRegularTypeface = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::IndieFlower_ttf, SurgeXTBinary::IndieFlower_ttfSize);
#else
    latoRegularTypeface = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::LatoRegular_ttf, SurgeXTBinary::LatoRegular_ttfSize);
    latoBoldTypeface = juce::Typeface::createSystemTypefaceFor(SurgeXTBinary::LatoBold_ttf,
                                                               SurgeXTBinary::LatoBold_ttfSize);
    latoItalicTypeface = juce::Typeface::createSystemTypefaceFor(SurgeXTBinary::LatoItalic_ttf,
                                                                 SurgeXTBinary::LatoItalic_ttfSize);
    latoBoldItalicTypeface = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::LatoBoldItalic_ttf, SurgeXTBinary::LatoBoldItalic_ttfSize);

#endif

    firaMonoRegularTypeface = juce::Typeface::createSystemTypefaceFor(
        SurgeXTBinary::FiraMonoRegular_ttf, SurgeXTBinary::FiraMonoRegular_ttfSize);
    setupFontMembers();
}

void FontManager::setupFontMembers()
{
    displayFont = getLatoAtSize(9);
    patchNameFont = getLatoAtSize(13);
    lfoTypeFont = getLatoAtSize(8);
    aboutFont = getLatoAtSize(10);
}
FontManager::~FontManager(){};

juce::Font FontManager::getLatoAtSize(float size, juce::Font::FontStyleFlags style) const
{
    if (hasLatoOverride)
    {
        return juce::Font(latoOverride).withPointHeight(size).withStyle(style);
    }
    else if (useOSLato)
    {
        return juce::Font("Lato", 10, 0).withPointHeight(size).withStyle(style);
    }
    else
    {
        auto tf = latoRegularTypeface;
        if (style & juce::Font::bold && style & juce::Font::italic)
        {
            tf = latoBoldItalicTypeface;
        }
        else if (style & juce::Font::bold)
        {
            tf = latoBoldTypeface;
        }
        else if (style & juce::Font::italic)
        {
            tf = latoItalicTypeface;
        }
        return juce::Font(tf).withPointHeight(size).withStyle(style);
    }
}

juce::Font FontManager::getFiraMonoAtSize(float size, juce::Font::FontStyleFlags style) const
{
    return juce::Font(firaMonoRegularTypeface).withPointHeight(size).withStyle(style);
}

void FontManager::overrideLatoWith(juce::ReferenceCountedObjectPtr<juce::Typeface> itf)
{
    hasLatoOverride = true;
    latoOverride = itf;
    setupFontMembers();
}
void FontManager::restoreLatoAsDefault()
{
    hasLatoOverride = false;
    latoOverride = nullptr;
    setupFontMembers();
}

} // namespace GUI
} // namespace Surge