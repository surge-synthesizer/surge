#include "globals.h"
#include "SurgeStorage.h"
#include "CEffectSettings.h"
#include "SurgeBitmaps.h"

using namespace VSTGUI;

const int blocks[8][2] = {{18, 1},  {44, 1},  {18, 41}, {44, 41},
                          {18, 21}, {44, 21}, {89, 11}, {89, 31}};

int get_fxtype(int id)
{
   switch (id)
   {
   case fxt_delay:
      return 1;
   case fxt_reverb:
      return 2;
   case fxt_phaser:
      return 7;
   case fxt_rotaryspeaker:
   case fxt_chorus4:
      return 3;
   case fxt_distortion:
      return 4;
   case fxt_eq:
      return 5;
   case fxt_conditioner:
      return 6;
   case fxt_freqshift:
      return 8;
   case fxt_off:
      return 0;
      /*return 6;  // dyn
      return 7;  // fx
      return 8;  // invader 1
      return 9;  // invader 2*/
   }
   return 7;
}

CEffectSettings::CEffectSettings(const CRect& size,
                                 IControlListener* listener,
                                 long tag,
                                 int current)
    : CControl(size, listener, tag, 0)
{
   this->current = current;
   bg = getSurgeBitmap(IDB_FXCONF);
   labels = getSurgeBitmap(IDB_FXCONF_SYMBOLS);
   disabled = 0;
}

void CEffectSettings::draw(CDrawContext* dc)
{
   CRect size = getViewSize();

   if (bg)
   {
      bg->draw(dc, size, CPoint(0, 0), 0xff);
      // source position in bitmap
      // CPoint where (0, heightOfOneImage * (long)((value * (float)(rows*columns - 1) + 0.5f)));
      // pBackground->draw(dc,size,where,0xff);
   }
   if (labels)
   {
      for (int i = 0; i < 8; i++)
      {
         CRect r(0, 0, 17, 9);
         r.offset(size.left, size.top);
         r.offset(blocks[i][0], blocks[i][1]);
         int stype = 0;
         if (disabled & (1 << i))
            stype = 4;
         switch (bypass)
         {
         case fxb_no_fx:
            stype = 2;
            break;
         case fxb_no_sends:
            if ((i == 4) || (i == 5))
               stype = 2;
            break;
         case fxb_scene_fx_only:
            if (i > 3)
               stype = 2;
            break;
         }
         if (i == current)
            stype += 1;
         labels->draw(dc, r, CPoint(17 * stype, 9 * get_fxtype(type[i])), 0xff);
      }
   }
   setDirty(false);
}

CMouseEventResult CEffectSettings::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (!buttons.isLeftButton() && !buttons.isRightButton())
      return kMouseEventHandled;

   for (int i = 0; i < 8; i++)
   {
      CRect size = getViewSize();
      CRect r(0, 0, 17, 9);
      r.offset(size.left, size.top);
      r.offset(blocks[i][0], blocks[i][1]);
      if (r.pointInside(where))
      {
         if (buttons.isRightButton())
            disabled ^= (1 << i);
         else
            current = i;
         invalid();
      }
   }

   if (listener)
      listener->valueChanged(this);
   return kMouseEventHandled;
}

CMouseEventResult CEffectSettings::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   return kMouseEventHandled;
}
