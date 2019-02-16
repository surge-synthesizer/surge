//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeBitmaps.h"

class CEffectSettings : public VSTGUI::CControl
{
public:
   CEffectSettings(const VSTGUI::CRect& size,
                   VSTGUI::IControlListener* listener,
                   long tag,
                   int current,
                   std::shared_ptr<SurgeBitmaps> bitmapStore);
   virtual void draw(VSTGUI::CDrawContext* dc);
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons); ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons); ///< called when a mouse up event occurs

   int current;
   VSTGUI::CBitmap *bg, *labels;
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

   CLASS_METHODS(CEffectSettings, VSTGUI::CControl)
};
