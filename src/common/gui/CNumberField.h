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
#include "SurgeParamConfig.h"
#include "CursorControlGuard.h"

inline int get_mod_mode(int ct)
{
   switch (ct)
   {
   case cm_eq_bandwidth:
      ct = cm_mod_percent;
      break;
   case cm_mod_decibel:
   case cm_decibel:
      ct = cm_mod_decibel;
      break;
   case cm_mod_freq:
   case cm_frequency_audible:
   case cm_frequency_samplerate:
      ct = cm_mod_freq;
      break;
   case cm_frequency0_2k:
   case cm_percent:
      ct = cm_mod_percent;
      break;
   case cm_mod_pitch:
      ct = cm_mod_pitch;
      break;
   case cm_bitdepth_16:
      ct = cm_mod_percent;
      break;
   case cm_time1s:
      ct = cm_mod_time;
      break;
   case cm_frequency_khz:
   case cm_frequency_khz_bi:
      ct = cm_frequency_khz_bi;
      break;
   default:
      ct = cm_none;
   };
   return ct;
}

enum label_placement
{
   lp_left = 0,
   lp_right,
   lp_below,
   lp_above
};

class CScalableBitmap;

class CNumberField : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent,
                     public Surge::UI::CursorControlAdapter<CNumberField>
{
public:
   CNumberField(const VSTGUI::CRect& size,
                VSTGUI::IControlListener* listener = 0,
                long tag = 0,
                VSTGUI::CBitmap* pBackground = 0,
                SurgeStorage* storage = nullptr);
   ~CNumberField();

   virtual void bounceValue() override;

   virtual void setValue(float val) override;

   void setIntValue(int ival)
   {
      i_value = ival;
      if( i_max != i_min )
         value = 1.f * (i_value - i_min) / ( i_max - i_min );
      else
         value = 0;
      bounceValue();
      setDirty();
   }
   int getIntValue()
   {
      return i_value;
   }

   void setIntDefaultValue(int idef)
   {
      i_default = idef;
   }

   void setControlMode(int mode);
   int getControlMode() { return controlmode; }
   void setPoly(int p)
   {
      if (i_poly != p)
      {
         i_poly = p;
         setDirty(true);
      }
   }
   int getPoly() { return i_poly; }

   void setIntMin(int value)
   {
      i_min = value;
   }
   int getIntMin()
   {
      return i_min;
   }
   void setIntMax(int value)
   {
      i_max = value;
   }
   int getIntMax()
   {
      return i_max;
   }
   void setIntStepSize(int value)
   {
      i_stepsize = value;
   }
   int getIntStepSize()
   {
      return i_stepsize;
   }
   void setFloatMin(float value)
   {
      f_min = value;
   }
   float getFloatMin()
   {
      return f_min;
   }
   void setFloatMax(float value)
   {
      f_max = value;
   }
   float getFloatMax()
   {
      return f_max;
   }

   virtual void draw(VSTGUI::CDrawContext*) override;
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;

   bool hovered = false;
   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      hovered = true;
      getFrame()->setCursor(VSTGUI::kCursorVSize);
      invalid();
      return VSTGUI::kMouseEventHandled;
   }
   virtual VSTGUI::CMouseEventResult onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      hovered = false;
      invalid();
      getFrame()->setCursor( VSTGUI::kCursorDefault);
      return VSTGUI::kMouseEventHandled;
   }
   
   virtual bool onWheel(const VSTGUI::CPoint& where, const float& distance, const VSTGUI::CButtonState& buttons) override;
   SurgeStorage* storage = nullptr;

private:
   int controlmode, controlstate;
   int i_min, i_max, i_stepsize;
   float f_movespeed;
   float f_min, f_max;
   int i_value;
   int i_default;
   int i_poly;
   int labelplacement;
   bool enqueueCursorHide = false;
   VSTGUI::CRect drawsize;
   VSTGUI::CPoint lastmousepos, startmousepos;

   CScalableBitmap *bg = nullptr, *hoverBg = nullptr;
   bool triedToLoadBg = false;

   CLASS_METHODS(CNumberField, VSTGUI::CControl)
};
