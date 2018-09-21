//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class gui_hswitch : public CHorizontalSwitch
{
public:
   gui_hswitch(const CRect& size,
               IControlListener* listener,
               long tag,
               long subPixmaps,       // number of subPixmaps
               long heightOfOneImage, // pixel
               long rows,
               long columns,
               CBitmap* background,
               CPoint& offset,
               bool dragable = false)
       : CHorizontalSwitch(
             size, listener, tag, subPixmaps, heightOfOneImage, subPixmaps, background, offset)
   {
      this->rows = rows;
      this->columns = columns;
      this->dragable = dragable;
      imgoffset = 0;
   }

   int rows, columns;
   int imgoffset;
   bool dragable;

   virtual void draw(CDrawContext* dc);
   virtual CMouseEventResult
   onMouseDown(CPoint& where,
               const CButtonState& buttons); ///< called when a mouse down event occurs
   virtual CMouseEventResult
   onMouseUp(CPoint& where, const CButtonState& buttons); ///< called when a mouse up event occurs
   virtual CMouseEventResult
   onMouseMoved(CPoint& where,
                const CButtonState& buttons); ///< called when a mouse move event occurs
   CLASS_METHODS(gui_hswitch, CControl)
};