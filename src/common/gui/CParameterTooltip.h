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
#include "vstcontrols.h"
#include "Parameter.h"
#include <iostream>
#include "SkinSupport.h"
#include "SkinColors.h"
#include "RuntimeFont.h"

class CParameterTooltip : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
  public:
    CParameterTooltip(const VSTGUI::CRect &size) : VSTGUI::CControl(size, 0, 0, 0)
    {
        label[0][0] = 0;
        label[1][0] = 0;
        visible = false;
        last_tag = -1;
    }

    void setLabel(const char *txt1, const char *txt2, const char *txt2left = nullptr)
    {
        if (txt1)
            strncpy(label[0], txt1, 256);
        else
            label[0][0] = 0;
        if (txt2)
            strncpy(label[1], txt2, 256);
        else
            label[1][0] = 0;
        if (txt2left)
            strncpy(label2left, txt2left, 256);
        else
            label2left[0] = 0;

        setDirty(true);
    }

    int getMaxLabelLen()
    {
        auto l0len = strlen(label[0]);
        auto l1len = strlen(label[1]) + strlen(label2left);
        return std::max(l0len, l1len);
    }

    void Show()
    {
        visible = true;
        invalid();
        setDirty(true);
    }
    void Hide()
    {
        visible = false;
#if TARGET_JUCE_UI
        this->setViewSize(VSTGUI::CRect(VSTGUI::CPoint(0, 0), VSTGUI::CPoint(1, 1)));
#endif
        invalid();
        setDirty(true);
    }
    bool isNewTag(long tag)
    {
        bool b = (last_tag != tag);
        last_tag = tag;
        return b;
    }
    bool isVisible() { return visible; }

    virtual void draw(VSTGUI::CDrawContext *dc) override
    {
        if (visible)
        {
            dc->setFont(Surge::GUI::getLatoAtSize(10));

            auto frameCol = skin->getColor(Colors::InfoWindow::Border);
            auto bgCol = skin->getColor(Colors::InfoWindow::Background);

            auto txtCol = skin->getColor(Colors::InfoWindow::Text);
            auto mpCol = skin->getColor(Colors::InfoWindow::Modulation::Positive);
            auto mnCol = skin->getColor(Colors::InfoWindow::Modulation::Negative);
            auto mpValCol = skin->getColor(Colors::InfoWindow::Modulation::ValuePositive);
            auto mnValCol = skin->getColor(Colors::InfoWindow::Modulation::ValueNegative);

            auto size = getViewSize();
            size = size.inset(0.75, 0.75);
            dc->setFrameColor(frameCol);
            dc->drawRect(size);
            VSTGUI::CRect sizem1(size);
            sizem1.inset(1, 1);
            dc->setFillColor(bgCol);
            dc->drawRect(sizem1, VSTGUI::kDrawFilled);
            dc->setFontColor(txtCol);
            VSTGUI::CRect trect(size);
            trect.inset(4, 1);
            VSTGUI::CRect tupper(trect), tlower(trect);
            tupper.bottom = tupper.top + 13;
            tlower.top = tlower.bottom - 15;

            if (hasiwstrings)
            {
                VSTGUI::CRect tmid(trect);
                tmid.bottom = trect.bottom - 18;
                tmid.top = trect.top + 15;
                if (!extendedwsstrings)
                {
                    tmid = tlower;
                }
                if (label[0][0])
                {
                    dc->drawString(label[0], tupper, VSTGUI::kLeftText, true);
                }

                if (!extendedwsstrings)
                {
                    dc->drawString(iwstrings.val.c_str(), tmid, VSTGUI::kLeftText, true);
                    dc->setFontColor(mpCol);
                    dc->drawString(iwstrings.dvalplus.c_str(), tmid, VSTGUI::kRightText, true);
                    dc->setFontColor(txtCol);
                }
                else
                {
                    auto valalign = VSTGUI::kCenterText;
                    if (iwstrings.dvalminus.size() == 0 && iwstrings.valminus.size() == 0)
                        valalign = VSTGUI::kLeftText;

                    dc->drawString(iwstrings.val.c_str(), tmid, valalign, true);
                    dc->setFontColor(mpCol);
                    dc->drawString(iwstrings.dvalplus.c_str(), tmid, VSTGUI::kRightText, true);
                    dc->setFontColor(mnCol);
                    dc->drawString(iwstrings.dvalminus.c_str(), tmid, VSTGUI::kLeftText, true);

                    dc->setFontColor(mpValCol);
                    dc->drawString(iwstrings.valplus.c_str(), tlower, VSTGUI::kRightText, true);
                    dc->setFontColor(mnValCol);
                    dc->drawString(iwstrings.valminus.c_str(), tlower, VSTGUI::kLeftText, true);
                    dc->setFontColor(txtCol);
                }
            }
            else
            {
                if (label[0][0])
                {
                    dc->drawString(label[0], tupper, VSTGUI::kLeftText, true);
                }
                // dc->drawString(label[1],tlower,false,label[0][0]?kRightText:kCenterText);
                dc->drawString(label[1], tlower, VSTGUI::kRightText, true);

                if (label2left[0])
                {
                    dc->drawString(label2left, tlower, VSTGUI::kLeftText, true);
                }
            }
        }
        else
        {
#if 0
            auto size = getViewSize();
            size = size.inset(0.75, 0.75);
            dc->setFrameColor(VSTGUI::kRedCColor);
            dc->setFrameColor(VSTGUI::kBlueCColor);
            dc->drawRect(size, VSTGUI::kDrawFilledAndStroked);
#endif
        }
        setDirty(false);
    }

    void setMDIWS(const ModulationDisplayInfoWindowStrings &i)
    {
        iwstrings = i;
        hasiwstrings = true;
    }

    void setExtendedMDIWS(bool b) { extendedwsstrings = b; }

    void clearMDIWS() { hasiwstrings = false; }
    bool hasMDIWS() { return hasiwstrings; }

  protected:
    char label[2][256], label2left[256];
    bool visible;
    int last_tag;

    ModulationDisplayInfoWindowStrings iwstrings;
    bool hasiwstrings = false;
    bool extendedwsstrings = false;

    CLASS_METHODS(CParameterTooltip, VSTGUI::CControl)
};
