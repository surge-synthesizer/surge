#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "ISliderKnobInterface.h"

class CSurgeKnob : public VSTGUI::CKnob, public virtual Surge::ISliderKnobInterface
{
public:
   CSurgeKnob(VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag)
       : VSTGUI::CKnob(size,
                       listener,
                       tag,
                       nullptr,
                       nullptr,
                       VSTGUI::CPoint(0, 0),
                       VSTGUI::CKnob::kCoronaOutline | VSTGUI::CKnob::kCoronaDrawing)
   {
      setCoronaInset(7);
      setCoronaOutlineWidthAdd(4);
      setHandleLineWidth(3);
   }

   virtual void setModMode(int f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setModPresent(bool f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setIsMod(bool f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setModCurrent(bool f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setModValue(float f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setValue(float f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setLabel(const char* f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setMoveRate(float f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }
   virtual void setDefaultValue(float f)
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }

   virtual void setStyle(int i ) 
   {
      std::cerr << "FIXME " << __func__ << std::endl;
   }

   virtual int getStyle() 
   {
      std::cerr << "FIXME " << __func__ << std::endl;
      return 0;
   }
};
