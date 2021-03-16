#include "globals.h"
#include "SurgeStorage.h"
#include "CEffectSettings.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"

using namespace VSTGUI;

CEffectSettings::CEffectSettings(const CRect &size, IControlListener *listener, long tag,
                                 int current, std::shared_ptr<SurgeBitmaps> bitmapStore)
    : CControl(size, listener, tag, 0), labels(nullptr)
{
    this->current = current;
    bg = bitmapStore->getBitmap(IDB_FX_GRID);
    disabled = 0;
}

void CEffectSettings::draw(CDrawContext *dc)
{
    CRect size = getViewSize();

    if (skin->getVersion() >= 2)
    {
        if (bg)
        {
            bg->draw(dc, size, CPoint(0, 0), 0xff);
        }

        dc->saveGlobalState();

        dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);
        dc->setFont(Surge::GUI::getLatoAtSize(7, kBoldFace));

        // scene A/B boxes
        for (int i = 0; i < n_scenes; ++i)
        {
            CRect r(0, 0, scenelabelboxWidth, scenelabelboxHeight);
            r.offset(size.left, size.top);
            r.offset(scenelabelbox[i][0], scenelabelbox[i][1]);

            dc->setFrameColor(skin->getColor(Colors::Effect::Grid::Scene::Border));
            dc->setFillColor(skin->getColor(Colors::Effect::Grid::Scene::Background));
            dc->setFontColor(skin->getColor(Colors::Effect::Grid::Scene::Text));

            dc->drawRect(r, kDrawFilledAndStroked);
            dc->drawString(scenename[i], r, kCenterText, true);
        }

        // FX slots
        for (int i = 0; i < n_fx_slots; i++)
        {
            CRect r(0, 0, fxslotWidth, fxslotHeight);
            r.offset(size.left, size.top);
            r.offset(fxslotpos[i][0], fxslotpos[i][1]);

            setColorsForFXSlot(dc, r, i);
        }

        dc->restoreGlobalState();
    }
    else
    {
        if (skin && associatedBitmapStore && !labels)
        {
            labels = associatedBitmapStore->getBitmap(IDB_FX_TYPE_ICONS);
        }

        if (bg)
        {
            bg->draw(dc, size, CPoint(0, 0), 0xff);
            // source position in bitmap
            // CPoint where (0, heightOfOneImage * (long)((value * (float)(rows*columns - 1) +
            // 0.5f))); pBackground->draw(dc,size,where,0xff);
        }

        if (labels)
        {
            for (int i = 0; i < n_fx_slots; i++)
            {
                CRect r(0, 0, 17, 9);
                r.offset(size.left, size.top);
                r.offset(fxslotpos[i][0], fxslotpos[i][1]);
                int stype = 0;
                if (disabled & (1 << i))
                    stype = 4;
                switch (bypass)
                {
                case fxb_no_fx:
                    stype = 2;
                    break;
                case fxb_no_sends:
                    if ((i == 4) || (i == 5))
                        stype = 2;
                    break;
                case fxb_scene_fx_only:
                    if (i > 3)
                        stype = 2;
                    break;
                }
                if (i == current)
                    stype += 1;
                labels->draw(dc, r, CPoint(17 * stype, 9 * get_fxtype(type[i])), 0xff);
            }
        }
    }

    if (mouseActionMode == drag && dragSource >= 0 && dragSource < n_fx_slots)
    {
        auto dx = 0.f, dy = 0.f;
        auto r = CRect(dragCurrent - dragCornerOff, CPoint(fxslotWidth - 2.f, fxslotHeight - 2.f));
        CRect vs = getViewSize();
        vs = vs.inset(1, 1);

        if (r.top < vs.top)
        {
            dy = vs.top - r.top;
        }

        if (r.bottom > vs.bottom)
        {
            dy = vs.bottom - r.bottom;
        }

        if (r.left < vs.left)
        {
            dx = vs.left - r.left;
        }

        if (r.right > vs.right)
        {
            dx = vs.right - r.right;
        }

        r.top += dy;
        r.bottom += dy;
        r.right += dx;
        r.left += dx;

        auto q = r;
        q = q.extend(1, 1);

        if (skin->getVersion() < 2)
        {
            dc->setFillColor(skin->getColor(Colors::Effect::Grid::Border));
            dc->drawRect(q, kDrawFilled);
            labels->draw(dc, r, CPoint(17, 9 * get_fxtype(type[dragSource])), 0xff);
        }
        else
        {
            dc->setFont(Surge::GUI::getLatoAtSize(7, kBoldFace));
            setColorsForFXSlot(dc, q, dragSource);
        }
    }

    setDirty(false);
}

