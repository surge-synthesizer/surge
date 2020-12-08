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

#include "SurgeGUIEditor.h"
#include "CSurgeSlider.h"
#include "resource.h"
#include "DspUtilities.h"
#include "MouseCursorControl.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeStorage.h"
#include "SkinColors.h"
#include <UserDefaults.h>
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

CSurgeSlider::MoveRateState CSurgeSlider::sliderMoveRateState = kUnInitialized;

CSurgeSlider::CSurgeSlider(const CPoint& loc,
                           long stylee,
                           IControlListener* listener,
                           long tag,
                           bool is_mod,
                           std::shared_ptr<SurgeBitmaps> bitmapStore,
                           SurgeStorage* storage)
    : CControl(CRect(loc, CPoint(1, 1)), listener, tag, 0), Surge::UI::CursorControlAdapterWithMouseDelta<CSurgeSlider>(storage)
{
   this->style = stylee;
   this->is_mod = is_mod;
   
   this->storage = storage;

   labfont = displayFont;
   labfont->remember();

   modmode = 0;
   disabled = false;
   deactivated = false;

   label[0] = 0;
   leftlabel[0] = 0;
   pModHandle = 0;

   typex = 0;
   typey = 0;
   typehx = 0;
   typehy = 0;
   qdvalue = value;

   edit_value = 0;
   drawcount_debug = 0;

   controlstate = cs_none;

   has_modulation = false;
   has_modulation_current = false;

   restvalue = 0.0f;
   restmodval = 0.0f;

   CRect size;

   if (style & CSlider::kHorizontal)
   {
      if (style & kWhite)
         typehy = 1;

      range = 112;
      size = CRect(0, 0, 140, 26);
      size.offset(loc.x, max(0, (int)loc.y));
      handle_rect_orig = CRect(1, 0, 20, 14);
      offsetHandle = CPoint(9, 5);
   }
   else
   {
      if (!(style & CSlider::kTop))
         style |= CSlider::kBottom; // CSlider::kBottom by default

      if (style & kWhite)
         typehy = 0;
      else
         typehy = 1;

      if (style & kMini)
         range = 39;
      else
         range = 56;
      size = CRect(0, 0, 22, 84);
      size.offset(loc.x, max(0, (int)loc.y - 3));
      handle_rect_orig = CRect(1, 0, 15, 19);
      offsetHandle = CPoint(7, 9);
   }

   setViewSize(size);
   setMouseableArea(size);
}

void CSurgeSlider::setModValue(float val)
{
   this->modval = val;
   invalid();
}

CSurgeSlider::~CSurgeSlider()
{
   labfont->forget();
}

void CSurgeSlider::setLabel(const char* txt)
{
   if (!_stricmp(txt, "filter balance"))
   {
      strcpy(label, "F2");
      strcpy(leftlabel, "F1");
   }
   else
   {
      leftlabel[0] = 0;
      int rp = 0;
      for (int i = 0; i < 255; i++)
      {
         label[i] = txt[rp++];
      }
      label[255] = 0;
   }
   setDirty();
}

