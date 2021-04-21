#pragma once

#include <efvg/escape_from_vstgui.h>

namespace Surge
{
namespace GUI
{

struct DefaultFonts : public juce::DeletedAtShutdown
{
    DefaultFonts();
    ~DefaultFonts();
    // Fix (or expand) this signature to not couple to VSTGUI
    juce::Font getLatoAtSize(float size, int style = VSTGUI::kNormalFace) const;

    juce::Font displayFont;
    juce::Font patchNameFont;
    juce::Font lfoTypeFont;
    juce::Font aboutFont;

    juce::ReferenceCountedObjectPtr<juce::Typeface> latoRegularTypeface;
};

DefaultFonts *getFontManager();

} // namespace GUI
} // namespace Surge
