#include "CSurgeVuMeter.h"
#include "DspUtilities.h"
#include "SkinColors.h"

using namespace VSTGUI;

CSurgeVuMeter::CSurgeVuMeter(const CRect& size, VSTGUI::IControlListener *listener) : CControl(size, listener, 0, 0)
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

CMouseEventResult CSurgeVuMeter::onMouseDown(CPoint& where, const CButtonState& button)
{
   if (listener && (button & (kMButton | kButton4 | kButton5)))
   {
      listener->controlModifierClicked(this, button);
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }
   return CControl::onMouseDown(where, button);
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
   auto vulow = skin->getColor(Colors::VuMeter::LowLevel);
   auto vulowNotch = skin->getColor(Colors::VuMeter::LowLevelNotch);

   auto vumid = skin->getColor(Colors::VuMeter::MidLevel);
   auto vumidNotch = skin->getColor(Colors::VuMeter::MidLevelNotch);

   auto vuhigh = skin->getColor(Colors::VuMeter::HighLevel);
   auto vuhighNotch = skin->getColor(Colors::VuMeter::HighLevelNotch);

   auto vugr = skin->getColor(Colors::VuMeter::GRLevel);
   auto vugrNotch = skin->getColor(Colors::VuMeter::GRLevelNotch);

   auto isNotched = (vulow != vulowNotch);

   auto notchDistance = 2;

   auto borderCol = skin->getColor(Colors::VuMeter::Border);

   CRect componentSize = getViewSize();

   VSTGUI::CDrawMode origMode = dc->getDrawMode();
   VSTGUI::CDrawMode newMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode );
   dc->setDrawMode(newMode);

   dc->setFillColor(skin->getColor(Colors::VuMeter::Background));
   dc->drawRect(componentSize, VSTGUI::kDrawFilled);

   CRect borderRoundedRectBox = componentSize;
   borderRoundedRectBox.inset(1, 1);
   VSTGUI::CGraphicsPath* borderRoundedRectPath = dc->createRoundRectGraphicsPath(borderRoundedRectBox, 2);

   dc->setFillColor(borderCol);
   dc->drawGraphicsPath(borderRoundedRectPath, VSTGUI::CDrawContext::kPathFilled);

   CRect vuBarRegion(componentSize);

   if (stereo)
   {
      vuBarRegion.inset(3, 3);
   }
   else
   {
      vuBarRegion.inset(3, 2);
   }
   vuBarRegion.bottom -= 1;

   float w = vuBarRegion.getWidth();
   /*
    * Where does this constant come from?
    * Well in 'scale' we map x -> x^3 to compress the range
    * cuberoot(2) = 1.26 and change
    * 1 / cuberoot(2) = 0.7937
    * so this scales it so the zerodb point is 1/cuberoot(2) along
    * still not entirely sure why, but there you go
    */
   int zerodb = (0.7937f * w);

   // since the mid levels draw as a gradient, start it earlier
   int midleveldb = zerodb * 0.25;

   if (type == vut_gain_reduction)
   {
      // OK we need to draw from zerodb to the value
      int yMargin = 1;
      auto startingFromPoint = CPoint( vuBarRegion.right, vuBarRegion.top + yMargin );
      auto displayRect = CRect(startingFromPoint,
                               CPoint((scale(value) * (w - 1)) - zerodb, vuBarRegion.getHeight() - yMargin));
                                                      // this -1 somehow aligns the right edge of GR meter nicely with regular meter's right edge
                                                      // there's probably a better way but I don't know it

      dc->setFillColor(vugr);
      dc->drawRect( displayRect, kDrawFilled);

      // Draw over the notches
      if (isNotched)
      {
         auto start = displayRect.left;
         auto end = displayRect.right;
   
         if( displayRect.getWidth() < 0 )
         {
            auto notches = ceil( displayRect.getWidth() / notchDistance );
            start = start + notches * notchDistance;
            end = displayRect.left;
         }
   
         dc->setFillColor(vugrNotch);
   
         for( auto pos = start; pos <= end; pos += notchDistance )
         {
            auto notch = CRect(CPoint(pos, displayRect.top),CPoint(1, displayRect.getHeight()));
            dc->drawRect( notch, kDrawFilled );
         }
      }
   }
   else
   {
      auto vuBarLeft = vuBarRegion;
      auto vuBarRight = vuBarRegion;

      vuBarLeft.right = vuBarLeft.left + scale(value) * w;

      if (stereo)
      {
         auto center = vuBarLeft.top + vuBarLeft.getHeight() / 2;

         vuBarLeft.bottom = center;
         vuBarRight.top = center;
         vuBarRight.offset(0, 1); // jog down one pixel to make a gap
         vuBarRight.right = vuBarRight.left + scale(valueR) * w;
      }

      auto drawMultiNotchedBar = [&](const CRect &targetBar )
      {
         auto drawThis = targetBar;
         auto zp = vuBarLeft.left + zerodb;
         auto ap = vuBarLeft.left + midleveldb;

         if (drawThis.right > zp)
         {
            auto highArea = drawThis;
            highArea.left = zp;

            dc->setFillColor(vuhigh);
            dc->drawRect(highArea, kDrawFilled);

            if (isNotched)
            {
               dc->setFillColor(vuhighNotch);
               
               for (auto pos = drawThis.left; pos <= drawThis.right; pos += notchDistance)
               {
                  if (pos >= zp)
                  {
                     auto notch = CRect(CPoint(pos, drawThis.top), CPoint(1, drawThis.getHeight()));
                     dc->drawRect(notch, kDrawFilled);
                  }
               }
            }

            drawThis.right = zp;
         }

         if( drawThis.right >= ap )
         {
            float dpx = 1.0 / ( zp - ap );

            for( auto pos = drawThis.left; pos <= drawThis.right; pos += notchDistance )
            {
               // a hand-drawn gradient, but we need to gradient the notch markers so this calculation is kinda what we need
               if( pos + notchDistance > ap )
               {
                  auto r = CRect( CPoint( pos, drawThis.top), CPoint( notchDistance, drawThis.getHeight()));
                  float frac = limit_range( (float)( pos - ap ) * dpx, 0.f, 1.f);
                  auto c = VSTGUI::CColor(vulow.red * (1 - frac) + vumid.red * ( frac ),
                                          vulow.green * (1 - frac) + vumid.green * ( frac ),
                                          vulow.blue * (1 - frac) + vumid.blue * ( frac )
                  );

                  dc->setFillColor(c);
                  dc->drawRect(r, kDrawFilled);

                  if (isNotched)
                  {
                     auto cN = VSTGUI::CColor(vulowNotch.red * (1 - frac) + vumidNotch.red * ( frac ),
                                             vulowNotch.green * (1 - frac) + vumidNotch.green * ( frac ),
                                             vulowNotch.blue * (1 - frac) + vumidNotch.blue * ( frac )
                     );
                     
                     dc->setFillColor(cN);
                     
                     auto notch = CRect(CPoint(pos, drawThis.top),CPoint(1, drawThis.getHeight()));
                     dc->drawRect( notch, kDrawFilled );
                  }
               }
            }

            drawThis.right = ap;
         }

         dc->setFillColor( vulow );
         dc->drawRect( drawThis, kDrawFilled );

         if (isNotched)
         {
            dc->setFillColor(vulowNotch);
            
            for( auto pos = drawThis.left; pos <= drawThis.right; pos += notchDistance )
            {
              auto notch = CRect(CPoint(pos, drawThis.top),CPoint(1, drawThis.getHeight()));
              dc->drawRect( notch, kDrawFilled );
            }
         }
      };

      drawMultiNotchedBar(vuBarLeft);
      if( stereo )
      {
         drawMultiNotchedBar(vuBarRight);
      }
   }

   dc->setFrameColor(skin->getColor(Colors::VuMeter::Background));
   dc->setLineWidth(1);
   dc->drawGraphicsPath(borderRoundedRectPath, VSTGUI::CDrawContext::kPathStroked);

   borderRoundedRectPath->forget();
   dc->setDrawMode(origMode);

   setDirty(false);
}
