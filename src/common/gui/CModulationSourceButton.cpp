#include "CModulationSourceButton.h"
#include "Colors.h"
#include "MouseCursorControl.h"
#include "globals.h"
#include "ModulationSource.h"
#include "SurgeBitmaps.h"
#include <vt_dsp/basic_dsp.h>

using namespace VSTGUI;

extern CFontRef surge_minifont;

enum
{
   cs_none = 0,
   cs_drag = 1,
};

//------------------------------------------------------------------------------------------------

CModulationSourceButton::CModulationSourceButton(
    const CRect& size, IControlListener* listener, long tag, int state, int msid)
    : CCursorHidingControl(size, listener, tag, 0), OldValue(0.f)
{
   this->state = state;
   this->msid = msid;
   tint = false;
   used = false;
   bipolar = false;
   click_is_editpart = false;
   is_metacontroller = false;
   dispval = 0;
   controlstate = cs_none;
   label[0] = 0;
}

//------------------------------------------------------------------------------------------------

CModulationSourceButton::~CModulationSourceButton()
{}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::update_rt_vals(bool ntint, float fdispval, bool nused)
{
   bool changed = (tint && !ntint) || (!tint && ntint) || (used && !nused) || (!used && nused);
   // int new_dispval = max(0,fdispval * 45);
   int new_dispval = (int)max(0.f, (float)(fdispval * 150.f));
   // changed = changed || (new_dispval != dispval);

   tint = ntint;
   used = nused;
   dispval = new_dispval;

   if (changed)
      setDirty(true);
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::set_ismeta(bool b)
{
   is_metacontroller = b;
   setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::setBipolar(bool b)
{
   bipolar = b;
   setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::setblink(bool nblink)
{
   bool changed = ((blink && !nblink) || (!blink && nblink)) && ((state & 3) == 2);
   blink = nblink;

   if (changed)
   {
      invalid();
   }
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::setlabel(const char* lbl)
{
   strncpy(label, lbl, 16);
   label[15] = 0;
   setDirty();
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::draw(CDrawContext* dc)
{
   CRect sze = getViewSize();
   // sze.offset(-size.left,-size.top);

   const CColor ColSelectedBG = CColor(0, 0, 0, 255);
   const CColor ColBG = CColor(18, 52, 99, 255);
   const CColor ColHover = CColor(103, 167, 253, 255);
   const CColor ColEdge = CColor(46, 134, 254, 255);
   const CColor ColTint = CColor(46, 134, 254, 255);
   const CColor ColSemiTint = CColor(32, 93, 176, 255);
   const CColor ColBlink = CColor(173, 255, 107, 255);

   const int rh = 16;

   /*
        state
        0 - nothing
        1 - selected modeditor
        2 - selected modsource (locked)
        4 [bit 2] - selected arrowbutton [0,1,2 -> 4,5,6]
        */

   // source position in bitmap

   bool ActiveArrow = state > 4;
   bool SelectedModSource = (state & 3) == 1;
   bool ActiveModSource = (state & 3) == 2;
   bool UsedOrActive = used || (state & 3);

   CColor FrameCol, FillCol, FontCol;

   FillCol = ColBG;
   FrameCol = UsedOrActive ? ColEdge : ColSemiTint;
   FontCol = UsedOrActive ? ColEdge : ColSemiTint;
   if (ActiveModSource)
   {
      FrameCol = blink ? ColBlink : ColTint;
      FillCol = blink ? ColBlink : ColTint;
      FontCol = ColBG;
   }
   else if (SelectedModSource)
   {
      FrameCol = ColTint;
      FillCol = ColTint;
      FontCol = ColBG;
   }
   else if (tint)
   {
      FrameCol = ColHover;
      FontCol = ColHover;
   }

   CRect framer(sze);
   CRect fillr(framer);
   fillr.inset(1, 1);
   dc->setFont(surge_minifont);
   dc->setFontColor(FontCol);
   dc->setFrameColor(FrameCol);
   dc->setFillColor(FillCol);
   CRect txtbox(sze);
   txtbox.inset(1, 1);
   txtbox.bottom = txtbox.top + 13;

   dc->setFillColor(FrameCol);
   dc->drawRect(framer, kDrawFilled);
   dc->setFillColor(FillCol);
   dc->drawRect(fillr, kDrawFilled);
   dc->drawString(label, txtbox, kCenterText, true);

   if (is_metacontroller)
   {
      MCRect = sze;
      MCRect.left++;
      MCRect.top++;
      MCRect.top += 12;
      MCRect.bottom--;
      MCRect.right--;
      dc->setFillColor(ColBG);
      dc->drawRect(MCRect, kDrawFilled);
      CRect brect(MCRect);
      brect.inset(1, 1);
      dc->setFillColor(ColSemiTint);
      dc->drawRect(brect, kDrawFilled);

      int midx = brect.left + ((brect.getWidth() - 1) * 0.5);
      int barx = brect.left + (value * (float)brect.getWidth());

      if (bipolar)
      {
         dc->setFillColor(ColTint);
         CRect bar(brect);

         if (barx >= midx)
         {
            bar.left = midx;
            bar.right = barx + 1;
         }
         else
         {
            bar.left = barx;
            bar.right = midx + 2;
         }
         bar.bound(brect);
         dc->drawRect(bar, kDrawFilled);
      }
      else
      {
         CRect vr(brect);
         vr.right = barx + 1;
         vr.bound(brect);
         dc->setFillColor(ColTint);
         if (vr.right > vr.left)
            dc->drawRect(vr, kDrawFilled);
      }
   }

   if (msid >= ms_lfo1 && msid <= ms_slfo6)
   {
      CPoint where;
      where.x = 0;
      if (state >= 4)
         where.y = 8 * rh;
      else
         where.y = 7 * rh;
      getSurgeBitmap(IDB_MODSRC_BG)->draw(dc, sze, where, 0xff);
   }

   setDirty(false);
}

//------------------------------------------------------------------------------------------------

CMouseEventResult CModulationSourceButton::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   super::onMouseDown(where, buttons);

   if (!getMouseEnabled())
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

   CRect size = getViewSize();
   CPoint loc(where);
   loc.offset(-size.left, -size.top);

   if (controlstate)
   {
#if MAC
//		if(buttons & kRButton) statezoom = 0.1f;
#endif
      return kMouseEventHandled;
   }

   if (listener &&
       buttons & (kAlt | kRButton | kMButton | kShift | kControl | kApple | kDoubleClick))
   {
      if (listener->controlModifierClicked(this, buttons) != 0)
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   if (is_metacontroller && MCRect.pointInside(where) && (buttons & kLButton) && !controlstate)
   {
      beginEdit();
      controlstate = cs_drag;

      detachCursor(where);

      return kMouseEventHandled;
   }
   else if (buttons & kLButton)
   {
      click_is_editpart = loc.x > 53;
      event_is_drag = false;
      if (listener)
         listener->valueChanged(this);
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   return kMouseEventHandled;
}

//------------------------------------------------------------------------------------------------

CMouseEventResult CModulationSourceButton::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   super::onMouseUp(where, buttons);

   if (controlstate)
   {
      endEdit();
      controlstate = cs_none;

      attachCursor();
   }
   return kMouseEventHandled;
}

//------------------------------------------------------------------------------------------------

void CModulationSourceButton::onMouseMoveDelta(CPoint& where,
                                               const CButtonState& buttons,
                                               double dx,
                                               double dy)
{
   if ((controlstate) && (buttons & kLButton))
   {
      value += dx / (double)(getWidth());
      value = limit_range(value, 0.f, 1.f);
      event_is_drag = true;
      invalid();
      if (listener)
         listener->valueChanged(this);
   }
}

double CModulationSourceButton::getMouseDeltaScaling(CPoint& where, const CButtonState& buttons)
{
   double scaling = 0.25f;

   if (buttons & kShift)
      scaling *= 0.1;

   return scaling;
}

//------------------------------------------------------------------------------------------------
