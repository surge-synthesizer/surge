//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

enum CControlEnum_turbodeluxe
{
   kBipolar = 1 << 15,
   kWhite = 1 << 16,
   kSemitone = 1 << 17,
   kMini = 1 << 18,
   kMeta = 1 << 19,
   kEasy = 1 << 20,
   kHide = 1 << 21,
   kNoPopup = 1 << 22,
};

class CSurgeSlider : public CCursorHidingControl
{
public:
   CSurgeSlider(const CPoint& loc,
                long style,
                IControlListener* listener = 0,
                long tag = 0,
                bool is_mod = false);
   ~CSurgeSlider();
   virtual void draw(CDrawContext*);
   // virtual void mouse (CDrawContext *pContext, CPoint &where, long buttons = -1);
   // virtual bool onWheel (CDrawContext *pContext, const CPoint &where, float distance);

   virtual CMouseEventResult
   onMouseDown(CPoint& where,
               const CButtonState& buttons); ///< called when a mouse down event occurs
   virtual CMouseEventResult
   onMouseUp(CPoint& where, const CButtonState& buttons); ///< called when a mouse up event occurs

   virtual double getMouseDeltaScaling(CPoint& where, const CButtonState& buttons);
   virtual void onMouseMoveDelta(CPoint& where, const CButtonState& buttons, double dx, double dy);

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
   virtual void setModCurrent(bool b)
   {
      has_modulation_current = b;
      invalid();
   } // - " " - for the currently selected modsource
   virtual void bounceValue();

   virtual bool isInMouseInteraction();

   virtual void setValue(float val);
   virtual void setBipolar(bool);

   void SetQuantitizedDispValue(float f);

   CLASS_METHODS(CSurgeSlider, CControl)

   bool is_mod;
   bool disabled;

private:
   CBitmap *pHandle, *pTray, *pModHandle;
   CRect handle_rect, handle_rect_orig;
   CPoint offsetHandle;
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
   CPoint lastpoint, sourcepoint;
   float oldVal, *edit_value;
   int drawcount_debug;
};