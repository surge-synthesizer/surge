#include "vstgui/vstgui.h"
#include "CScalableBitmap.h"

namespace Surge
{
class LayoutEngineContainer : public VSTGUI::CViewContainer
{
public:
   LayoutEngineContainer(const VSTGUI::CRect& size);

   ~LayoutEngineContainer() noexcept override = default;

   /** draw the background */
   virtual void drawBackgroundRect(VSTGUI::CDrawContext* pContext,
                                   const VSTGUI::CRect& _updateRect) override;

   std::string label;
   std::string style;
   VSTGUI::CColor bgcolor = VSTGUI::CColor(0, 255, 0);
   VSTGUI::CColor fgcolor = VSTGUI::CColor(0, 0, 0);

   CScalableBitmap *bgSVG = nullptr;
   
   CLASS_METHODS(LayoutEngineContainer, VSTGUI::CViewContainer)
};

class LayoutEngineLabel : public VSTGUI::CTextLabel
{
public:
   LayoutEngineLabel(const VSTGUI::CRect& size, VSTGUI::UTF8StringPtr txt = nullptr)
       : VSTGUI::CTextLabel(size, txt)
   {}
};
} // namespace Surge
