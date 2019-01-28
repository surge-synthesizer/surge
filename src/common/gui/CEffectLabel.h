//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

extern VSTGUI::CFontRef surge_minifont;

class CEffectLabel : public VSTGUI::CControl
{
public:
   CEffectLabel(const VSTGUI::CRect& size) : VSTGUI::CControl(size, 0, 0, 0)
   {}

    virtual void draw(VSTGUI::CDrawContext* dc)
   {
      VSTGUI::CRect size = getViewSize();
      VSTGUI::CRect bl(size);
      bl.top = bl.bottom - 2;
      VSTGUI::CColor gray = {106, 106, 106, 255};
      dc->setFillColor(gray);
      dc->drawRect(bl, VSTGUI::kDrawFilled);
      dc->setFontColor(gray);
      // dc->setFont(kNormalFontSmaller,8,kBoldFace);
      dc->setFont(surge_minifont);
      dc->drawString(label.c_str(), size, VSTGUI::kLeftText, false);
      setDirty(false);
   }
   void setLabel(string s)
   {
      label = s;
   }

private:
   string label;

   CLASS_METHODS(CEffectLabel, VSTGUI::CControl)
};
