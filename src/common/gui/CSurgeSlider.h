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

class CSurgeSlider : public CCursorHidingControl, public Surge::UI::SkinConsumingComponnt
{
public:
   CSurgeSlider(const VSTGUI::CPoint& loc,
                long style,
                VSTGUI::IControlListener* listener,
                long tag,
                bool is_mod,
                std::shared_ptr<SurgeBitmaps> bitmapStore,
                SurgeStorage* storage = nullptr);
   ~CSurgeSlider();
   virtual void draw(VSTGUI::CDrawContext*) override;
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
   // virtual bool onWheel (VSTGUI::CDrawContext *pContext, const VSTGUI::CPoint &where, float distance);
   virtual bool 
   onWheel(const VSTGUI::CPoint& where, const float &distane, const VSTGUI::CButtonState& buttons) override;

   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse up event occurs

   virtual VSTGUI::CMouseEventResult
   onMouseEntered(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   
   virtual VSTGUI::CMouseEventResult
   onMouseExited(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   
   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual void onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy) override;

   virtual void setLabel(const char* txt);
   virtual void setModValue(float val);
   virtual float getModValue()
   {
      return modval;
   }
   virtual void setModMode(int mode)
   {
      modmode = mode;
   } // 0 - absolute pos, 1 - absolute mod, 2 - relative mod
   virtual void setMoveRate(float rate)
   {
      moverate = rate;
   }
   virtual void setModPresent(bool b)
   {
      has_modulation = b;
   } // true if the slider has modulation routings assigned to it
   virtual void setModCurrent(bool isCurrent, bool isBipolarModulator)
   {
      has_modulation_current = isCurrent;
      modulation_is_bipolar = isBipolarModulator;
      invalid();
   } // - " " - for the currently selected modsource
   virtual void bounceValue(const bool keeprest = false);

   virtual bool isInMouseInteraction();

   virtual void setValue(float val) override;
   virtual void setBipolar(bool);

   virtual void setTempoSync( bool b ) { is_temposync = b; }
   
   void SetQuantitizedDispValue(float f);

   CLASS_METHODS(CSurgeSlider, CControl)

   bool in_hover = false;
   bool is_mod;
   bool disabled; // means it can't be used unless something else changes
   bool deactivated; // means it has been turned off by user action
   bool hasBeenDraggedDuringMouseGesture = false;
   
   bool isStepped = false;
   int intRange = 0;
   
   SurgeStorage* storage = nullptr;
   
   enum MoveRateState
   {
      kUnInitialized = 0,
      kClassic,
      kSlow,
      kMedium,
      kExact
   };

   virtual void onSkinChanged() override;
   
   static MoveRateState sliderMoveRateState;

private:
   VSTGUI::CBitmap *pHandle = nullptr, *pTray = nullptr,
      *pModHandle = nullptr, *pTempoSyncHandle = nullptr, *pHandleHover = nullptr;
   VSTGUI::CRect handle_rect, handle_rect_orig;
   VSTGUI::CPoint offsetHandle;
   int range;
   int controlstate;
   long style;
   float modval, qdvalue;
   char label[256], leftlabel[256];
   int modmode;
   float moverate, statezoom;
   int typex, typey;
   int typehx, typehy;
   bool has_modulation, has_modulation_current, modulation_is_bipolar = false;
   bool is_temposync = false;
   VSTGUI::CPoint lastpoint, sourcepoint;
   float oldVal, *edit_value;
   int drawcount_debug;
   
   

   float restvalue, restmodval;
   bool wheelInitiatedEdit = false;
};
