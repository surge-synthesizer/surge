//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeBitmaps.h"
#include "SurgeParamConfig.h"
#include "ISliderKnobInterface.h"
#include "CScalableBitmap.h"

class CSurgeSlider : public CCursorHidingControl, public virtual Surge::ISliderKnobInterface
{
public:
   CSurgeSlider(const VSTGUI::CPoint& loc,
                long style,
                VSTGUI::IControlListener* listener,
                long tag,
                bool is_mod,
                std::shared_ptr<SurgeBitmaps> bitmapStore);
   ~CSurgeSlider();
      
   virtual void draw(VSTGUI::CDrawContext*);
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
   // virtual bool onWheel (VSTGUI::CDrawContext *pContext, const VSTGUI::CPoint &where, float distance);
   virtual bool 
   onWheel(const VSTGUI::CPoint& where, const float &distane, const VSTGUI::CButtonState& buttons);

   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons); ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons); ///< called when a mouse up event occurs

   virtual double getMouseDeltaScaling(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual void onMouseMoveDelta(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons, double dx, double dy);

   virtual void setDefaultValue(float val)
   {
      CCursorHidingControl::setDefaultValue(val);
   }

   virtual void setLabel(const char* txt);
   virtual void setModValue(float val);
   virtual void setIsMod( bool b ) { is_mod = b; }
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
   virtual void setModCurrent(bool b)
   {
      has_modulation_current = b;
      invalid();
   } // - " " - for the currently selected modsource
   virtual void bounceValue(const bool keeprest = false);

   virtual bool isInMouseInteraction();

   virtual void setValue(float val);
   virtual void setBipolar(bool);

   void SetQuantitizedDispValue(float f);

   CLASS_METHODS(CSurgeSlider, CControl)

   bool is_mod;
   bool disabled;

   enum MoveRateState
   {
      kUnInitialized = 0,
      kClassic,
      kSlow,
      kMedium,
      kExact
   };

   static MoveRateState sliderMoveRateState;

   virtual void setStyle(int i ) 
   {
      style = i;
      invalid();
   }

   virtual int getStyle() 
   {
      return style;
   }


   typedef enum BitmapMode {
      Surge16_Bitmaps,
      Individual_Bitmaps
   } BitmapMode;
   BitmapMode bitmapmode = Surge16_Bitmaps;

   typedef enum StateBitmaps {
      UnipolarBackground,
      UnipolarModulatedBackground,

      /* These two states could be here logically but actually aren't used so leave them out
      UnipolarTickedBackground,
      UnipolarTickedModulatedBackground,
      */
      
      BipolarBackground,
      BipolarModulatedBackground,
      BipolarTickedBackground,
      BipolarTickedModulatedBackground,

      ValueHandle,
      ModulationHandle,

      n_stateBitmaps
   } StateBitmaps;
      

   void setBitmapForState( StateBitmaps b, VSTGUI::CBitmap *bm ) {
      bitmapmode = Individual_Bitmaps;
      stateBitmaps[b] = bm;
   }

   
   
private:
   // For ContainsMultitudes mode
   VSTGUI::CBitmap *pHandle, *pTray, *pModHandle;

   VSTGUI::CBitmap *stateBitmaps[n_stateBitmaps];
   
   // For BitmapPerState mode
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
   bool has_modulation, has_modulation_current;
   VSTGUI::CPoint lastpoint, sourcepoint;
   float oldVal, *edit_value;
   int drawcount_debug;

   float restvalue, restmodval;
};
