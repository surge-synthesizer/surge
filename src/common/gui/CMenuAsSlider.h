// -*-c++-*-
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
#include "SurgeBitmaps.h"
#include "SurgeParamConfig.h"
#include "SkinSupport.h"

class CMenuAsSlider : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:
   CMenuAsSlider(const VSTGUI::CPoint& loc,
                VSTGUI::IControlListener* listener,
                long tag,
                std::shared_ptr<SurgeBitmaps> bitmapStore,
                SurgeStorage* storage = nullptr);
   virtual ~CMenuAsSlider();
   virtual void draw(VSTGUI::CDrawContext*) override;
   void setLabel( const char* lab ) { label = lab; }
   
   virtual bool onWheel(const VSTGUI::CPoint& where, const float &distane, const VSTGUI::CButtonState& buttons) override;

   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
      {
         return VSTGUI::kMouseEventHandled;
      }

   virtual VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
      {
         isHover = true;
         wheelDistance = 0;
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
   
   virtual VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
      {
         isHover = false;
         wheelDistance = 0;
         invalid();
         return VSTGUI::kMouseEventHandled;
      }


   void setMinMax( int i, int x ) {
      iMin = i; iMax = x;
   }

   void setDeactivated( bool d ) { deactivated = d; }
   bool deactivated;
   
   CLASS_METHODS(CMenuAsSlider, CControl)
   bool in_hover = false;
   SurgeStorage* storage = nullptr;
   
   virtual void onSkinChanged() override;

private:
   VSTGUI::CBitmap *pBackground = nullptr, *pBackgroundHover = nullptr;
   std::string label = "";
   bool isHover = false;
   float wheelDistance = 0;
   
   int iMin = 0, iMax = 10;
};
