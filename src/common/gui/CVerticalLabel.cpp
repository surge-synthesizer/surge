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

#include "CVerticalLabel.h"
#include <iostream>

using namespace VSTGUI;

void CVerticalLabel::draw(VSTGUI::CDrawContext *dc)
{
#if TARGET_JUCE_UI
    auto t = getText().getString();
    auto q = t.size();
#else
    auto t = getText();
    auto q = strlen(t);
#endif

    auto f = getFont();
    auto fh = f->getSize();
    auto ht = 0.f;
    for (int i = 0; i < q; ++i)
    {
        if (t[i] == '-')
            ht = ht;
        else if (t[i] == ' ')
            ht += fh * 0.5;
        else
            ht += fh;
    }

    float start = (getViewSize().getHeight() - ht) * 0.5;
    auto vs = getViewSize();
    float ypos = vs.top + start;
    for (int i = 0; i < q; ++i)
    {
        if (t[i] == '-')
            continue;
        auto cht = fh;
        if (t[i] == ' ')
            cht = fh * 0.5;
        auto p = CRect(CPoint(vs.left, ypos), CPoint(vs.getWidth(), cht));
        ypos += cht;
        char txt[2];
        txt[0] = t[i];
        txt[1] = 0;

        dc->setFont(getFont());
        dc->setFontColor(getFontColor());
        dc->drawString(txt, p);
    }
}