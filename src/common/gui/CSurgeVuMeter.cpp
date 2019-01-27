#include "CSurgeVuMeter.h"
#include "DspUtilities.h"

using namespace VSTGUI;

CSurgeVuMeter::CSurgeVuMeter(const CRect& size) : CControl(size, 0, 0, 0)
{
   stereo = true;
   valueR = 0.0;
   setMax( 2.0 ); // since this can clip values are above 1 sometimes
}

float scale(float x)
{
   x = limit_range(0.5f * x, 0.f, 1.f);
   return powf(x, 0.3333333333f);
}

void CSurgeVuMeter::setType(int vutype)
{
   type = vutype;
   switch (type)
   {
   case vut_vu:
      stereo = false;
      break;
   case vut_vu_stereo:
      stereo = true;
      break;
   case vut_gain_reduction:
      stereo = false;
      break;
   default:
      type = 0;
      break;
   }
}

// void setSecondaryValue(float v);

void CSurgeVuMeter::setValueR(float f)
{
   if (f != valueR)
   {
      valueR = f;
      setDirty();
   }
}

void CSurgeVuMeter::draw(CDrawContext* dc)
{
   const CColor vugreen = {5, 201, 13, 255};
   CRect size = getViewSize();
   CRect lbox(size);

   dc->setFrameColor(kBlackCColor);
   CRect f1(lbox), f2(lbox);
   f1.inset(1, 1);
   f2.inset(0, 2);
   dc->drawRect(f1);
   dc->drawRect(f2);

   lbox.right--;
   lbox.bottom--;

   CRect bar(lbox);
   bar.inset(2, 2);
   if (stereo)
      bar.bottom -= 3;
   CRect barblack(bar);

   float w = bar.getWidth();
   int zerodb = (0.7937f * w);

   dc->drawPoint(CPoint(bar.left + zerodb, size.top), kBlackCColor);
   dc->drawPoint(CPoint(bar.left + zerodb, size.bottom - 1), kBlackCColor);

   if (type == vut_gain_reduction)
   {
      CRect bb2(bar);
      bar.left++;
      bar.top++;
      bar.right = bar.left + zerodb + 1;
      bar.left = bar.left + (scale(value) * w) - 1;
      barblack.right = bar.left + 1;
      barblack.left--;
      bb2.left = bar.right - 1;
      bb2.right++;
      dc->setFillColor(kBlackCColor);
      dc->drawRect(barblack, kDrawFilled);
      dc->drawRect(bb2, kDrawFilled);
      dc->setFillColor(kRedCColor);
      dc->drawRect(bar, kDrawFilled);
   }
   else
   {
      bar.left++;
      bar.top++;
      bar.right = bar.left + (scale(value) * w);
      dc->setFillColor(vugreen);
      if (value > 1.0f)
         dc->setFillColor(kRedCColor);
      dc->drawRect(bar, kDrawFilled);

      barblack.left = bar.right - 1;
      barblack.right++;
      dc->setFillColor(kBlackCColor);
      dc->drawRect(barblack, kDrawFilled);

      if (stereo)
      {
         CRect midline(lbox);
         midline.inset(0, 4);
         midline.left++;
         midline.top++;
         // midline.bottom = midline.top+2;
         bar.offset(0, 3);
         barblack.offset(0, 3);
         bar.right = bar.left + (scale(valueR) * w);
         dc->setFillColor(vugreen);
         if (valueR > 1.0f)
            dc->setFillColor(kRedCColor);
         dc->drawRect(bar, kDrawFilled);

         barblack.left = bar.right - 1;
         dc->setFillColor(kBlackCColor);
         dc->drawRect(barblack, kDrawFilled);
         dc->drawRect(midline, kDrawFilled);
      }
   }
   setDirty(false);
}
