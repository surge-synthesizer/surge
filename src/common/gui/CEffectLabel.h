//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SkinSupport.h"

extern VSTGUI::CFontRef displayFont;

class CEffectLabel : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponnt
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
      dc->setFillColor(skin->getColor("effect.label.hrule", gray));
      dc->drawRect(bl, VSTGUI::kDrawFilled);

      VSTGUI::CColor dgray = {76, 76, 76, 255};
      dc->setFontColor(skin->getColor("effect.label.foreground", dgray));
      dc->setFont(displayFont);
      dc->drawString(label.c_str(), size, VSTGUI::kLeftText, true);

      setDirty(false);
   }
   void setLabel(std::string s)
   {
      label = s;
   }

private:
   std::string label;

   CLASS_METHODS(CEffectLabel, VSTGUI::CControl)
};
