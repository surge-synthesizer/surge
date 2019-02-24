//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CSurgeSlider.h"
#include "resource.h"
#include "DspUtilities.h"
#include "MouseCursorControl.h"
#include "SurgeBitmaps.h"

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;

enum
{
   cs_none = 0,
   cs_drag = 1,
};

CSurgeSlider::CSurgeSlider(
    const CPoint& loc, long stylee, IControlListener* listener, long tag, bool is_mod)
    : CCursorHidingControl(CRect(loc, CPoint(1, 1)), listener, tag, 0)
{
   this->style = stylee;
   this->is_mod = is_mod;

   modmode = 0;
   disabled = false;

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

   CRect size;

   if (style & CSlider::kHorizontal)
   {
      pTray = getSurgeBitmap(IDB_FADERH_BG);
      pHandle = getSurgeBitmap(IDB_FADERH_HANDLE);

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

      pTray = getSurgeBitmap(IDB_FADERV_BG);
      pHandle = getSurgeBitmap(IDB_FADERV_HANDLE);

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
{}

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

      int alpha = 0xff;
      if (disabled)
      {
         typex = 0;
         alpha = 0x80;
      }

      if (style & CSlider::kHorizontal)
         pTray->draw(dc, trect, CPoint(133 * typex, 14 * typey), alpha);
      else
         pTray->draw(dc, trect, CPoint(16 * typex, 75 * typey), alpha);
   }
   if (disabled)
      return;

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
         dc->setFontColor(kWhiteCColor);
      else
         dc->setFontColor(kBlackCColor);
      dc->setFont(displayFont);

      //		int a = 'a' + (rand()&31);
      //		label[1] = a;
      // sprintf(label,"%i",drawcount_debug++);
      dc->drawString(label, trect, kRightText, true);
      if (leftlabel[0])
         dc->drawString(leftlabel, trect, kLeftText, true);
   }

   if (pHandle && (modmode != 2))
   {
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

      if (style & CSlider::kHorizontal)
         pHandle->draw(dc, hrect, CPoint(0, 24 * typehy), modmode ? 0x7f : 0xff);
      else
         pHandle->draw(dc, hrect, CPoint(0, 28 * typehy), modmode ? 0x7f : 0xff);
   }

   // draw mod-fader
   if (pHandle && modmode)
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
         pHandle->draw(dc, hrect, CPoint(28, 24 * typehy), 0xff);
      else
         pHandle->draw(dc, hrect, CPoint(24, 28 * typehy), 0xff);
   }

   setDirty(false);
}

void CSurgeSlider::bounceValue()
{
   if (value > vmax)
      value = vmax;
   else if (value < vmin)
      value = vmin;

   if (modval > 1.f)
      modval = 1.f;
   if (modval < -1.f)
      modval = -1.f;
}

bool CSurgeSlider::isInMouseInteraction()
{
   return controlstate == cs_drag;
}

CMouseEventResult CSurgeSlider::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   CCursorHidingControl::onMouseDown(where, buttons);
   if (disabled)
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   if (controlstate)
   {
#if MAC
      if (buttons & kRButton)
         statezoom = 0.1f;
#endif
      return kMouseEventHandled;
   }

   if (listener &&
       buttons & (kAlt | kRButton | kMButton | kShift | kControl | kApple | kDoubleClick))
   {
      if (listener->controlModifierClicked(this, buttons) != 0)
         return kMouseEventHandled;
   }

   if ((buttons & kLButton) && !controlstate)
   {
      beginEdit();
      controlstate = cs_drag;
      statezoom = 1.f;

      edit_value = modmode ? &modval : &value;
      oldVal = *edit_value;

      detachCursor(where);
      return kMouseEventHandled;
   }
   return kMouseEventHandled;
}

CMouseEventResult CSurgeSlider::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   CCursorHidingControl::onMouseUp(where, buttons);
   if (disabled)
      return kMouseEventHandled;
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
      edit_value = nullptr;

      attachCursor();
   }
   return kMouseEventHandled;
}

double CSurgeSlider::getMouseDeltaScaling(CPoint& where, const CButtonState& buttons)
{
   double rate = 0.3 * moverate;

   if (buttons & kRButton)
      rate *= 0.1;
   if (buttons & kShift)
      rate *= 0.1;

   return rate;
}

void CSurgeSlider::onMouseMoveDelta(CPoint& where,
                                    const CButtonState& buttons,
                                    double dx,
                                    double dy)
{
   if (disabled)
      return;
   if ((controlstate == cs_drag) && (buttons & kLButton))
   {
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

      bounceValue();

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
   
   beginEdit();
   *edit_value += rate * distance;
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
   //endEdit();
   /*
   ** No need to call endEdit since the timer in SurgeGUIEditor will close the
   ** info window, and  SurgeGUIEditor will make sure a window
   ** doesn't appear twice
   */
   edit_value = nullptr;
   return true;   
}