void CEffectSettings::setColorsForFXSlot(CDrawContext *dc, CRect rect, int fxslot)
{
    VSTGUI::CColor bgcol, frcol, txtcol;
    bool byp = false;

    if (disabled & (1 << fxslot))
        byp = true;
    switch (bypass)
    {
    case fxb_no_fx:
        byp = true;
        break;
    case fxb_no_sends:
        if ((fxslot == 4) || (fxslot == 5))
            byp = true;
        break;
    case fxb_scene_fx_only:
        if (fxslot > 3)
            byp = true;
        break;
    }

    if (fxslot == current)
    {
        if (byp)
        {
            if (hovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Text);
            }
        }
        else
        {
            if (hovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Selected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Selected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Selected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Selected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Selected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Selected::Text);
            }
        }
    }
    else
    {
        if (byp)
        {
            if (hovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Bypassed::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Bypassed::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Bypassed::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Bypassed::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Bypassed::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Bypassed::Text);
            }
        }
        else
        {
            if (hovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Unselected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Unselected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Unselected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Unselected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Unselected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Unselected::Text);
            }
        }
    }

    dc->setFrameColor(frcol);
    dc->setFillColor(bgcol);
    dc->setFontColor(txtcol);

    dc->drawRect(rect, kDrawFilledAndStroked);

    // draw text string or a line if no effect loaded
    auto selfx = fx_type_shortnames[get_fxtype(type[fxslot])];

    if (strcmp(selfx, "OFF") == 0)
    {
        CPoint center = rect.getCenter();
        auto line = CRect(CPoint(center.x - 3, center.y - 1), CPoint(6, 2));

        dc->setFrameColor(txtcol);
        dc->drawRect(line, kDrawStroked);
    }
    else
    {
        dc->drawString(selfx, rect, kCenterText, true);
    }
}

CMouseEventResult CEffectSettings::onMouseDown(CPoint &where, const CButtonState &buttons)
{
    if (listener && (buttons & (kMButton | kButton4 | kButton5)))
    {
        listener->controlModifierClicked(this, buttons);
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    mouseActionMode = click;
    dragStart = where;
    dragSource = -1;

    for (int i = 0; i < n_fx_slots; i++)
    {
        CRect size = getViewSize();
        CRect r(0, 0, fxslotWidth, fxslotHeight);
        r.offset(size.left, size.top);
        r.offset(fxslotpos[i][0], fxslotpos[i][1]);

        if (r.pointInside(where))
        {
            dragSource = i;
            dragCornerOff = where - r.getTopLeft();
        }
    }

    for (int i = 0; i < n_scenes; i++)
    {
        CRect size = getViewSize();
        CRect r(0, 0, scenelabelboxWidth, scenelabelboxHeight);
        r.offset(size.left, size.top);
        r.offset(scenelabelbox[i][0], scenelabelbox[i][1]);

        if (r.pointInside(where) && (buttons & kRButton))
        {
            auto sge = dynamic_cast<SurgeGUIEditor *>(listener);

            if (sge)
            {
                sge->effectSettingsBackgroundClick(i);
            }
        }
    }

    return kMouseEventHandled;
}

CMouseEventResult CEffectSettings::onMouseUp(CPoint &where, const CButtonState &buttons)
{
    if (mouseActionMode == drag && dragSource >= 0 && dragSource < n_fx_slots)
    {
        int droppedOn = -1;

        for (int i = 0; i < n_fx_slots; i++)
        {
            CRect size = getViewSize();
            CRect r(0, 0, fxslotWidth, fxslotHeight);
            r.offset(size.left, size.top);
            r.offset(fxslotpos[i][0], fxslotpos[i][1]);

            if (r.pointInside(where))
            {
                droppedOn = i;
            }
        }

        auto sge = dynamic_cast<SurgeGUIEditor *>(listener);

        if (sge && droppedOn >= 0)
        {
            auto m = SurgeSynthesizer::FXReorderMode::SWAP;

            if (buttons & kControl)
            {
                m = SurgeSynthesizer::FXReorderMode::COPY;
            }

            if (buttons & kShift)
            {
                m = SurgeSynthesizer::FXReorderMode::MOVE;
            }

            sge->swapFX(dragSource, droppedOn, m);

            current = droppedOn;

            if (listener)
            {
                listener->valueChanged(this);
            }
        }

        mouseActionMode = none;
        invalid();
    }
    else if (mouseActionMode == click)
    {
        mouseActionMode = none;

        if (!((buttons.getButtonState() & kLButton) || (buttons.getButtonState() & kRButton)))
        {
            return kMouseEventHandled;
        }

        for (int i = 0; i < n_fx_slots; i++)
        {
            CRect size = getViewSize();
            CRect r(0, 0, fxslotWidth, fxslotHeight);
            r.offset(size.left, size.top);
            r.offset(fxslotpos[i][0], fxslotpos[i][1]);

            if (r.pointInside(where))
            {
                if (buttons.getButtonState() & kRButton)
                {
                    disabled ^= (1 << i);
                }
                else
                {
                    current = i;
                }

                invalid();
            }
        }

        if (listener)
            listener->valueChanged(this);
    }

    return kMouseEventHandled;
}

CMouseEventResult CEffectSettings::onMouseMoved(CPoint &where, const CButtonState &buttons)
{
    if (hovered)
    {
        int testHover = -1;

        for (int i = 0; i < n_fx_slots; i++)
        {
            CRect size = getViewSize();
            CRect r(0, 0, fxslotWidth, fxslotHeight);
            r.offset(size.left, size.top);
            r.offset(fxslotpos[i][0], fxslotpos[i][1]);

            if (r.pointInside(where))
            {
                testHover = i;
            }
        }

        if (testHover != currentHover)
        {
            currentHover = testHover;
            invalid();
        }
    }

    if (mouseActionMode == click)
    {
        float dx = dragStart.x - where.x;
        float dy = dragStart.y - where.y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist > 3 && dragSource >= 0 && dragSource < n_fx_slots)
        {
            mouseActionMode = drag;
        }
    }

    if (mouseActionMode == drag)
    {
        dragCurrent = where;
        invalid();
    }
    return kMouseEventHandled;
}
