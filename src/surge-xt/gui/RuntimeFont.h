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
#ifndef SURGE_SRC_SURGE_XT_GUI_RUNTIMEFONT_H
#define SURGE_SRC_SURGE_XT_GUI_RUNTIMEFONT_H

#include "juce_graphics/juce_graphics.h"

namespace Surge
{
namespace GUI
{
struct FontManager
{
    FontManager();
    ~FontManager();

    // When we can make this say 'private' we are done
    // private:

    juce::Font
    getLatoAtSize(float size,
                  juce::Font::FontStyleFlags style = juce::Font::FontStyleFlags::plain) const;
    juce::Font
    getFiraMonoAtSize(float size,
                      juce::Font::FontStyleFlags style = juce::Font::FontStyleFlags::plain) const;

    bool useOSLato{false};

    juce::Font displayFont;
    juce::Font patchNameFont;
    juce::Font lfoTypeFont;
    juce::Font aboutFont;
    void setupFontMembers();

    juce::ReferenceCountedObjectPtr<juce::Typeface> latoRegularTypeface, latoBoldTypeface,
        latoItalicTypeface, latoBoldItalicTypeface;
    juce::ReferenceCountedObjectPtr<juce::Typeface> firaMonoRegularTypeface;

    void overrideLatoWith(juce::ReferenceCountedObjectPtr<juce::Typeface>);
    void restoreLatoAsDefault();
    juce::ReferenceCountedObjectPtr<juce::Typeface> latoOverride;
    bool hasLatoOverride{false};

    friend class Skin;
};

} // namespace GUI
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_RUNTIMEFONT_H
