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
#include "SurgeParamConfig.h"
#include "SkinSupport.h"

class CScalableBitmap;
class CSurgeVuMeter : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:
   CSurgeVuMeter(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener);
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   void setType(int vutype);
   // void setSecondaryValue(float v);
   void setValueR(float f);
   float getValueR()
   {
      return valueR;
   }
   bool stereo;

   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button) override;

private:
   float valueR;
   int type;
   CScalableBitmap* hVuBars = nullptr;
   CLASS_METHODS(CSurgeVuMeter, VSTGUI::CControl)
};
