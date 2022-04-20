#pragma once

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
