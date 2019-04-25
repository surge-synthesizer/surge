//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeParamConfig.h"

class CSurgeVuMeter : public VSTGUI::CControl
{
public:
   CSurgeVuMeter(const VSTGUI::CRect& size);
   virtual void draw(VSTGUI::CDrawContext* dc);
   void setType(int vutype);
   // void setSecondaryValue(float v);
   void setValueR(float f);
   float getValueR()
   {
      return valueR;
   }
   bool stereo;

private:
   float valueR;
   int type;
   CLASS_METHODS(CSurgeVuMeter, VSTGUI::CControl)
};
