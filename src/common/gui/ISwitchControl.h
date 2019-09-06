#pragma once

namespace Surge
{
class ISwitchControl
{
public:
   int ivalue, imax;
   bool isMultiValued;
   
   virtual void setBitmapForState( int state, VSTGUI::CBitmap *bitmap ) = 0;
};
}
