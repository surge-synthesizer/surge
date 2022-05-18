/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

/*
 * For a discussion of why this is in src/common rather than src/gui
 * See the discussion in SkinColors.h
 */

#ifndef SURGE_XT_SKINFONTS_H
#define SURGE_XT_SKINFONTS_H

#include <string>

namespace Surge
{
namespace Skin
{
struct FontDesc // this isn't actually a font; it is a description of a desire to get a font
                // also avoid name clashes with all the other things called "Font"
{
    // I don't want to couple to JUCE this early so let me do this here
    // and then assert they are equal in Skin::getFont in case JUCE changes
    enum FontStyleFlags
    {
        plain = 0,
        bold = 1,
        italic = 2,
    };

    enum DefaultFamily
    {
        SANS,
        MONO,
        NO_DEFAULT,
    };

    FontDesc(const std::string &id, int sz, int style = 0)
        : id(id), defaultFamily(SANS), size(sz), style(style)
    {
    }
    FontDesc(const std::string &id, DefaultFamily fam, int sz, int style = 0)
        : id(id), defaultFamily(fam), size(sz), style(style)
    {
    }
    FontDesc(const std::string &id, const std::string &family, int sz, int style)
        : id(id), defaultFamily(NO_DEFAULT), family(family), size(sz), style(style)
    {
    }

    FontDesc(const std::string &id, const FontDesc &p) : parent(p), hasParent(true) {}
    FontDesc(const std::string &id, const FontDesc &&) = delete;

    static FontDesc uninitializedParent;
    const FontDesc &parent{uninitializedParent};
    bool hasParent{false};
    DefaultFamily defaultFamily{SANS};

    std::string id{"font.error"};
    std::string family;
    int size{10};
    int style{plain};
};
} // namespace Skin
} // namespace Surge

namespace Fonts
{
namespace System
{
extern const Surge::Skin::FontDesc Display;
}

namespace Widgets
{
extern const Surge::Skin::FontDesc NumberField, EffectLabel;
extern const Surge::Skin::FontDesc TabButtonFont;
extern const Surge::Skin::FontDesc ModButtonFont;
extern const Surge::Skin::FontDesc SelfDrawSwitchFont;
} // namespace Widgets
namespace PatchStore
{
extern const Surge::Skin::FontDesc Label, TextEntry;
}
namespace LuaEditor
{
extern const Surge::Skin::FontDesc Code;
}
namespace Waveshaper
{
namespace Preview
{
extern const Surge::Skin::FontDesc Title, DriveAmount, DriveLabel;
}
} // namespace Waveshaper

} // namespace Fonts
#endif // SURGE_XT_SKINFONTS_H
