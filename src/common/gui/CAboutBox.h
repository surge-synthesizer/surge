//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "vstcontrols.h"
#include "SkinSupport.h"

class CAboutBox : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponnt
{
public:
   CAboutBox(const VSTGUI::CRect& size,
             VSTGUI::IControlListener* listener,
             long tag,
             VSTGUI::CBitmap* background,
             VSTGUI::CRect& toDisplay,
             VSTGUI::CPoint& offset,
             VSTGUI::CBitmap* aboutBitmap);
   virtual ~CAboutBox();

   virtual void draw(VSTGUI::CDrawContext*) override;
   virtual bool hitTest(const VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons = -1) override;
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual void unSplash();

   void boxShow(std::string dataPath, std::string userPath);
   void boxHide(bool invalidateframe = true);

   CLASS_METHODS(CAboutBox, VSTGUI::CControl)

protected:
   VSTGUI::CRect toDisplay;
   VSTGUI::CRect keepSize;
   VSTGUI::CPoint offset;
   VSTGUI::SharedPointer<VSTGUI::CBitmap> _aboutBitmap;
   bool bvalue;
   std::string dataPath, userPath;

   static VSTGUI::SharedPointer<VSTGUI::CFontDesc> infoFont;
};
