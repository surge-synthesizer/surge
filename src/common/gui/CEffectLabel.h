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
#include "SkinSupport.h"
#include "SkinColors.h"

extern VSTGUI::CFontRef displayFont;

class CEffectLabel : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:
   CEffectLabel(const VSTGUI::CRect& size) : VSTGUI::CControl(size, 0, 0, 0)
   {}

   virtual void draw(VSTGUI::CDrawContext* dc) override
   {
      VSTGUI::CRect size = getViewSize();
      VSTGUI::CRect bl(size);
      bl.top = bl.bottom - 2;

      dc->setFillColor(skin->getColor(Colors::Effect::Label::Separator));
      dc->drawRect(bl, VSTGUI::kDrawFilled);

      dc->setFontColor(skin->getColor(Colors::Effect::Label::Text));
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
