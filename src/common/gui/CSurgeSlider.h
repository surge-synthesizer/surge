//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
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
                std::shared_ptr<SurgeBitmaps> bitmapStore);
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

   bool is_mod;
   bool disabled;
   bool hasBeenDraggedDuringMouseGesture = false;
   
   enum MoveRateState
   {
      kUnInitialized = 0,
      kClassic,
      kSlow,
      kMedium,
      kExact
   };

   static MoveRateState sliderMoveRateState;

private:
   VSTGUI::CBitmap *pHandle, *pTray, *pModHandle, *pTempoSyncHandle;
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
