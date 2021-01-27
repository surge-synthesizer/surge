#include "CSurgeVuMeter.h"
#include "DspUtilities.h"
#include "SkinColors.h"
#include "SkinSupport.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"

using namespace VSTGUI;

CSurgeVuMeter::CSurgeVuMeter(const CRect &size, VSTGUI::IControlListener *listener)
    : CControl(size, listener, 0, 0)
{
    stereo = true;
    valueR = 0.0;
    setMax(2.0); // since this can clip values are above 1 sometimes
}

float scale(float x)
{
    x = limit_range(0.5f * x, 0.f, 1.f);
    return powf(x, 0.3333333333f);
}

CMouseEventResult CSurgeVuMeter::onMouseDown(CPoint &where, const CButtonState &button)
{
    if (listener && (button & (kMButton | kButton4 | kButton5)))
    {
        listener->controlModifierClicked(this, button);
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }
    return CControl::onMouseDown(where, button);
}

void CSurgeVuMeter::setType(int vutype)
{
    type = vutype;
    switch (type)
    {
    case vut_vu:
        stereo = false;
        break;
    case vut_vu_stereo:
        stereo = true;
        break;
    case vut_gain_reduction:
        stereo = false;
        break;
    default:
        type = 0;
        break;
    }
}

void CSurgeVuMeter::setValueR(float f)
{
    if (f != valueR)
    {
        valueR = f;
        setDirty();
    }
}

void CSurgeVuMeter::draw(CDrawContext *dc)
{
    if (hVuBars == nullptr)
    {
        hVuBars = associatedBitmapStore->getBitmap(IDB_VUMETER_BARS);
    }

    auto bgCol = skin->getColor(Colors::VuMeter::Background);

    CRect componentSize = getViewSize();

    VSTGUI::CDrawMode origMode = dc->getDrawMode();
    VSTGUI::CDrawMode newMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);
    dc->setDrawMode(newMode);

    dc->setFillColor(skin->getColor(Colors::VuMeter::Border));
    dc->drawRect(componentSize, VSTGUI::kDrawFilled);

    CRect borderRoundedRectBox = componentSize;
    borderRoundedRectBox.inset(1, 1);
    VSTGUI::CGraphicsPath *borderRoundedRectPath =
        dc->createRoundRectGraphicsPath(borderRoundedRectBox, 1);

    dc->setFillColor(bgCol);
    dc->drawGraphicsPath(borderRoundedRectPath, VSTGUI::CDrawContext::kPathFilled);

    /*
     * Strategy is draw the entire bar then occlude it appropriately
     */
    if (hVuBars)
    {
        int off = stereo ? 1 : 0;
        if (type == vut_gain_reduction)
        {
            off = 2;
        }

        hVuBars->draw(dc, componentSize, CPoint(0, off * 13), 0xFF);
    }

    CRect vuBarRegion(componentSize);
    float w = vuBarRegion.getWidth();

    /*
     * Where does this constant come from?
     * Well in 'scale' we map x -> x^3 to compress the range
     * cuberoot(2) = 1.26 and change
     * 1 / cuberoot(2) = 0.7937
     * so this scales it so the zerodb point is 1/cuberoot(2) along
     * still not entirely sure why, but there you go
     */
    int zerodb = (0.7937f * w);

    if (type == vut_gain_reduction)
    {
        float occludeTo = (scale(value) * (w - 1)) - zerodb; // Strictly negative
        auto dr = componentSize;
        dr = dr.inset(1, 1);
        dr.right = dr.right + occludeTo;
        dc->setFillColor(bgCol);
        dc->drawRect(dr, kDrawFilled);
    }
    else
    {
        float occludeTo = (scale(value) * (w - 1)) - zerodb; // Strictly negative
        auto drL = componentSize;
        drL = drL.inset(1, 1);

        auto drR = componentSize;
        drR = drR.inset(1, 1);

        auto occludeFromL = scale(value) * w;
        auto occludeFromR = scale(valueR) * w;
        if (stereo)
        {
            drL.bottom = drL.top + 6;
            drR.top = drL.bottom - 1;
        }
        drL.left += occludeFromL;
        drR.left += occludeFromR;
        dc->setFillColor(bgCol);
        dc->drawRect(drL, kDrawFilled);
        if (stereo)
        {
            dc->drawRect(drR, kDrawFilled);
        }
    }

    dc->setFrameColor(skin->getColor(Colors::VuMeter::Background));
    dc->setLineWidth(1);
    dc->drawGraphicsPath(borderRoundedRectPath, VSTGUI::CDrawContext::kPathStroked);

    borderRoundedRectPath->forget();
    dc->setDrawMode(origMode);

    setDirty(false);
}
