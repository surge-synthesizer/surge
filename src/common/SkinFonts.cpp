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

#include "SkinFonts.h"

using namespace Surge::Skin;

namespace Fonts
{
namespace System
{
const FontDesc Display("fonts.system.display", 9);
}

namespace Widgets
{
const FontDesc NumberField("fonts.widgets.numberfield", System::Display),
    EffectLabel("fonts.widgets.effectlabel", System::Display);
const FontDesc TabButtonFont("fonts.widgets.tabbutton", System::Display);
const FontDesc ModButtonFont("fonts.widgets.modbutton", 8, FontDesc::FontStyleFlags::bold);
const FontDesc SelfDrawSwitchFont("fonts.widgets.multiswitch", 8, FontDesc::FontStyleFlags::bold);

} // namespace Widgets
namespace PatchStore
{
const FontDesc Label("fonts.patchstore.label", 11), TextEntry("fonts.patchstore.textentry", 11);
}
namespace LuaEditor
{
const FontDesc Code("fonts.luaeditor.code", FontDesc::MONO, 9);
}

namespace Waveshaper
{
namespace Preview
{
const FontDesc Title("fonts.waveshaper.preview.title", 9, FontDesc::FontStyleFlags::bold),
    DriveAmount("fonts.waveshaper.preview.driveamount", 9),
    DriveLabel("fonts.waveshaper.preview.drivelabel", 7);
}
} // namespace Waveshaper
} // namespace Fonts

/*
 * Implementation Detauls
 */
namespace Surge
{
namespace Skin
{
FontDesc FontDesc::uninitializedParent("software.error", 0);
} // namespace Skin
} // namespace Surge