void CSurgeSlider::draw(CDrawContext* dc)
{
#if 0
	CColor c;
	c.red = rand()&255;
	c.green = rand()&255;
	c.blue = rand()&255;
	dc->setFillColor(c);
	dc->fillRect(size);
#endif

   CRect size = getViewSize();

   if (style & CSlider::kHorizontal)
   {
      if (style & kSemitone)
         typey = 2;
      else if (style & kBipolar)
         typey = 1;
      else
         typey = 0;
      if (style & kWhite)
         typey += 3;
   }
   else
   {
      if (style & kMini)
         typey = 2;
      else if (style & kBipolar)
         typey = 1;
      else
         typey = 0;
   }

   typex = has_modulation ? 1 : 0;
   if (has_modulation_current)
      typex = 2;

   float slider_alpha = (disabled || deactivated) ? 0.35 : 1.0;
   bool showHandle = true;
#if LINUX
   // linux transparency is a bit broken (i have it patched to ignored) as described in
   // #2053. For now just disable handles on linux transparent sliders, which is a bummer
   // but better than a mispaint, then come back to this after 1.7.0
   if( disabled || deactivated )
   {
      showHandle = false;
      slider_alpha = 1.0;
   }
#endif

   if (pTray)
   {
      // CRect trect(0,0,pTray->getWidth(),pTray->getHeight());
      CRect trect;
      if (style & CSlider::kHorizontal)
         trect = CRect(0, 0, 133, 14);
      else
         trect = CRect(0, 0, 16, 75);

      trect.offset(size.left, size.top);

      if (style & CSlider::kHorizontal)
         trect.offset(2, 5);
      else
         trect.offset(2, 2);


      if (style & CSlider::kHorizontal)
         pTray->draw(dc, trect, CPoint(133 * typex, 14 * typey), slider_alpha);
      else
         pTray->draw(dc, trect, CPoint(16 * typex, 75 * typey), slider_alpha);
   }

   CRect headrect;
   if (style & CSlider::kHorizontal)
      headrect = CRect(0, 0, 28, 24);
   else
      headrect = CRect(0, 0, 24, 28);

   if (label[0] && (style & CSlider::kHorizontal))
   {
      CRect trect(0, 0, 111, 13);
      trect.offset(size.left, size.top);
      trect.offset(13, 12);
      trect.inset(2, 0);
      // if (label_id >= 0) pLabels->draw(dc,trect,CPoint(0,8*label_id),0xff);

      if (style & kWhite)
         dc->setFontColor(skin->getColor(Colors::Slider::Label::Light));
      else
         dc->setFontColor(skin->getColor(Colors::Slider::Label::Dark));
      dc->setFont(labfont);

      //		int a = 'a' + (rand()&31);
      //		label[1] = a;
      // sprintf(label,"%i",drawcount_debug++);
      dc->drawString(label, trect, kRightText, true);
      if (leftlabel[0])
         dc->drawString(leftlabel, trect, kLeftText, true);
   }

   CRect hrect(headrect);
   handle_rect = handle_rect_orig;
   hrect.offset(size.left, size.top);
   if (style & CSlider::kHorizontal)
      hrect.offset(0, 3);
   
   float dispv = limit_range(qdvalue, 0.f, 1.f);
   if (style & CSlider::kRight || style & CSlider::kBottom)
      dispv = 1 - dispv;
   dispv *= range;
   
   if (style & CSlider::kHorizontal)
   {
      hrect.offset(dispv + 1, 0);
      handle_rect.offset(dispv + 1, 0);
   }
   else
   {
      hrect.offset(1, dispv);
      handle_rect.offset(1, dispv);
   }

   if( modmode )
   {
      CRect trect = hrect;
      CRect trect2 = hrect;

      CColor ColBar = skin->getColor(Colors::Slider::Modulation::Positive);
      CColor ColBarNeg = skin->getColor(Colors::Slider::Modulation::Negative);

      ColBar.alpha = (int)(slider_alpha * 255.f);
      ColBarNeg.alpha = (int)(slider_alpha * 255.f);

      // We want modval + value to be bound by -1 and 1. So:
      float modup = modval;
      float moddn = modval;
      bool overtop = false;
      bool overbot = false;
      bool overotherside = false;

      if( modup + value > 1.f )
      {
         overtop = true;
         modup = 1.f - value;
      }
      if( modup + value < 0.f )
      {
         overbot = true;
         overotherside = true;
         modup = 0.f - value;
      }
      if( value - moddn < 0.f )
      {
         overbot = true;
         moddn = value + 0.f;
      }
      if( value - moddn > 1.f )
      {
         overtop = true;
         overotherside = true;
         moddn = value - 1.f;
      }

      // at some point in the future draw something special with overtop and overbot
      modup *= range;
      moddn *= range;

      std::vector<CRect> drawThese;
      std::vector<CRect> drawTheseToo;

      if (style & CSlider::kHorizontal)
      {
         trect.top += 8;
         trect.bottom = trect.top + 2;

         trect2.top += 8;
         trect2.bottom = trect2.top + 2;

         if (!modulation_is_bipolar)
         {
            trect.left += 11;
            trect.right = trect.left + modup;
         }
         else
         {
            trect.left += 11;
            trect.right = trect.left + modup;
            
            trect2.right -= 17;
            trect2.left = trect2.right - moddn;
            if (!overotherside && overbot)
               trect2.left += 1;
         }

         if (trect.left > trect.right)
            std::swap(trect.left, trect.right);

         if (trect2.left > trect2.right)
            std::swap(trect2.left, trect2.right);
         
         drawThese.push_back(trect);
         drawTheseToo.push_back(trect2);

         if (overtop)
         {
            CRect topr;

            if (overotherside)
            {
               topr.top = trect2.top;
               topr.bottom = trect2.bottom;
               topr.left = trect2.right + 1;
               topr.right = topr.left + 4;

               drawTheseToo.push_back(topr);
            }
            else
            {
               topr.top = trect.top;
               topr.bottom = trect.bottom;
               topr.left = trect.right + 1;
               topr.right = topr.left + 4;

               drawThese.push_back(topr);
            }
         }

         if (overbot)
         {
            CRect topr;

            if (overotherside)
            {
               topr.top = trect.top;
               topr.bottom = trect.bottom;
               topr.left = trect.left - 3;
               topr.right = topr.left + 1;

               drawThese.push_back(topr);
            }
            else
            {
               topr.top = trect2.top;
               topr.bottom = trect2.bottom;
               topr.left = trect2.left - 5;
               topr.right = topr.left + 4;

               drawTheseToo.push_back(topr);
            }
         }
      }
      else
      {
         trect.left += 8;
         trect.right = trect.left + 2;

         trect2.left += 8;
         trect2.right = trect2.left + 2;

         if (!modulation_is_bipolar)
         {
            trect.top += 11;
            trect.bottom = trect.top - modup;
         }
         else
         {
            trect.top += 11;
            trect.bottom = trect.top - modup;

            trect2.bottom -= 17;
            trect2.top = trect2.bottom + moddn;
            if (overotherside && overtop)
               trect2.top += 1;

         }

         if (trect.top < trect.bottom)
            std::swap(trect.top, trect.bottom);

         if (trect2.top < trect2.bottom)
            std::swap(trect2.top, trect2.bottom);

         if (overbot)
         {
            CRect topr;

            if (overotherside)
            {
               topr.left = trect.left;
               topr.right = trect.right;
               topr.bottom = trect.top + 1;
               topr.top = topr.bottom + 4;

               drawThese.push_back(topr);
            }
            else
            {
               topr.left = trect2.left;
               topr.right = trect2.right;
               topr.bottom = trect2.top + 1;
               topr.top = topr.bottom + 4;

               drawTheseToo.push_back(topr);
            }
         }

         if (overtop)
         {
            CRect topr;

            if (overotherside)
            {
               topr.left = trect2.left;
               topr.right = trect2.right;
               topr.bottom = trect2.bottom - 1;
               topr.top = topr.bottom - 4;

               drawTheseToo.push_back(topr);
            }
            else
            {
               topr.left = trect.left;
               topr.right = trect.right;
               topr.bottom = trect.bottom - 1;
               topr.top = topr.bottom - 4;

               drawThese.push_back(topr);
            }
         }

         drawThese.push_back(trect);
         drawTheseToo.push_back(trect2);
      }

      for (auto r : drawThese)
      {
         dc->setFillColor(ColBar);
         dc->drawRect(r, VSTGUI::kDrawFilled);
      }

      if (modulation_is_bipolar)
      {
          for (auto r : drawTheseToo)
          {
             dc->setFillColor(ColBarNeg);
             dc->drawRect(r, VSTGUI::kDrawFilled);
          }
      }
   }
   

   if (pHandle && showHandle && (modmode != 2) && (!deactivated || !disabled))
   {
      draghandlecenter = hrect.getCenter();

      if (style & CSlider::kHorizontal)
      {
         pHandle->draw(dc, hrect, CPoint(0, 24 * typehy), modmode ? slider_alpha : slider_alpha);
         if( pHandleHover && in_hover && (!modmode) )
            pHandleHover->draw(dc, hrect, CPoint(0, 24 * typehy), modmode ? slider_alpha : slider_alpha);


         if( is_temposync )
         {
            if (in_hover && pTempoSyncHoverHandle && (!modmode))
            {
               pTempoSyncHoverHandle->draw( dc, hrect, CPoint( 0, 0 ), slider_alpha );
            }
            else if( pTempoSyncHandle )
            {
               pTempoSyncHandle->draw(dc, hrect, CPoint(0, 0), slider_alpha);
            }
            else
            {
               dc->setFont(labfont);
               dc->setFontColor(CColor(80,80,100));
               auto newRect = hrect;
               newRect.top += 1;
               newRect.left += 1;
               newRect.bottom = newRect.top + 15;
               newRect.right = newRect.left + 21;
               
               auto tRect = newRect;
               tRect.right = tRect.left + 11;
               tRect.left += 2;
               
               auto sRect = newRect;
               sRect.left += 11;
               sRect.right -= 2;
               dc->drawString("T", tRect, kCenterText, true);
               dc->drawString("S", sRect, kCenterText, true);
            }
         }
      }
      else if ((!deactivated || !disabled))
      {
         pHandle->draw(dc, hrect, CPoint(0, 28 * typehy), modmode ? slider_alpha : slider_alpha); // used to be 0x80 which was solid - lets keep taht for now
         if (pHandleHover && in_hover && (!modmode))
            pHandleHover->draw(dc, hrect, CPoint(0, 28 * typehy), modmode ? slider_alpha : slider_alpha); // used to be 0x80 which was solid - lets keep taht for now

         if( is_temposync )
         {
            if (in_hover && pTempoSyncHoverHandle && (!modmode))
            {
               pTempoSyncHoverHandle->draw( dc, hrect, CPoint( 0, 0 ), slider_alpha );
            }
            else if( pTempoSyncHandle )
            {
               pTempoSyncHandle->draw(dc, hrect, CPoint(0, 0), slider_alpha);
            }
            else
            {
               auto newRect = hrect;
               newRect.top += 1;
               newRect.left += 1;
               newRect.bottom = newRect.top + 20;
               newRect.right = newRect.left + 16;
               
               dc->setFont(labfont);
               dc->setFontColor(CColor(80,80,100));
               
               auto tRect = newRect;
               tRect.bottom = tRect.top + 11;
               tRect.top += 2;
               
               auto sRect = newRect;
               sRect.top += 11;
               sRect.bottom -= 2;
            
               dc->drawString("T", tRect, kCenterText, true);
               dc->drawString("S", sRect, kCenterText, true);
            }
         }
      }
   }

   // draw mod-fader
   if (pHandle && showHandle && modmode && (!deactivated || !disabled))
   {
      CRect hrect(headrect);
      handle_rect = handle_rect_orig;
      hrect.offset(size.left, size.top);
      if (style & CSlider::kHorizontal)
         hrect.offset(0, 3);

      float dispv;
      if (modmode == 2)
         dispv = limit_range(0.5f + 0.5f * modval, 0.f, 1.f);
      else
         dispv = limit_range(modval + value, 0.f, 1.f);

      if (style & CSlider::kRight || style & CSlider::kBottom)
         dispv = 1 - dispv;
      dispv *= range;

      if (style & CSlider::kHorizontal)
      {
         hrect.offset(dispv + 1, 0);
         handle_rect.offset(dispv + 1, 0);
      }
      else
      {
         hrect.offset(1, dispv);
         handle_rect.offset(1, dispv);
      }

      if (style & CSlider::kHorizontal)
      {
         pHandle->draw(dc, hrect, CPoint(28, 24 * typehy), slider_alpha);
         if( pHandleHover && in_hover )
            pHandleHover->draw(dc, hrect, CPoint(28, 24 * typehy), slider_alpha);
      }
      else
      {
         pHandle->draw(dc, hrect, CPoint(24, 28 * typehy), slider_alpha);
         if( pHandleHover && in_hover )
            pHandleHover->draw(dc, hrect, CPoint(24, 28 * typehy), slider_alpha);
      }
      draghandlecenter = hrect.getCenter();
   }

   setDirty(false);
}

