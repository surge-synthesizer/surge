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
