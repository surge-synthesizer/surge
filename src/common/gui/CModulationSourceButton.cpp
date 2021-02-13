#include "CModulationSourceButton.h"
#include "CSurgeSlider.h"
#include "MouseCursorControl.h"
#include "globals.h"
#include "guihelpers.h"
#include "ModulationSource.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "SkinColors.h"
#include <UserDefaults.h>
#include <vt_dsp/basic_dsp.h>
#include <iostream>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;

enum
{
    cs_none = 0,
    cs_drag = 1,
    cs_maybeswap = 2,
    cs_swap = 3
};

class SurgeStorage;

//------------------------------------------------------------------------------------------------

CModulationSourceButton::CModulationSourceButton(const CRect &size, IControlListener *listener,
                                                 long tag, int state, int msid,
                                                 std::shared_ptr<SurgeBitmaps> bitmapStore,
                                                 SurgeStorage *storage)
    : CControl(size, listener, tag, 0),
      OldValue(0.f), Surge::UI::CursorControlAdapterWithMouseDelta<CModulationSourceButton>(storage)
{
    this->state = state;
    this->msid = msid;

    this->storage = storage;

    tint = false;
    used = false;
    bipolar = false;
    click_is_editpart = false;
    is_metacontroller = false;
    dispval = 0;
    controlstate = cs_none;
    label[0] = 0;
    blink = 0;
    bmp = bitmapStore->getBitmap(IDB_MODSOURCE_BG);
    arrow = bitmapStore->getBitmap(IDB_MODSOURCE_SHOW_LFO);
    this->storage = nullptr;
}

//------------------------------------------------------------------------------------------------