void CSurgeSlider::bounceValue(const bool keeprest)
{
    if (keeprest) {
        if (restvalue!=0.0f) {
            value += restvalue;
            restvalue = 0.0f;
        }
        if (restmodval!=0.0f) {
            modval += restmodval;
            restmodval = 0.0f;
        }
    }

   if (value > vmax) {
      restvalue = value - vmax;
      value = vmax;
   }
   else if (value < vmin) {
      restvalue = value - vmin;
      value = vmin;
   }

   if (modval > 1.f) {
       restmodval = modval - 1.f;
       modval = 1.f;
   }
   if (modval < -1.f) {
       restmodval = modval - (-1.f);
       modval = -1.f;
   }
}

bool CSurgeSlider::isInMouseInteraction()
{
   return controlstate == cs_drag;
}

CMouseEventResult CSurgeSlider::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   startPosition = where;
   currentPosition = where;

   {
      auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
      if( sge )
      {
         sge->clear_infoview_peridle = 0;
      }
   }
   if (storage)
      this->hideCursor = !Surge::Storage::getUserDefaultValue(storage, "showCursorWhileEditing", 0);

   hasBeenDraggedDuringMouseGesture = false;
   if( wheelInitiatedEdit )
      while( editing )
         endEdit();
   wheelInitiatedEdit = false;


   if (listener &&
       buttons & (kAlt | kRButton | kMButton | kButton4 | kButton5 | kShift | kControl | kApple | kDoubleClick))
   {
      unenqueueCursorHideIfMoved();

      if (listener->controlModifierClicked(this, buttons) != 0)
      {
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
      }
   }

   onMouseDownCursorHelper(where);

   if (controlstate)
   {
      return kMouseEventHandled;
   }

   if ((buttons & kLButton) && !controlstate)
   {
      beginEdit();
      controlstate = cs_drag;
      // getFrame()->setCursor( VSTGUI::kCursorHand );

      edit_value = modmode ? &modval : &value;
      oldVal = *edit_value;

      restvalue = 0.f;
      restmodval = 0.f;

      // Show the infowindow. Bit heavy handed
      bounceValue();
      if( listener )
         listener->valueChanged( this );
      
      // detachCursor(where);
      enqueueCursorHideIfMoved(where);
      return kMouseEventHandled;
   }
   return kMouseEventHandled;
}

