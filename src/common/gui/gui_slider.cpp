//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "gui_slider.h"
#include "resource.h"
#include "dsputils.h"
#include "cursormanipulation.h"
#include "surge_bitmap_keeper.h"

extern CFontRef surge_minifont;

enum
{
   cs_none = 0,
   cs_drag = 1,
};

gui_slider::gui_slider(
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

   if (style & kHorizontal)
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
      if (!(style & kTop))
         style |= kBottom; // kBottom by default

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

void gui_slider::setModValue(float val)
{
   this->modval = val;
   invalid();
}

gui_slider::~gui_slider()
{}

void gui_slider::setLabel(const char* txt)
{
   if (!stricmp(txt, "filter balance"))
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

void gui_slider::draw(CDrawContext* dc)
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

   if (style & kHorizontal)
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
      if (style & kHorizontal)
         trect = CRect(0, 0, 133, 14);
      else
         trect = CRect(0, 0, 16, 75);

      trect.offset(size.left, size.top);

      if (style & kHorizontal)
         trect.offset(2, 5);
      else
         trect.offset(2, 2);

      int alpha = 0xff;
      if (disabled)
      {
         typex = 0;
         alpha = 0x80;
      }

      if (style & kHorizontal)
         pTray->draw(dc, trect, CPoint(133 * typex, 14 * typey), alpha);
      else
         pTray->draw(dc, trect, CPoint(16 * typex, 75 * typey), alpha);
   }
   if (disabled)
      return;

   CRect headrect;
   if (style & kHorizontal)
      headrect = CRect(0, 0, 28, 24);
   else
      headrect = CRect(0, 0, 24, 28);

   if (label[0] && (style & kHorizontal))
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
      dc->setFont(surge_minifont);

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
      if (style & kHorizontal)
         hrect.offset(0, 3);

      float dispv = limit_range(qdvalue, 0.f, 1.f);
      if (style & kRight || style & kBottom)
         dispv = 1 - dispv;
      dispv *= range;

      if (style & kHorizontal)
      {
         hrect.offset(dispv + 1, 0);
         handle_rect.offset(dispv + 1, 0);
      }
      else
      {
         hrect.offset(1, dispv);
         handle_rect.offset(1, dispv);
      }

      if (style & kHorizontal)
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
      if (style & kHorizontal)
         hrect.offset(0, 3);

      float dispv;
      if (modmode == 2)
         dispv = limit_range(0.5f + 0.5f * modval, 0.f, 1.f);
      else
         dispv = limit_range(modval + value, 0.f, 1.f);

      if (style & kRight || style & kBottom)
         dispv = 1 - dispv;
      dispv *= range;

      if (style & kHorizontal)
      {
         hrect.offset(dispv + 1, 0);
         handle_rect.offset(dispv + 1, 0);
      }
      else
      {
         hrect.offset(1, dispv);
         handle_rect.offset(1, dispv);
      }

      if (style & kHorizontal)
         pHandle->draw(dc, hrect, CPoint(28, 24 * typehy), 0xff);
      else
         pHandle->draw(dc, hrect, CPoint(24, 28 * typehy), 0xff);
   }

   setDirty(false);
}

void gui_slider::bounceValue()
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

bool gui_slider::isInMouseInteraction()
{
   return controlstate == cs_drag;
}

CMouseEventResult gui_slider::onMouseDown(CPoint& where, const CButtonState& buttons)
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

CMouseEventResult gui_slider::onMouseUp(CPoint& where, const CButtonState& buttons)
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

double gui_slider::getMouseDeltaScaling(CPoint& where, const CButtonState& buttons)
{
   double rate = 0.3 * moverate;

   if (buttons & kRButton)
      rate *= 0.1;
   if (buttons & kShift)
      rate *= 0.1;

   return rate;
}

void gui_slider::onMouseMoveDelta(CPoint& where, const CButtonState& buttons, double dx, double dy)
{
   if (disabled)
      return;
   if ((controlstate == cs_drag) && (buttons & kLButton))
   {
      if (!edit_value)
         return;
      CPoint p;

      double diff;
      if (style & kHorizontal)
         diff = dx;
      else
         diff = dy;

      if (style & kRight || style & kBottom)
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

void gui_slider::SetQuantitizedDispValue(float f)
{
   qdvalue = f;

   // if (isDirty ())
   invalid();
}

void gui_slider::setValue(float val)
{
   if ((controlstate != cs_drag))
   {
      value = val;
      qdvalue = val;
   }
}

void gui_slider::setBipolar(bool b)
{
   if (b)
      style |= kBipolar;
   else
      style &= ~kBipolar;

   setDirty();
}
