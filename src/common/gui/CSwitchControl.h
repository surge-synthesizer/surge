//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CSwitchControl : public CControl
{
public:
   CSwitchControl(const CRect& size, IControlListener* listener, long tag, CBitmap* background);
   virtual void draw(CDrawContext* dc);
   virtual void setValue(float f);
   virtual CMouseEventResult
   onMouseDown(CPoint& where,
               const CButtonState& buttons); ///< called when a mouse down event occurs
   virtual CMouseEventResult
   onMouseUp(CPoint& where, const CButtonState& buttons); ///< called when a mouse up event occurs
   virtual CMouseEventResult
   onMouseMoved(CPoint& where,
                const CButtonState& buttons); ///< called when a mouse move event occurs
   int ivalue, imax;
   bool is_itype;

private:
   bool down;

   CLASS_METHODS(CSwitchControl, CControl)
};