CMouseEventResult CSurgeSlider::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   unenqueueCursorHideIfMoved();
   {
      auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
      if( sge )
      {
         sge->clear_infoview_peridle = -1;
      }
   }

   bool resetPosition = hasBeenDraggedDuringMouseGesture;

   // "elastic edit" - resets to the value before the drag started if Alt is held
   if (buttons & kAlt)
   {
      hasBeenDraggedDuringMouseGesture = false;
      resetPosition = false;
      *edit_value = oldVal;
      setDirty();
      if (isDirty() && listener)
         listener->valueChanged(this);
   }

   if (controlstate)
   {
#if MAC
      /*if(buttons & kRButton)
      {
              statezoom = 1.0f;
              return kMouseEventHandled;
      }*/
#endif
      endEdit();
      controlstate = cs_none;
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
      
      edit_value = nullptr;

      if (resetPosition &&
          startPosition != currentPosition)
      {
         endCursorHide(draghandlecenter);
      }
      else
      {
         endCursorHide();
      }

      //attachCursor();
   }

   return kMouseEventHandled;
}

VSTGUI::CMouseEventResult CSurgeSlider::onMouseEntered(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   in_hover = true;

   {
      auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
      if( sge )
      {
         sge->sliderHoverStart( getTag() );
      }
   }

   invalid();
   return kMouseEventHandled;
}

