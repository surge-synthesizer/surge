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
#include "CTextButtonWithHover.h"
#include <iostream>

template <typename T> struct HoverGuard
{
    HoverGuard() {}

    void activate(std::function<T()> getf, std::function<void(T &)> isetf, T &overr)
    {
        active = true;
        setf = isetf;
        orig = getf();
        setf(overr);
    }

    ~HoverGuard()
    {
        if (active)
            setf(orig);
    }

    bool active = false;
    std::function<void(T &)> setf;
    T orig;
};

void CTextButtonWithHover::draw(VSTGUI::CDrawContext *context)
{

#define DO_HOVER(x, T)                                                                             \
    HoverGuard<T> hg_##x;                                                                          \
    if (hc_##x.isSet)                                                                              \
    {                                                                                              \
        hg_##x.activate([this]() { return get##x(); }, [this](T &c) { set##x(c); }, hc_##x.item);  \
    }

    if (isHovered)
    {
        DO_HOVER(Gradient, VSTGUI::CGradient *);
        DO_HOVER(FrameColor, VSTGUI::CColor);
        DO_HOVER(TextColor, VSTGUI::CColor);
        DO_HOVER(GradientHighlighted, VSTGUI::CGradient *);
        DO_HOVER(FrameColorHighlighted, VSTGUI::CColor);
        DO_HOVER(TextColorHighlighted, VSTGUI::CColor);
        CTextButton::draw(context);
    }
    else
    {
        CTextButton::draw(context);
    }
}
VSTGUI::CMouseEventResult CTextButtonWithHover::onMouseEntered(VSTGUI::CPoint &where,
                                                               const VSTGUI::CButtonState &buttons)
{
    isHovered = true;
    invalid();
    return CView::onMouseEntered(where, buttons);
}

VSTGUI::CMouseEventResult CTextButtonWithHover::onMouseExited(VSTGUI::CPoint &where,
                                                              const VSTGUI::CButtonState &buttons)
{
    isHovered = false;
    invalid();
    return CView::onMouseExited(where, buttons);
}
