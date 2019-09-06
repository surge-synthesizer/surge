//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "ISwitchControl.h"

class CSwitchControl : public VSTGUI::CControl, public virtual Surge::ISwitchControl
{
public:
   CSwitchControl(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, VSTGUI::CBitmap* background);
   ~CSwitchControl();
   virtual void draw(VSTGUI::CDrawContext* dc);
   virtual void setValue(float f);
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons); ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons); ///< called when a mouse up event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseMoved(VSTGUI::CPoint& where,
                const VSTGUI::CButtonState& buttons); ///< called when a mouse move event occurs

   virtual void setBitmapForState(int state, VSTGUI::CBitmap *bitmap);

private:
   static const int max_states = 16;
   bool useStateBitmaps = false;
   VSTGUI::CBitmap *bitmaps[max_states];
   
   bool down;
   float heightOfSingleImage;

   CLASS_METHODS(CSwitchControl, VSTGUI::CControl)
};
