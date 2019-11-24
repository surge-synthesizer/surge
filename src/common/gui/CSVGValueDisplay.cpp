#include "CSVGValueDisplay.h"
#include <iostream>

CSVGValueDisplay::CSVGValueDisplay(const VSTGUI::CRect &size,
                                   VSTGUI::IControlListener *listener,
                                   long tag)
   : VSTGUI::CControl( size, listener, tag )
{
}
   
   // virtual void setValue(float f) override;
void CSVGValueDisplay::draw( VSTGUI::CDrawContext *dc )
{
   auto r = getViewSize();
   dc->setFillColor(VSTGUI::kYellowCColor);
   dc->drawRect(r, VSTGUI::kDrawFilled);

   char txt[256];
   sprintf( txt, "v=%f", getValue() );

   auto stringR = r;
   
   VSTGUI::SharedPointer<VSTGUI::CFontDesc> labelFont = new VSTGUI::CFontDesc("Lato", 14);
   dc->setFont(labelFont);
   dc->setFontColor(VSTGUI::kBlackCColor);

   stringR.top = r.getHeight() / 2 - 10;
   stringR.bottom = r.getHeight() / 2 + 10;
   dc->drawString(txt, stringR, VSTGUI::kCenterText, true);

}
