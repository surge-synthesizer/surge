//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CHSwitch2 : public VSTGUI::CHorizontalSwitch
{
public:
   CHSwitch2(const VSTGUI::CRect& size,
             VSTGUI::IControlListener* listener,
             long tag,
             long subPixmaps,       // number of subPixmaps
             long heightOfOneImage, // pixel
             long rows,
             long columns,
             VSTGUI::CBitmap* background,
             VSTGUI::CPoint& offset,
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

   virtual void draw(VSTGUI::CDrawContext* dc);
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons); ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons); ///< called when a mouse up event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseMoved(VSTGUI::CPoint& where,
                const VSTGUI::CButtonState& buttons); ///< called when a mouse move event occurs
   virtual bool
   onWheel (const VSTGUI::CPoint& where, const float& distance, const VSTGUI::CButtonState& buttons); ///< called when scrollwheel events occurs    
   CLASS_METHODS(CHSwitch2, VSTGUI::CControl)
};
