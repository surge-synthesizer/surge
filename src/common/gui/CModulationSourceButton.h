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
#include <iostream>
#include "SkinSupport.h"


class CModulationSourceButton : public CCursorHidingControl, public Surge::UI::SkinConsumingComponnt
{
private:
   typedef CCursorHidingControl super;

public:
   CModulationSourceButton(const VSTGUI::CRect& size,
                           VSTGUI::IControlListener* listener,
                           long tag,
                           int state,
                           int msid,
                           std::shared_ptr<SurgeBitmaps> bitmapStore,
                           SurgeStorage* storage);
   ~CModulationSourceButton();

   virtual void setValue(float val) override
   {
      if (value != val)
         invalid();
      value = val;
   }

   virtual void onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy) override;
   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual bool onWheel(const VSTGUI::CPoint& where, const float &distance, const VSTGUI::CButtonState& buttons) override;

   int state, msid, controlstate;

   bool tint, used, blink;
   int dispval;
   bool click_is_editpart, event_is_drag, is_metacontroller, bipolar;
   char label[16];
   VSTGUI::CRect  MCRect;
   VSTGUI::CPoint LastPoint;
   VSTGUI::CPoint SourcePoint;
   float OldValue;

   SurgeStorage* storage = nullptr;

   VSTGUI::CBitmap* bmp = nullptr;

   void setblink(bool state);
   void setlabel(const char*);
   void set_ismeta(bool);
   virtual void setBipolar(bool);
   void update_rt_vals(bool ntint, float fdispval, bool used);
   int get_state()
   {
      return state;
   }

   bool hasAlternate = false;
   int alternateId;
   std::string alternateLabel;
   virtual void setAlternate( int alt, const std::string &altLabel ) {
      hasAlternate = true;
      this->alternateId = alt;
      this->alternateLabel = altLabel;
   }
   bool useAlternate = false;
   void setUseAlternate( bool f ) { useAlternate = f; if( hasAlternate ) { invalid(); setDirty(); } }
   
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;

   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      hovered = true;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   virtual VSTGUI::CMouseEventResult onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      hovered = false;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   bool hovered = false;

   CLASS_METHODS(CModulationSourceButton, VSTGUI::CControl)
};