VSTGUI::CMouseEventResult CSurgeSlider::onMouseExited(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons)
{
   in_hover = false;

   {
      auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
      if( sge )
      {
         sge->sliderHoverEnd( getTag() );
      }
   }
   
   if( wheelInitiatedEdit )
      while( editing )
         endEdit();
   wheelInitiatedEdit = false;
   invalid();
   return kMouseEventHandled;
}

double CSurgeSlider::getMouseDeltaScaling(const CPoint& where, const CButtonState& buttons)
{
   double rate;

   switch (CSurgeSlider::sliderMoveRateState)
   {
   case kSlow:
      rate = 0.3;
      break;
   case kMedium:
      rate = 0.7;
      break;
   case kExact:
      rate = 1.0;
      break;
   case kLegacy:
   default:
      rate = 0.3 * moverate;
      break;
   }

   if (buttons.isTouch())
      rate = 1.0;

   if (buttons & kRButton)
      rate *= 0.1;
   if (buttons & kShift)
      rate *= 0.1;

   return rate;
}

void CSurgeSlider::onMouseMoveDelta(const CPoint& where,
                                    const CButtonState& buttons,
                                    double dx,
                                    double dy)
{
   currentPosition = where;
   if( controlstate != cs_drag )
   {
      // FIXME - deal with modulation
      auto handle_rect = handle_rect_orig;
      auto size = getViewSize();
      handle_rect.offset(size.left, size.top);
      
      float dispv = limit_range(qdvalue, 0.f, 1.f);
      if (style & CSlider::kRight || style & CSlider::kBottom)
         dispv = 1 - dispv;
      dispv *= range;
      
      if( modmode )
      {
         if (modmode == 2)
            dispv = limit_range(0.5f + 0.5f * modval, 0.f, 1.f);
         else
            dispv = limit_range(modval + value, 0.f, 1.f);
         
         if (style & CSlider::kRight || style & CSlider::kBottom)
            dispv = 1 - dispv;
         dispv *= range;
      }
      
      if (style & CSlider::kHorizontal)
      {
         handle_rect.offset(dispv + 1, 3);
      }
      else
      {
         handle_rect.offset(1, dispv);
      }

      /*
      if( handle_rect.pointInside(where) )
         getFrame()->setCursor( VSTGUI::kCursorHand );
      else
         getFrame()->setCursor( VSTGUI::kCursorDefault );
      */
   }
   
   if ((controlstate == cs_drag) && (buttons & kLButton))
   {
      hasBeenDraggedDuringMouseGesture = true;
      if (!edit_value)
         return;
      CPoint p;

      double diff;
      if (style & CSlider::kHorizontal)
         diff = dx;
      else
         diff = dy;

      if (style & CSlider::kRight || style & CSlider::kBottom)
         diff = -diff;

      *edit_value += diff / (float)range;

      // this "retain" means if you turnaround an overshoot you have to use it all up first, vs
      // starting the turnaround right away. That behavior only makes sense in exact mode.
      bounceValue(sliderMoveRateState==MoveRateState::kExact);

      setDirty();

      if (isDirty() && listener)
         listener->valueChanged(this);

      /*if (isDirty ())
              invalid ();*/
   }
}

