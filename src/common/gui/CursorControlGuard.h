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

#pragma once
#if ESCAPE_FROM_VSTGUI
#include "efvg/escape_from_vstgui.h"
#else
#include "vstgui/vstgui.h"
#endif
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "guihelpers.h"

namespace Surge
{
namespace UI
{

/*
 * CursorControlGuard is the primary guard class we use to hide, show,
 * and move a cursor. It uses the RAII style pattern in a way, in that
 * constructing it hides the cursor and destroying it shows the cursor.
 *
 * A common usage would be to have a std::shared_ptr<CursorControlGuard> member
 * of your class and onMouseDown initialize it with a std::make_shared of this
 * and onMouseUp reset it.
 *
 * The guard will also allow you to change the coordinates at which the mouse
 * re-appears usign the various APIs in the class.
 */
struct CursorControlGuard
{
    enum MotionMode
    {
        SHOW_AT_LOCATION, // Use the where from construction or setShowLocation when you show
        SHOW_AT_MOUSE_MOTION_POINT // Use the point of construction when you show
    } motionMode = SHOW_AT_LOCATION;
    VSTGUI::CPoint showLocation; // In physical screen cordinates

    CursorControlGuard();
    CursorControlGuard(VSTGUI::CFrame *, const VSTGUI::CPoint &where);
    ~CursorControlGuard();

    void setShowLocationFromFrameLocation(VSTGUI::CFrame *, const VSTGUI::CPoint &where);
    void setShowLocationFromViewLocation(VSTGUI::CView *, const VSTGUI::CPoint &where);
    bool resetToShowLocation();

  private:
    void doHide();
};

/*
 * There's a common pattern where we want to manipulate the guard in a common way
 * including reading the hiding preference and so forth. Add this class to your
 * class as a base class and you get what you need. This is the approach we recommend
 * to cursor hiding in surge, although the class above is a public API.
 *
 * The use cases are 'startCursorHide(CPoint & )' or 'startCursorHide()'
 * and then 'endCursorHide( CPoint &)' or 'endCursorHide()'.
 *
 * The intent is you make this a base clase of yourself with yourself as
 * the template arg (the curiosly reocurring pattern thing), that the
 * "T" is a CView, and then everything here works in view local coordinates.
 */

template <typename T> // We assume T is a CView subclass
struct CursorControlAdapter
{
    CursorControlAdapter(SurgeStorage *s)
    {
#if !TARGET_JUCE_UI
        if (s)
            hideCursor = !Surge::UI::showCursor(s);
#else
        hideCursor = false;
#endif
    }

    void startCursorHide()
    {
        if (hideCursor)
        {
            ccadapterGuard = std::make_shared<CursorControlGuard>();
        }
    }

    void startCursorHide(const VSTGUI::CPoint &where)
    {
        if (hideCursor)
        {
            ccadapterGuard = std::make_shared<CursorControlGuard>();
            ccadapterGuard->setShowLocationFromViewLocation(asT(), where);
        }
    }

    void setCursorLocation(const VSTGUI::CPoint &where)
    {
        if (ccadapterGuard)
            ccadapterGuard->setShowLocationFromViewLocation(asT(), where);
    }

    bool resetToShowLocation()
    {
        if (ccadapterGuard)
            return ccadapterGuard->resetToShowLocation();
        return false;
    }

    void endCursorHide() { ccadapterGuard = nullptr; }

    void endCursorHide(const VSTGUI::CPoint &where)
    {
        if (ccadapterGuard)
            ccadapterGuard->setShowLocationFromViewLocation(asT(), where);
        ccadapterGuard = nullptr;
    }

    T *asT() { return static_cast<T *>(this); }

    bool hideCursor = true;
    std::shared_ptr<CursorControlGuard> ccadapterGuard;
};

template <typename T> struct CursorControlAdapterWithMouseDelta : public CursorControlAdapter<T>
{
    CursorControlAdapterWithMouseDelta(SurgeStorage *s) : CursorControlAdapter<T>(s) {}

    bool hideIsEnqueued = false;
    VSTGUI::CPoint enqueuedLocation;
    void onMouseDownCursorHelper(const VSTGUI::CPoint &where)
    {
        hideIsEnqueued = false;
        deltapoint = where;
        origdeltapoint = where;
    }

    void enqueueCursorHideIfMoved(const VSTGUI::CPoint &where)
    {
        hideIsEnqueued = true;
        enqueuedLocation = where;
    }
    void unenqueueCursorHideIfMoved() { hideIsEnqueued = false; }
    VSTGUI::CMouseEventResult onMouseMovedCursorHelper(const VSTGUI::CPoint &where,
                                                       const VSTGUI::CButtonState &buttons)
    {
        if (hideIsEnqueued)
        {
            CursorControlAdapter<T>::startCursorHide(enqueuedLocation);
            hideIsEnqueued = false;
        }
        auto scale = CursorControlAdapter<T>::asT()->getMouseDeltaScaling(where, buttons);
        float dx = (where.x - deltapoint.x);
        float dy = (where.y - deltapoint.y);
        CursorControlAdapter<T>::asT()->onMouseMoveDelta(
            where, buttons, scale * (where.x - deltapoint.x), scale * (where.y - deltapoint.y));
        auto odx = where.x - origdeltapoint.x;
        auto ody = where.y - origdeltapoint.y;
        auto delt = sqrt(odx * odx + ody * ody);

        /*
         * Windows10 and macos don't need this but Windows8.1 really
         * craps out with fractional cursor moves so make sure we have
         * something big move wise before we reset.
         */
        if (delt > 10)
        {
            if (CursorControlAdapter<T>::asT()->resetToShowLocation())
                deltapoint = origdeltapoint;
            else
                deltapoint = where;
        }
        else
        {
            deltapoint = where;
        }
        return VSTGUI::kMouseEventHandled;
    }

  private:
    VSTGUI::CPoint deltapoint, origdeltapoint;
};

} // namespace UI
} // namespace Surge
