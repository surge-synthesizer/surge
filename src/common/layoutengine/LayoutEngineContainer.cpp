#include "LayoutEngineContainer.h"
#include "LayoutLog.h"
#include <iostream>

namespace Surge
{
LayoutEngineContainer::LayoutEngineContainer(const VSTGUI::CRect& rect)
    : VSTGUI::CViewContainer(rect)
{}

#define DUMPR(r)                                                                                   \
   "(x=" << r.getTopLeft().x << ",y=" << r.getTopLeft().y << ")+(w=" << r.getWidth()               \
         << ",h=" << r.getHeight() << ")"

void LayoutEngineContainer::drawBackgroundRect(VSTGUI::CDrawContext* dc, const VSTGUI::CRect& ud)
{
   if (style == "fill")
   {
      dc->setFillColor(bgcolor);
      dc->drawRect(ud, VSTGUI::kDrawFilled);
   }
   else if (style == "blank")
   {
      return;
   }
   else if (style == "labelblock")
   {
      dc->setFillColor(bgcolor);
      dc->setFrameColor(fgcolor);
      dc->drawRect(ud, VSTGUI::kDrawFilledAndStroked);

      VSTGUI::SharedPointer<VSTGUI::CFontDesc> labelFont = new VSTGUI::CFontDesc("Lato", 14);
      dc->setFont(labelFont);
      dc->setFontColor(fgcolor);

      auto stringR = getViewSize(); stringR.top = 0; stringR.left = 0;
      stringR.top = ud.getHeight() / 2 - 10;
      stringR.bottom = ud.getHeight() / 2 + 10;
      dc->drawString(label.c_str(), stringR, VSTGUI::kCenterText, true);

      return;
   }
   else if (style == "roundedblock" || style == "roundedlabel" )
   {
      auto rr =  getViewSize();
      auto t = rr.top;
      rr.offset( -rr.left, -rr.top );
      rr.inset(2,2);
      dc->setClipRect(ud);
      auto rp = dc->createRoundRectGraphicsPath( rr, 7 );
      dc->setDrawMode(VSTGUI::kAntiAliasing);
      dc->setFillColor(bgcolor);
      dc->drawGraphicsPath(rp, VSTGUI::CDrawContext::kPathFilled);
      dc->setFrameColor(fgcolor);
      dc->setLineWidth(1.6);
      dc->drawGraphicsPath(rp, VSTGUI::CDrawContext::kPathStroked);
      rp->forget();


      if( style == "roundedlabel" )
      {
         VSTGUI::SharedPointer<VSTGUI::CFontDesc> labelFont = new VSTGUI::CFontDesc("Lato", 14);
         dc->setFont(labelFont);
         dc->setFontColor(fgcolor);
         
         auto stringR = ud;
         stringR.top = ud.getHeight() / 2 - 10;
         stringR.bottom = ud.getHeight() / 2 + 10;
         dc->drawString(label.c_str(), stringR, VSTGUI::kCenterText, true);
      }
      return;
   }
   else if( style == "svg" && bgSVG != nullptr )
   {
      VSTGUI::CPoint where(0,0);
      bgSVG->draw(dc, ud, where, 0xff );
   }
   else
   {
      if (style != "" && style != "debug")
      {
         // LayoutLog::warn() << "Didn't understand style '" << style << "'. Defaulting to debug" <<
         // std::endl;
      }
      auto rect = ud;

      int r = bgcolor.red, g = bgcolor.green, b = bgcolor.blue;
      int rt, gt, bt;
      int amp = 50;

      if (r > 128)
      {
         rt = std::max((int)r - amp, 0);
      }
      else
      {
         rt = std::min((int)r + amp, 255);
      }
      if (g > 128)
      {
         gt = std::max((int)g - amp, 0);
      }
      else
      {
         gt = std::min((int)g + amp, 255);
      }
      if (b > 128)
      {
         bt = std::max((int)b - amp, 0);
      }
      else
      {
         bt = std::min((int)b + amp, 255);
      }

      dc->setFrameColor(VSTGUI::CColor(rt, gt, bt));
      dc->setFillColor(bgcolor);
      dc->drawRect(rect, VSTGUI::kDrawFilledAndStroked);

      VSTGUI::SharedPointer<VSTGUI::CFontDesc> lato8 = new VSTGUI::CFontDesc("Lato", 8);
      dc->setFont(lato8);
      dc->setFontColor(VSTGUI::CColor(rt, gt, bt));

      int tw = 20;
      int th = 10;
      for (int i = 0; i < rect.getWidth(); i += tw)
      {
         for (int j = th; j < rect.getHeight(); j += th)
         {
            dc->drawString(label.c_str(), VSTGUI::CPoint(i, j), true);
         }
      }
   }
}
} // namespace Surge
