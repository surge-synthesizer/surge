#pragma once

#include "juce_graphics/juce_graphics.h"

namespace Surge
{
namespace GUI
{

struct DefaultFonts : public juce::DeletedAtShutdown
{
    DefaultFonts();
    ~DefaultFonts();
    juce::Font
    getLatoAtSize(float size,
                  juce::Font::FontStyleFlags style = juce::Font::FontStyleFlags::plain) const;
    juce::Font getFiraMonoAtSize(float size) const;

    bool useOSLato{false};

    juce::Font displayFont;
    juce::Font patchNameFont;
    juce::Font lfoTypeFont;
    juce::Font aboutFont;

    juce::ReferenceCountedObjectPtr<juce::Typeface> latoRegularTypeface;
    juce::ReferenceCountedObjectPtr<juce::Typeface> firaMonoRegularTypeface;
};

DefaultFonts *getFontManager();

} // namespace GUI
} // namespace Surge
