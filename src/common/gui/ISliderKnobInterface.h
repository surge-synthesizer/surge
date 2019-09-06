#pragma once

namespace Surge
{
class ISliderKnobInterface
{
public:
   virtual void setIsMod(bool) = 0;
   virtual void setModMode(int) = 0;
   virtual void setModValue(float) = 0;
   virtual void setModPresent(bool) = 0;
   virtual void setModCurrent(bool) = 0;
   virtual void setValue(float) = 0;
   virtual void setLabel(const char*) = 0;
   virtual void setMoveRate(float) = 0;
   virtual void setDefaultValue(float) = 0;
   virtual void setStyle(int) = 0;
   virtual int  getStyle() = 0;
};
}; // namespace Surge
