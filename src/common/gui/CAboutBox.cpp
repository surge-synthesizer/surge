#include "CAboutBox.h"
#include "globals.h"
#include "resource.h"
#include "RuntimeFont.h"
#include <stdio.h>

using namespace VSTGUI;

SharedPointer<CFontDesc> CAboutBox::infoFont;

//------------------------------------------------------------------------
// CAboutBox
//------------------------------------------------------------------------
// one click draw its pixmap, an another click redraw its parent
CAboutBox::CAboutBox(const CRect& size,
                     IControlListener* listener,
                     long tag,
                     CBitmap* background,
                     CRect& toDisplay,
                     CPoint& offset,
                     CBitmap* aboutBitmap)
    : CControl(size, listener, tag, background), toDisplay(toDisplay), offset(offset)
{
   _aboutBitmap = aboutBitmap;
   boxHide(false);
   if (infoFont == NULL)
   {
       infoFont = aboutFont;
   }
}

//------------------------------------------------------------------------
CAboutBox::~CAboutBox()
{}

//------------------------------------------------------------------------
void CAboutBox::draw(CDrawContext* pContext)
{
   if (value)
   {
      _aboutBitmap->draw(pContext, getViewSize(), CPoint(0, 0), 0xff);

      int strHeight = infoFont->getSize(); // There should really be a better API for this in VSTGUI
      std::string bittiness = (sizeof(size_t)==4? std::string("32") : std::string("64")) + " bit";

#if TARGET_AUDIOUNIT      
      std::string flavor = "au";
#elif TARGET_VST3
      std::string flavor = "vst3";
#elif TARGET_VST2
      std::string flavor = "vst2";
#elif TARGET_LV2
      std::string flavor = "lv2";
#else
      std::string flavor = "NON-PLUGIN"; // for linux app
#endif      

#if MAC
      std::string platform = "macOS";
#elif WINDOWS
      std::string platform = "windows";
#elif LINUX
      std::string platform = "linux";
#else
      std::string platform = "orac or skynet or something";
#endif      

      std::vector< std::string > msgs = { {
              std::string() + "Version " + SURGE_STR(SURGE_VERSION) + " (" + bittiness + " " + platform + " " + flavor + ". Built " +
              __DATE__ + " " + __TIME__ + ")",
              "Released under the GNU General Public License, v3",
              "Copyright 2005-2019 by individual contributors",
              "Source, contributors and other information at https://github.com/surge-synthesizer/surge",
              "VST Plug-in technology by Steinberg, AU Plugin Technology by Apple Computer"
          } };

      int yMargin = 5;
      int yPos = toDisplay.getHeight() - msgs.size() * (strHeight + yMargin); // one for the last; one for the margin
      int xPos = strHeight;
      pContext->setFontColor(kWhiteCColor);
      pContext->setFont(infoFont);
      for (auto s : msgs)
      {
          pContext->drawString(s.c_str(), CPoint( xPos, yPos ));
          yPos += strHeight + yMargin;
      }
      pContext->setFontColor(kRedCColor);
      pContext->setFont(infoFont);
      for (auto i = 0; i < 200; i += 20)
         pContext->drawString("EXPERIMENTAL", CPoint(i + 15, i + 15));
   }
   else
   {
   }

   setDirty(false);
}

//------------------------------------------------------------------------
bool CAboutBox::hitTest(const CPoint& where, const CButtonState& buttons)
{
   bool result = CView::hitTest(where, buttons);
   if (result && !(buttons & kLButton))
      return false;
   return result;
}

//------------------------------------------------------------------------

void CAboutBox::boxShow()
{
   setViewSize(toDisplay);
   setMouseableArea(toDisplay);
   value = 1.f;
   getFrame()->invalid();
}
void CAboutBox::boxHide(bool invalidateframe)
{
   CRect nilrect(0, 0, 0, 0);
   setViewSize(nilrect);
   setMouseableArea(nilrect);
   value = 0.f;
   if (invalidateframe)
      getFrame()->invalid();
}

// void CAboutBox::mouse (CDrawContext *pContext, CPoint &where, long button)
CMouseEventResult
CAboutBox::onMouseDown(CPoint& where,
                       const CButtonState& button) ///< called when a mouse down event occurs
{
   if (!(button & kLButton))
      return kMouseEventNotHandled;

   boxHide();

   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

//------------------------------------------------------------------------
void CAboutBox::unSplash()
{
   setDirty();
   value = 0.f;

   setViewSize(keepSize);
   if (getFrame())
   {
      if (getFrame()->getModalView() == this)
      {
          // the modal view is never set anywhere. Replace this with the new vstgui
          // modal view session eventually
          // getFrame()->setModalView(NULL);
      }
   }
}
