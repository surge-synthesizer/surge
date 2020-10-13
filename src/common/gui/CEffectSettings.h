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
#include "SkinSupport.h"

class CEffectSettings : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:
   CEffectSettings(const VSTGUI::CRect& size,
                   VSTGUI::IControlListener* listener,
                   long tag,
                   int current,
                   std::shared_ptr<SurgeBitmaps> bitmapStore);
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual VSTGUI::CMouseEventResult
   onMouseDown(VSTGUI::CPoint& where,
               const VSTGUI::CButtonState& buttons) override; ///< called when a mouse down event occurs
   virtual VSTGUI::CMouseEventResult
   onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override; ///< called when a mouse up event occurs

   virtual VSTGUI::CMouseEventResult onMouseMoved( VSTGUI::CPoint &where, const VSTGUI::CButtonState &buttons ) override;

   enum MouseActionMode {
      none,
      click,
      drag
   } mouseActionMode = none;
   VSTGUI::CPoint dragStart, dragCurrent, dragCornerOff;
   int dragSource = -1;

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
