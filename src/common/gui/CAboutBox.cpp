#include "CAboutBox.h"
#include "globals.h"
#include "resource.h"
#include "RuntimeFont.h"
#include <stdio.h>
#include "version.h"

#include "UserInteractions.h"
#include "SkinColors.h"

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
      CRect infobg(0, skin->getWindowSizeY() - 167, skin->getWindowSizeX(), skin->getWindowSizeY());
      CRect skininfobg(0, 0, BASE_WINDOW_SIZE_X, 80);
#if BUILD_IS_DEBUG
      /*
       * This code is here JUST because baconpaul keeps developing surge and then swapping
       * to make music and wondering why LPX is stuttering. Please don't remove it!
       */
      pContext->setFillColor(CColor(120, 80, 80, 255));
#else
      pContext->setFillColor(CColor(0, 0, 0, 255));
#endif
      pContext->drawRect(infobg, CDrawStyle::kDrawFilled);
      pContext->drawRect(skininfobg, CDrawStyle::kDrawFilled);

      int strHeight = infoFont->getSize(); // There should really be a better API for this in VSTGUI
      std::string bitness = (sizeof(size_t)==4? std::string("32") : std::string("64")) + "-bit";

      std::string chipmanu = Surge::Build::BuildArch;
      
#if TARGET_AUDIOUNIT      
      std::string flavor = "AU";
#elif TARGET_VST3
      std::string flavor = "VST3";
#elif TARGET_VST2
      std::string flavor = "VST2 (unsupported)";
#elif TARGET_LV2
      std::string flavor = "LV2 (experimental)";
#else
      std::string flavor = "Non-Plugin"; // for linux app
#endif      

#if MAC
      std::string platform = "macOS";
#elif WINDOWS
      std::string platform = "Windows";
#elif LINUX
      std::string platform = "Linux";
#else
      std::string platform = "Orac, Skynet or something";
#endif      

      {
         std::vector< std::string > msgs = { {
               std::string() + "Version " + Surge::Build::FullVersionStr + " (" + chipmanu + " " + bitness + " " + platform + " " + flavor
#if TARGET_VST2 || TARGET_VST3               
               + " in " + host
#endif
#if BUILD_IS_DEBUG
                 + " - DEBUG BUILD"
#endif
               + ". Built " +
               Surge::Build::BuildDate + " at " + Surge::Build::BuildTime + " on " + Surge::Build::BuildLocation + " host '" + Surge::Build::BuildHost + "')",
               "Factory Data Path: " + dataPath,
               "User Data Path: " + userPath,
               "Released under the GNU General Public License, v3",
               "Copyright 2005-2020 by Vember Audio and individual contributors",
               "Source, contributors and other information at",
               "VST plugin technology by Steinberg Media Technologies GmbH, AU plugin technology by Apple Inc.",
               "Airwindows open source effects by Chris Johnson, licensed under MIT license",
            } };
         identifierLine = msgs[0];
         int yMargin = 6;
         int yPos = toDisplay.getHeight() - msgs.size() * (strHeight + yMargin); // one for the last; one for the margin
         int iyPos = yPos;
         int xPos = strHeight;
         pContext->setFontColor(skin->getColor(Colors::AboutBox::Text));
         pContext->setFont(infoFont);
         for (auto s : msgs)
         {
            pContext->drawString(s.c_str(), CPoint( xPos, yPos ));
            yPos += strHeight + yMargin;
         }

         // link to Surge github repo in another color because VSTGUI -_-
         pContext->setFontColor(skin->getColor(Colors::AboutBox::Link));
         pContext->drawString( "Copy", CPoint( toDisplay.getWidth() - 50, iyPos ) );
         copyBox = CRect( CPoint( toDisplay.getWidth() - 50, iyPos - strHeight  ), CPoint( 48, 2 * strHeight ) );
         std::cout << _DUMPR( copyBox ) << std::endl;
         pContext->drawString("https://github.com/surge-synthesizer/surge",
                              CPoint(253, skin->getWindowSizeY() - 36 - strHeight - yMargin));
      }
      {
         std::vector< std::string > msgs;
         msgs.push_back( std::string( ) + "Current Skin: " + skin->displayName );
         msgs.push_back( std::string( ) + "Skin Author: " + skin->author + " " + skin->authorURL );
         std::string skin_path = "Skin Root Folder: " + skin->resourceName("");
         skin_path.pop_back();
         msgs.push_back(skin_path);
         
         int yMargin = 6;
         int yPos = strHeight * 2;
         int xPos = strHeight;
         pContext->setFontColor(skin->getColor(Colors::AboutBox::Text));
         pContext->setFont(infoFont);
         for (auto s : msgs)
         {
            pContext->drawString(s.c_str(), CPoint( xPos, yPos ));
            yPos += strHeight + yMargin;
         }
      }      
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

void CAboutBox::boxShow(std::string dataPath, std::string userPath, std::string host)
{
   this->dataPath = dataPath;
   this->userPath = userPath;
   this->host = host;
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
      return kMouseEventHandled;

   if (where.x >= 252 && where.x <= 480 && (where.y >= skin->getWindowSizeY() - 66) &&
             where.y <= (skin->getWindowSizeY() - 50))
      Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge");
   else if( copyBox.pointInside(where) ){
     #if WINDOWS
         identifierLine = identifierLine + " ";
     #endif
         auto a = CDropSource::create(identifierLine.c_str(), identifierLine.size(), IDataPackage::kText );
         getFrame()->setClipboard(a);
   }
   else
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
