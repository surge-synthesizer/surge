#include "CModulationSourceButton.h"
#include "Colors.h"
#include "MouseCursorControl.h"
#include "globals.h"
#include "ModulationSource.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeStorage.h"
#include <UserDefaults.h>
#include <vt_dsp/basic_dsp.h>
#include <iostream>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;

enum
{
   cs_none = 0,
   cs_drag = 1,
};

class SurgeStorage;

//------------------------------------------------------------------------------------------------

CModulationSourceButton::CModulationSourceButton(const CRect& size,
                                                 IControlListener* listener,
                                                 long tag,
                                                 int state,
                                                 int msid,
                                                 std::shared_ptr<SurgeBitmaps> bitmapStore,
                                                 SurgeStorage* storage)
    : CCursorHidingControl(size, listener, tag, 0), OldValue(0.f)
{
   this->state = state;
   this->msid = msid;
   
   this->storage = storage;

   tint = false;
   used = false;
   bipolar = false;
   click_is_editpart = false;
   is_metacontroller = false;
   dispval = 0;
   controlstate = cs_none;
   label[0] = 0;
   blink = 0;
   bmp = bitmapStore->getBitmap(IDB_MODSRC_BG);
   this->storage = nullptr;
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

   auto brighten = [](const CColor &c) -> CColor
                      {
                         auto fac = 1.4f;
                         return CColor( std::min( (int)(c.red * fac), 255 ),
                                        std::min( (int)(c.green * fac), 255 ),
                                        std::min( (int)(c.blue * fac), 255 ) );
                      };

   FillCol = skin->getColor( "modbutton.inactive.fill", CColor(18, 52, 99, 255) );
   FrameCol = UsedOrActive ? skin->getColor( "modbutton.inactive.frame", ColEdge ) :
      skin->getColor( "modbutton.used.frame", ColSemiTint );
   FontCol = UsedOrActive ? skin->getColor( "modbutton.inactive.font", ColEdge ) :
      skin->getColor( "modbutton.used.font", ColSemiTint );
   if( hovered )
   {
      FrameCol = UsedOrActive ? skin->getColor( "modbutton.inactive.frame.hover", brighten( FrameCol ) ):
         skin->getColor( "modbutton.used.frame.hover", brighten( FrameCol ) );
      FontCol = UsedOrActive ? skin->getColor( "modbutton.inactive.font.hover", brighten( ColEdge )  ):
         skin->getColor( "modbutton.used.font.hover", brighten( ColSemiTint ) );
   }
   
   if (ActiveModSource)
   {
      FrameCol = skin->getColor( "modbutton.active.frame", ColBlink ); // blink ? ColBlink : ColTint;
      if( hovered )
         FrameCol = skin->getColor( "modbutton.active.frame.hover", brighten( FrameCol ) );
      
      FillCol = skin->getColor( "modbutton.active.fill", ColBlink ); // blink ? ColBlink : ColTint;
      FontCol = skin->getColor( "modbutton.active.font", CColor(18, 52, 99, 255) );
      if( hovered )
         FontCol = skin->getColor( "modbutton.active.font.hover", brighten( CColor(18, 52, 99, 255) ) );
   }
   else if (SelectedModSource)
   {
      FrameCol = skin->getColor( "modbutton.selected.frame", ColTint );
      if( hovered )
         FrameCol = skin->getColor( "modbutton.selected.frame.hover", brighten( FrameCol ) );
      FillCol = skin->getColor( "modbutton.selected.fill", ColTint );
      FontCol = skin->getColor( "modbutton.selected.font", CColor(18, 52, 99, 255) );
      if( hovered )
         FontCol = skin->getColor( "modbutton.selected.font.hover", brighten( CColor(18, 52, 99, 255) ) );
   }
   else if (tint)
   {
      FillCol = skin->getColor( "modbutton.used.fill", FillCol );
      FrameCol = skin->getColor( "modbutton.used.frame", ColHover );
      if( hovered )
         FrameCol = skin->getColor( "modbutton.used.frame.hover", brighten( FrameCol ) );
      FontCol = skin->getColor( "modbutton.used.font", ColHover );
      if( hovered )
         FontCol = skin->getColor( "modbutton.used.font.hover", brighten( ColHover ) );
   }

   CRect framer(sze);
   CRect fillr(framer);
   fillr.inset(1, 1);
   dc->setFont(displayFont);
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

   if( hasAlternate && useAlternate )
   {
      dc->drawString(alternateLabel.c_str(), txtbox, kCenterText, true);
   }
   else
   {
      dc->drawString(label, txtbox, kCenterText, true);
   }

   if (is_metacontroller)
   {
      MCRect = sze;
      MCRect.left++;
      MCRect.top++;
      MCRect.top += 12;
      MCRect.bottom--;
      MCRect.right--;
      dc->setFillColor(skin->getColor( "modbutton.inactive.fill", CColor(18, 52, 99, 255) ));
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
      bmp->draw(dc, sze, where, 0xff);
   }

   setDirty(false);
}

//------------------------------------------------------------------------------------------------

CMouseEventResult CModulationSourceButton::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (storage)
      this->hideCursor = Surge::Storage::getUserDefaultValue(storage, "showCursorWhileEditing", 0);

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
       buttons & (kAlt | kRButton | kMButton | kButton4 | kButton5 | kShift | kControl | kApple | kDoubleClick))
   {
      if (listener->controlModifierClicked(this, buttons) != 0)
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   if (is_metacontroller && (buttons & kDoubleClick) && MCRect.pointInside(where) && !controlstate)
   {
      if (bipolar)
         value = 0.5;
      else
         value = 0;

      invalid();
      if (listener)
         listener->valueChanged(this);

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

bool CModulationSourceButton::onWheel(const VSTGUI::CPoint& where, const float &distance, const VSTGUI::CButtonState& buttons)
{
    if( ! is_metacontroller )
        return false;
    
    auto rate = 1.f;

#if WINDOWS
    // This is purely empirical. The wheel for this control feels slow on windows
    // but fine on a macos trackpad. I callibrated it by making sure the trackpad
    // across the pad gesture on my macbook pro moved the control the same amount
    // in surge mac as it does in vst3 bitwig in parallels.
    rate = 10.f;
#endif      

    if( buttons & kShift )
      rate *= 0.05;
    
    value += rate * distance / (double)(getWidth());
    value = limit_range(value, 0.f, 1.f);
    event_is_drag = true;
    invalid();
    if (listener)
        listener->valueChanged(this);
    return true;
}

//------------------------------------------------------------------------------------------------
