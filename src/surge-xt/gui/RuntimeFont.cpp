/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
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
        return juce::Font(juce::FontOptions(latoOverride)).withPointHeight(size).withStyle(style);
    }
    else if (useOSLato)
    {
        return juce::Font(juce::FontOptions("Lato", 10, 0)).withPointHeight(size).withStyle(style);
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
        return juce::Font(juce::FontOptions(tf)).withPointHeight(size).withStyle(style);
    }
}

juce::Font FontManager::getFiraMonoAtSize(float size, juce::Font::FontStyleFlags style) const
{
    return juce::Font(juce::FontOptions(firaMonoRegularTypeface))
        .withPointHeight(size)
        .withStyle(style);
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