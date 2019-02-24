//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "vstcontrols.h"

enum ctrl_mode
{
   cm_none = 0,
   cm_integer,
   cm_notename,
   cm_float,
   cm_percent,
   cm_percent_bipolar,
   cm_decibel,
   cm_decibel_squared,
   cm_envelopetime,
   cm_lforate,
   cm_midichannel,
   cm_mutegroup,
   cm_lag,
   cm_pbdepth,
   cm_frequency20_20k,
   cm_frequency50_50k,
   cm_bitdepth_16,
   cm_frequency0_2k,
   cm_decibelboost12,
   cm_octaves3,
   cm_frequency1hz,
   cm_time1s,
   cm_frequency_audible,
   cm_frequency_samplerate,
   cm_frequency_khz,
   cm_frequency_khz_bi,
   cm_frequency_hz_bi,
   cm_eq_bandwidth,
   cm_stereowidth,
   cm_mod_decibel,
   cm_mod_pitch,
   cm_mod_freq,
   cm_mod_percent,
   cm_mod_time,
   cm_polyphony,
   cm_envshape,
   cm_osccount,
   cm_count4,
   cm_noyes,
   cm_temposync,
   cm_int_menu,
};

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

class CNumberField : public VSTGUI::CControl
{
public:
   CNumberField(const VSTGUI::CRect& size,
                VSTGUI::IControlListener* listener = 0,
                long tag = 0,
                VSTGUI::CBitmap* pBackground = 0);
   ~CNumberField();

   virtual void setFontColor(VSTGUI::CColor color);
   VSTGUI::CColor getFontColor()
   {
      return fontColor;
   }

   virtual void setBackColor(VSTGUI::CColor color);
   VSTGUI::CColor getBackColor()
   {
      return backColor;
   }

   virtual void setLineColor(VSTGUI::CColor color);
   VSTGUI::CColor getLineColor()
   {
      return lineColor;
   }

   // virtual void setTxtFace (VSTGUI::CTxtFace val);
   VSTGUI::CTxtFace getTxtFace()
   {
      return txtFace;
   }

   /*virtual void setFont (CFont fontID);
   CFont getFont () { return fontID; }*/

   void setBgcolor(VSTGUI::CColor bgcol)
   {
      envColor = bgcol;
   }

   virtual void bounceValue();

   virtual void setValue(float val);

   void setIntValue(int ival)
   {
      i_value = ival;
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

   virtual void setLabel(char* newlabel);
   virtual void setLabelPlacement(int placement);

   virtual void draw(VSTGUI::CDrawContext*);
   // virtual void mouse (VSTGUI::CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual bool onWheel(const VSTGUI::CPoint& where, const float& distance, const VSTGUI::CButtonState& buttons);
   bool altlook;

private:
   VSTGUI::CColor fontColor;
   VSTGUI::CColor backColor;
   VSTGUI::CColor lineColor;
   VSTGUI::CColor envColor;
   //	CFont   fontID;
   VSTGUI::CTxtFace txtFace;
   int controlmode, controlstate;
   int i_min, i_max, i_stepsize;
   float f_movespeed;
   float f_min, f_max;
   int i_value;
   int i_default;
   int i_poly;
   char label[32];
   int labelplacement;
   VSTGUI::CRect drawsize;
   VSTGUI::CPoint lastmousepos;

   CLASS_METHODS(CNumberField, VSTGUI::CControl)
};
