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
