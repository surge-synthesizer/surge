/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "CSurgeHyperlink.h"
#include <iostream>
#include "DebugHelpers.h"

using namespace VSTGUI;
void CSurgeHyperlink::draw(VSTGUI::CDrawContext *dc)
{
    if (bmp)
    {
        CPoint off;
        off.x += xoffset;
        if (isHovered)
            off.y += getViewSize().getHeight();
        bmp->draw(dc, getViewSize(), off);
    }
    else
    {
        dc->setFont(font);
        dc->setFontColor(isHovered ? hoverColor : labelColor);
        dc->drawString(label.c_str(), getViewSize(), textalign);
    }
}