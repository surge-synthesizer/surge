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

#include "MainFrame.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Widgets
{
void MainFrame::mouseDown(const juce::MouseEvent &event)
{
    if (!editor)
        return;
    if (event.mods.isMiddleButtonDown())
    {
        editor->toggle_mod_editing();
    }
    if (event.mods.isPopupMenu())
    {
        editor->useDevMenu = false;
        auto r = VSTGUI::CRect();
        editor->showSettingsMenu(r);
    }
}
} // namespace Widgets
} // namespace Surge