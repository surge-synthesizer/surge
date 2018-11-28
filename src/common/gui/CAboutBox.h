//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "vstcontrols.h"

class CAboutBox : public CControl
{
public:
   CAboutBox(const CRect& size,
             IControlListener* listener,
             long tag,
             CBitmap* background,
             CRect& toDisplay,
             CPoint& offset,
             CBitmap* aboutBitmap);
   virtual ~CAboutBox();

   virtual void draw(CDrawContext*);
   virtual bool hitTest(const CPoint& where, const CButtonState& buttons = -1);
   // virtual void mouse (CDrawContext *pContext, CPoint &where, long button = -1);
   virtual CMouseEventResult
   onMouseDown(CPoint& where,
               const CButtonState& buttons); ///< called when a mouse down event occurs
   virtual void unSplash();

   void boxShow();
   void boxHide(bool invalidateframe = true);

   CLASS_METHODS(CAboutBox, CControl)

protected:
   CRect toDisplay;
   CRect keepSize;
   CPoint offset;
   SharedPointer<CBitmap> _aboutBitmap;
   bool bvalue;
};