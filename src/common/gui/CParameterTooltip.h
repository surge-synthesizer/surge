//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"

class CParameterTooltip : public CControl
{
public:
   CParameterTooltip(const CRect& size) : CControl(size, 0, 0, 0)
   {
      label[0][0] = 0;
      label[1][0] = 0;
      visible = false;
      last_tag = -1;
   }

   void setLabel(const char* txt1, const char* txt2)
   {
      if (txt1)
         strncpy(label[0], txt1, 256);
      else
         label[0][0] = 0;
      if (txt2)
         strncpy(label[1], txt2, 256);
      else
         label[1][0] = 0;
      setDirty(true);
   }

   void Show()
   {
      visible = true;
      invalid();
      // setDirty();
   }
   void Hide()
   {
      visible = false;
      invalid();
      //	setDirty();
   }
   bool isNewTag(long tag)
   {
      bool b = (last_tag != tag);
      last_tag = tag;
      return b;
   }
   bool isVisible()
   {
      return visible;
   }

   virtual void draw(CDrawContext* dc)
   {
      if (visible)
      {
         // COffscreenContext *dc =
         // COffscreenContext::create(getFrame(),size.width(),size.height());

         dc->setFont(kNormalFontSmall);

         CRect smaller = getViewSize();
         int shrink = 0;
         /*if(!label[0][0])
         {
                 int width = dc->getStringWidth(label[1]);
                 shrink = limit_range(150 - width,0,75);
                 
         }
         //smaller.inset(shrink>>1,0);'
         smaller.x += shrink;*/

         auto size = getViewSize();
         dc->setFrameColor(kBlackCColor);
         dc->drawRect(size);
         CRect sizem1(size);
         sizem1.inset(1, 1);
         dc->setFillColor(kWhiteCColor);
         dc->drawRect(sizem1, kDrawFilled);
         dc->setFontColor(kBlackCColor);
         CRect trect(size);
         trect.inset(4, 1);
         trect.right -= shrink;
         CRect tupper(trect), tlower(trect);
         tupper.bottom = tupper.top + 13;
         tlower.top = tlower.bottom - 15;

         if (label[0][0])
            dc->drawString(label[0], tupper, kLeftText, true);
         // dc->drawString(label[1],tlower,false,label[0][0]?kRightText:kCenterText);
         dc->drawString(label[1], tlower, kRightText, true);
         // dc->copyFrom(dc1,smaller);
         // dc->forget();
      }
      setDirty(false);
   }

protected:
   char label[2][256];
   bool visible;
   int last_tag;

   CLASS_METHODS(CParameterTooltip, CControl)
};