CModulationSourceButton::~CModulationSourceButton() {}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::update_rt_vals(bool ntint, float fdispval, bool nused)
{
    bool changed = (tint && !ntint) || (!tint && ntint) || (used && !nused) || (!used && nused);
    // int new_dispval = max(0,fdispval * 45);
    int new_dispval = (int)max(0.f, (float)(fdispval * 150.f));
    // changed = changed || (new_dispval != dispval);

    tint = ntint;
    used = nused;
    dispval = new_dispval;

    if (changed)
        setDirty(true);
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::set_ismeta(bool b)
{
    is_metacontroller = b;
    setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::setBipolar(bool b)
{
    bipolar = b;
    setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::setlabel(const char *lbl)
{
    strncpy(label, lbl, 16);
    label[15] = 0;
    setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::draw(CDrawContext *dc)
{
    CRect sze = getViewSize();
    // sze.offset(-size.left,-size.top);

#if WINDOWS
    auto priorDrawMode = dc->getDrawMode();
    dc->setDrawMode(kAntiAliasing | kNonIntegralMode);
#endif

    /*
         state
         0 - nothing
         1 - selected modeditor
         2 - selected modsource (locked)
         4 [bit 2] - selected arrowbutton [0,1,2 -> 4,5,6]
         */

    // source position in bitmap

    bool SelectedModSource = (state & 3) == 1;
    bool ActiveModSource = (state & 3) == 2;
    bool UsedOrActive = used || (state & 3);

    CColor FrameCol, FillCol, FontCol;

    FillCol = skin->getColor(Colors::ModSource::Used::Background);
    FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Border)
                            : skin->getColor(Colors::ModSource::Unused::Border);
    FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Text)
                           : skin->getColor(Colors::ModSource::Unused::Text);
    if (hovered)
    {
        FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::BorderHover)
                                : skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::TextHover)
                               : skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    if (ActiveModSource)
    {
        FrameCol = skin->getColor(Colors::ModSource::Armed::Border);
        if (hovered)
            FrameCol = skin->getColor(Colors::ModSource::Armed::BorderHover);

        FillCol = skin->getColor(Colors::ModSource::Armed::Background);
        FontCol = skin->getColor(Colors::ModSource::Armed::Text);
        if (hovered)
            FontCol = skin->getColor(Colors::ModSource::Armed::TextHover);
    }
    else if (SelectedModSource)
    {
        FrameCol = used ? skin->getColor(Colors::ModSource::Selected::Used::Border)
                        : skin->getColor(Colors::ModSource::Selected::Border);
        if (hovered)
            FrameCol = used ? skin->getColor(Colors::ModSource::Selected::Used::BorderHover)
                            : skin->getColor(Colors::ModSource::Selected::BorderHover);
        FillCol = used ? skin->getColor(Colors::ModSource::Selected::Used::Background)
                       : skin->getColor(Colors::ModSource::Selected::Background);
        FontCol = used ? skin->getColor(Colors::ModSource::Selected::Used::Text)
                       : skin->getColor(Colors::ModSource::Selected::Text);
        if (hovered)
            FontCol = used ? skin->getColor(Colors::ModSource::Selected::Used::TextHover)
                           : skin->getColor(Colors::ModSource::Selected::TextHover);
    }
    else if (tint)
    {
        FillCol = skin->getColor(Colors::ModSource::Unused::Background);
        FrameCol = skin->getColor(Colors::ModSource::Unused::Border);
        if (hovered)
            FrameCol = skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = skin->getColor(Colors::ModSource::Unused::Text);
        if (hovered)
            FontCol = skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    if (secondaryHover)
    {
        FontCol = skin->getColor(Colors::ModSource::Used::UsedModHover);
        if (ActiveModSource)
            FontCol = skin->getColor(Colors::ModSource::Armed::UsedModHover);
        if (SelectedModSource)
            FontCol = skin->getColor(Colors::ModSource::Selected::UsedModHover);
    }

    CRect framer(sze);
    CRect fillr(framer);
    fillr.inset(1, 1);
    dc->setFont(displayFont);
    dc->setFontColor(FontCol);
    dc->setFrameColor(FrameCol);
    dc->setFillColor(FillCol);
    CRect txtbox(sze);
#if WINDOWS
    txtbox.inset(1, 0.6);
#else
    txtbox.inset(1, 1);
#endif
    txtbox.bottom = txtbox.top + 13;

    dc->setFillColor(FrameCol);
    dc->drawRect(framer, kDrawFilled);
    dc->setFillColor(FillCol);
    dc->drawRect(fillr, kDrawFilled);

    if (hasAlternate && useAlternate)
    {
        dc->drawString(alternateLabel.c_str(), txtbox, kCenterText, true);
    }
    else
    {
        dc->drawString(label, txtbox, kCenterText, true);
    }

    if (is_metacontroller)
    {
        MCRect = sze;
        MCRect.top += 12;

        auto mrect = MCRect;
        mrect.left++;
        mrect.top++;
        mrect.bottom--;
        mrect.right--;

        dc->setFillColor(skin->getColor(Colors::ModSource::Used::Background));
        dc->drawRect(mrect, kDrawFilled);

        CRect brect(mrect);
        brect.inset(1, 1);
        CRect notch(brect);

        dc->setFillColor(skin->getColor(Colors::ModSource::Macro::Background));
        dc->drawRect(brect, kDrawFilled);

        int midx = brect.left + ((brect.getWidth() - 1) * 0.5);
        int barx = brect.left + (value * (float)brect.getWidth());

        lastBarDraw = CPoint(barx, (brect.top + brect.bottom) * 0.5);

        if (bipolar)
        {
            dc->setFillColor(skin->getColor(Colors::ModSource::Macro::Fill));
            CRect bar(brect);

            if (barx > midx)
            {
                bar.left = midx + 1;
                bar.right = barx + 1;
            }
            else
            {
                bar.left = barx;
                bar.right = midx + 2;
            }
            bar.bound(brect);
            dc->drawRect(bar, kDrawFilled);
        }
        else
        {
            CRect vr(brect);
            vr.right = barx + 1;
            vr.bound(brect);
            dc->setFillColor(skin->getColor(Colors::ModSource::Macro::Fill));

            if (vr.right > vr.left)
            {
                dc->drawRect(vr, kDrawFilled);
            }
        }

        if (skin->getVersion() >= 2)
        {
            notch.left = (barx <= brect.right - 1) ? barx : barx - 1;
            notch.right = notch.left + 1;
            dc->setFillColor(skin->getColor(Colors::ModSource::Macro::CurrentValue));
            dc->drawRect(notch, kDrawFilled);
        }
    }

    const int rh = 16;

    // show LFO parameters arrow
    if (msid >= ms_lfo1 && msid <= ms_slfo6)
    {
        CRect arwsze = sze;
        arwsze.left = sze.right - 14;
        arwsze.setWidth(14);
        arwsze.setHeight(16);
        CPoint where;
        where.y = state >= 4 ? rh : 0;

        arrow->draw(dc, arwsze, where, 0xff);
    }

#if WINDOWS
    dc->setDrawMode(priorDrawMode);
#endif

    setDirty(false);
}

//------------------------------------------------------------------------------------------------

CMouseEventResult CModulationSourceButton::onMouseDown(CPoint &where, const CButtonState &buttons)
{
    hasMovedBar = false;

    if (storage)
        this->hideCursor = !Surge::UI::showCursor(storage);

    if (!getMouseEnabled())
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

    CRect size = getViewSize();
    CPoint loc(where);
    loc.offset(-size.left, -size.top);

    if (controlstate != cs_none)
    {
#if MAC
//		if(buttons & kRButton) statezoom = 0.1f;
#endif
        return kMouseEventHandled;
    }

    if (listener && buttons & (kAlt | kRButton | kMButton | kButton4 | kButton5 | kShift |
                               kControl | kApple | kDoubleClick))
    {
        if (listener->controlModifierClicked(this, buttons) != 0)
            return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    onMouseDownCursorHelper(where);

    if (is_metacontroller && (buttons & kDoubleClick) && MCRect.pointInside(where) &&
        controlstate == cs_none)
    {
        if (bipolar)
            value = 0.5;
        else
            value = 0;

        invalid();
        if (listener)
            listener->valueChanged(this);

        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if (is_metacontroller && MCRect.pointInside(where) && (buttons & kLButton) &&
        controlstate == cs_none)
    {
        beginEdit();
        controlstate = cs_drag;

        enqueueCursorHideIfMoved(where);

        return kMouseEventHandled;
    }
    else if (buttons & kLButton)
    {
        /*
        ** If you uncomment this if and have all paths set controstrate to cs_maybeswap
        ** modulators wll also drop onto targets if you also uncomment the sge->openBlah below
        */
        if (is_metacontroller || (buttons & kControl))
        {
            controlstate = cs_maybeswap;
            dragStart = where;
        }
        return kMouseEventHandled;
    }

    return kMouseEventHandled;
}

//------------------------------------------------------------------------------------------------

CMouseEventResult CModulationSourceButton::onMouseUp(CPoint &where, const CButtonState &buttons)
{
    unenqueueCursorHideIfMoved();

    if (controlstate == cs_swap && dragLabel)
    {
        dragLabel->setVisible(false);
        getFrame()->removeView(dragLabel);
        dragLabel = nullptr;
        controlstate = cs_none;

        auto p = getFrame()->getViewSize();
        auto wa = where - p.getTopLeft();
        wa = getFrame()->getTransform().transform(wa);
        auto v = getFrame()->getViewAt(wa);
        auto sge = dynamic_cast<SurgeGUIEditor *>(listener);
        if (sge)
        {
            auto c = dynamic_cast<CModulationSourceButton *>(v);
            if (c && c->is_metacontroller)
            {
                if (c->getTag() == getTag())
                {
                    // We've dropped onto ourself. Treat like an ARM toggle for now
                    click_is_editpart = false;
                    event_is_drag = false;
                    if (listener)
                        listener->valueChanged(this);
                }
                else
                {
                    sge->swapControllers(getTag(), c->getTag());
                }
            }

            auto s = dynamic_cast<CSurgeSlider *>(v);
            if (s)
            {
                // See comment above when state is set to maybeswap
                if (buttons & kControl)
                    sge->openModTypeinOnDrop(getTag(), s, s->getTag());
            }
        }

        controlstate = cs_none;
    }
    else if (controlstate == cs_none || controlstate == cs_maybeswap)
    {
        auto size = getViewSize();
        CPoint loc(where);
        loc.offset(-size.left, -size.top);

        click_is_editpart =
            loc.x >= (size.getWidth() - 14); // click area for show LFO parameters arrow
        event_is_drag = false;
        if (listener)
            listener->valueChanged(this);
    }

    if (controlstate == cs_drag)
    {
        endEdit();

        if (hasMovedBar)
            endCursorHide(lastBarDraw);
        else
            endCursorHide();
    }
    controlstate = cs_none;

    return kMouseEventHandled;
}

CMouseEventResult CModulationSourceButton::onMouseMoved(CPoint &where, const CButtonState &buttons)
{
    if (controlstate == cs_maybeswap)
    {
        float thresh = 0;
        thresh += (where.x - dragStart.x) * (where.x - dragStart.x);
        thresh += (where.y - dragStart.y) * (where.y - dragStart.y);
        thresh = sqrt(thresh);
        if (thresh < 3)
        {
            onMouseMovedCursorHelper(where, buttons);
        }
        if (dragLabel == nullptr)
        {
            std::string lb = label;
            if (hasAlternate && useAlternate)
            {
                lb = alternateLabel;
            }

            if (lb == "-")
                lb = "Drag Macro";

            auto l = new CTextLabel(getViewSize(), lb.c_str());
            l->setTransparency(false);

            CColor FrameCol, FillCol, FontCol;

            FillCol = skin->getColor(Colors::ModSource::Used::Background);
            FrameCol = skin->getColor(Colors::ModSource::Used::Border);
            FontCol = skin->getColor(Colors::ModSource::Used::Text);

            l->setFont(displayFont);
            l->setBackColor(FillCol);
            l->setFontColor(FontCol);
            l->setFrameColor(FrameCol);
            l->setVisible(true);
            getFrame()->addView(l);
            dragLabel = l;
            dragOffset = where - getViewSize().getTopLeft();
        }
        controlstate = cs_swap;
        return kMouseEventHandled;
    }
    else if (controlstate == cs_swap && dragLabel)
    {
        auto s = dragLabel->getViewSize();
        auto r = s;
        s = s.moveTo(where - dragOffset);
        dragLabel->setViewSize(s);
        dragLabel->invalid();
        r.extend(10, 10);
        getFrame()->invalid();
        return kMouseEventHandled;
    }
    else
    {
        return onMouseMovedCursorHelper(where, buttons);
    }
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::onMouseMoveDelta(const CPoint &where, const CButtonState &buttons,
                                               double dx, double dy)
{
    if ((controlstate == cs_drag) && (buttons & kLButton))
    {
        hasMovedBar = true;

        value += dx / (double)(getWidth());
        value = limit_range(value, 0.f, 1.f);
        event_is_drag = true;
        invalid();
        if (listener)
            listener->valueChanged(this);
    }
}

double CModulationSourceButton::getMouseDeltaScaling(const CPoint &where,
                                                     const CButtonState &buttons)
{
    double scaling = 0.25f;

    if (buttons & kShift)
        scaling *= 0.1;

    return scaling;
}

bool CModulationSourceButton::onWheel(const VSTGUI::CPoint &where, const float &distance,
                                      const VSTGUI::CButtonState &buttons)
{
    if (!is_metacontroller)
    {
#if ENABLE_WHEEL_FOR_ALTERNATE
        /*
         * This code selects the modulator improperly.
         */
        if (hasAlternate)
        {
            accumWheelDistance += distance;
#if WINDOWS
            bool doTick = accumWheelDistance != 0;
#else
            bool doTick = accumWheelDistance > 2 || accumWheelDistance < -2;
#endif
            if (doTick)
            {
                accumWheelDistance = 0;
                auto sge = dynamic_cast<SurgeGUIEditor *>(listener);
                if (sge)
                {
                    sge->toggleAlternateFor(this);
                    return true;
                }
            }
        }
#endif
        return false;
    }

    auto rate = 1.f;

#if WINDOWS
    // This is purely empirical. The wheel for this control feels slow on windows
    // but fine on a macos trackpad. I callibrated it by making sure the trackpad
    // across the pad gesture on my macbook pro moved the control the same amount
    // in surge mac as it does in vst3 bitwig in parallels.
    rate = 10.f;
#endif

    if (buttons & kShift)
        rate *= 0.05;

    value += rate * distance / (double)(getWidth());
    value = limit_range(value, 0.f, 1.f);
    event_is_drag = true;
    invalid();
    if (listener)
        listener->valueChanged(this);
    return true;
}

//------------------------------------------------------------------------------------------------
