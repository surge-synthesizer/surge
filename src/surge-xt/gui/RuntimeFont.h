#pragma once

#include "juce_graphics/juce_graphics.h"

namespace Surge
{
namespace GUI
{

class Skin;

struct DefaultFonts : public juce::DeletedAtShutdown
{
    DefaultFonts();
    ~DefaultFonts();

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

    juce::ReferenceCountedObjectPtr<juce::Typeface> latoRegularTypeface, latoBoldTypeface,
        latoItalicTypeface, latoBoldItalicTypeface;
    juce::ReferenceCountedObjectPtr<juce::Typeface> firaMonoRegularTypeface;

    static DefaultFonts *fmi;
    friend class Skin;
};

DefaultFonts *getFontManager();

} // namespace GUI
} // namespace Surge