void CSurgeSlider::SetQuantitizedDispValue(float f)
{
   qdvalue = f;

   // if (isDirty ())
   invalid();
}

void CSurgeSlider::setValue(float val)
{
   if ((controlstate != cs_drag))
   {
      value = val;
      qdvalue = val;
   }
}

void CSurgeSlider::setBipolar(bool b)
{
   if (b)
      style |= kBipolar;
   else
      style &= ~kBipolar;

   setDirty();
}
bool CSurgeSlider::onWheel(const VSTGUI::CPoint& where, const float &distance, const VSTGUI::CButtonState& buttons)
{
   // shift + scrollwheel for fine control
   double rate = 0.1 * moverate;
   if (buttons & kShift)
      rate *= 0.05;
   
   edit_value = modmode ? &modval : &value;
   oldVal = *edit_value;

   if( editing == 0 )
   {
      wheelInitiatedEdit = true;
      beginEdit();
   }
   
   if ( intRange && isStepped && !(buttons & kControl) )
   {
      *edit_value += distance / (intRange);
   }
   else
   {
      *edit_value += rate * distance;
   }
   
   bounceValue();
   if (modmode)
   {
      setModValue(*edit_value);
   }
   else
   {
      setValue(value);
   }
   setDirty();
   if (isDirty() && listener)
      listener->valueChanged(this);

   /*
   ** If we call endEdit it will dismiss the infowindow so instead
   ** use the state management where I have a lignering begin 
   ** which I clear when we click or leave.
   */
   edit_value = nullptr;
   return true;
}

