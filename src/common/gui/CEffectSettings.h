//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CEffectSettings : public CControl
{
public:
   CEffectSettings(const CRect& size, IControlListener* listener, long tag, int current);
   virtual void draw(CDrawContext* dc);
   virtual CMouseEventResult
   onMouseDown(CPoint& where,
               const CButtonState& buttons); ///< called when a mouse down event occurs
   virtual CMouseEventResult
   onMouseUp(CPoint& where, const CButtonState& buttons); ///< called when a mouse up event occurs

   int current;
   CBitmap *bg, *labels;
   int type[8], bypass, disabled;

   void set_type(int id, int t)
   {
      type[id] = t;
      setDirty(true);
   }

   void set_bypass(int bid)
   {
      bypass = bid;
      invalid();
   }

   void set_disable(int did)
   {
      disabled = did;
   }

   int get_disable()
   {
      return disabled;
   }

   int get_current()
   {
      return current;
   }

   CLASS_METHODS(CEffectSettings, CControl)
};