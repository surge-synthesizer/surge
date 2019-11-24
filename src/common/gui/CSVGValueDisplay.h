#pragma once

#include "vstcontrols.h"
#include "CScalableBitmap.h"

/*
** A switch which paints its background and draws a glyph over the front.
** When selected the value is the given value; when unselected the
** value is 0.
*/

class CSVGValueDisplay : public VSTGUI::CControl
{
public:
   CSVGValueDisplay(const VSTGUI::CRect &size,
                    VSTGUI::IControlListener *listener,
                    long tag);
   
   // virtual void setValue(float f) override;
   virtual void draw( VSTGUI::CDrawContext *dc ) override;

private:
   CLASS_METHODS( CSVGValueDisplay, VSTGUI::CControl );
};