void CSurgeSlider::onSkinChanged()
{
   if (style & CSlider::kHorizontal)
   {
      pTray = associatedBitmapStore->getBitmap(IDB_SLIDER_HORIZ_BG);
      pHandle = associatedBitmapStore->getBitmap(IDB_SLIDER_HORIZ_HANDLE);
      pTempoSyncHandle = associatedBitmapStore->getBitmapByStringID( "TEMPOSYNC_HORIZONTAL_OVERLAY" );
      pTempoSyncHoverHandle = associatedBitmapStore->getBitmapByStringID( "TEMPOSYNC_HORIZONTAL_HOVER_OVERLAY" );
   }
   else
   {
      pTray = associatedBitmapStore->getBitmap(IDB_SLIDER_VERT_BG);
      pHandle = associatedBitmapStore->getBitmap(IDB_SLIDER_VERT_HANDLE);
      pTempoSyncHandle = associatedBitmapStore->getBitmapByStringID( "TEMPOSYNC_VERTICAL_OVERLAY" );
      pTempoSyncHoverHandle = associatedBitmapStore->getBitmapByStringID( "TEMPOSYNC_VERTICAL_HOVER_OVERLAY" );
   }

   if( skinControl.get() )
   {
      auto htr = skin->propertyValue( skinControl, "handle_tray" );
      if( htr.isJust() )
      {
         pTray = associatedBitmapStore->getBitmapByStringID(htr.fromJust());
      }
      auto hi = skin->propertyValue( skinControl, "handle_image" );
      if( hi.isJust() )
      {
         pHandle = associatedBitmapStore->getBitmapByStringID(hi.fromJust());
      }
      auto ho = skin->propertyValue( skinControl, "handle_hover_image" );
      if( ho.isJust() )
      {
         pHandleHover = associatedBitmapStore->getBitmapByStringID(ho.fromJust());
      }
      auto ht = skin->propertyValue( skinControl, "handle_temposync_image" );
      if( ht.isJust() )
      {
         pTempoSyncHandle = associatedBitmapStore->getBitmapByStringID(ht.fromJust());
      }

      auto hth = skin->propertyValue( skinControl, "handle_temposync_hover_image" );
      if( hth.isJust() )
      {
         pTempoSyncHoverHandle = associatedBitmapStore->getBitmapByStringID(hth.fromJust());
      }
   }
   
   if( ! pHandleHover )
      pHandleHover = skin->hoverBitmapOverlayForBackgroundBitmap(skinControl,
                                                                 dynamic_cast<CScalableBitmap*>( pHandle ),
                                                                 associatedBitmapStore,
                                                                 Surge::UI::Skin::HoverType::HOVER);

}
