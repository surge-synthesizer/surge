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

class CParamEdit : public CControl
{
public:
   CParamEdit(const CRect& size,
              IControlListener* listener = 0,
              long tag = 0,
              CBitmap* pBackground = 0);
   ~CParamEdit();

   virtual void setFontColor(CColor color);
   CColor getFontColor()
   {
      return fontColor;
   }

   virtual void setBackColor(CColor color);
   CColor getBackColor()
   {
      return backColor;
   }

   virtual void setLineColor(CColor color);
   CColor getLineColor()
   {
      return lineColor;
   }

   // virtual void setTxtFace (CTxtFace val);
   CTxtFace getTxtFace()
   {
      return txtFace;
   }

   /*virtual void setFont (CFont fontID);
   CFont getFont () { return fontID; }*/

   void setBgcolor(CColor bgcol)
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

   virtual void draw(CDrawContext*);
   // virtual void mouse (CDrawContext *pContext, CPoint &where, long buttons = -1);
   virtual CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons);
   virtual CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons);
   virtual CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons);
   // virtual bool onWheel (CDrawContext *pContext, const CPoint &where, float distance);
   bool altlook;

private:
   CColor fontColor;
   CColor backColor;
   CColor lineColor;
   CColor envColor;
   //	CFont   fontID;
   CTxtFace txtFace;
   int controlmode, controlstate;
   int i_min, i_max, i_stepsize;
   float f_movespeed;
   float f_min, f_max;
   int i_value;
   int i_default;
   int i_poly;
   char label[32];
   int labelplacement;
   CRect drawsize;
   CPoint lastmousepos;

   CLASS_METHODS(CParamEdit, CControl)
};
