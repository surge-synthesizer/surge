#include "CAboutBox.h"
#include "globals.h"
#include "resource.h"
#include "RuntimeFont.h"
#include <stdio.h>
#include "version.h"

#include "UserInteractions.h"
#include "SkinColors.h"
#include "CSurgeHyperlink.h"
#include "SurgeGUIEditor.h"
#include "CScalableBitmap.h"


using namespace VSTGUI;


#if 0

// Here's the old draw code just for reference

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


#endif

enum abouttags
{
   tag_copy = 70000
};
CAboutBox::CAboutBox(const VSTGUI::CRect& size, SurgeGUIEditor* editor, Surge::UI::Skin::ptr_t skin,
                     std::shared_ptr<SurgeBitmaps> bitmapStore) : VSTGUI::CViewContainer(size)
{
   setTransparency(true);
   this->editor = editor;

   /*
    * So lay out the container
    */
   auto logo = bitmapStore->getBitmap(IDB_ABOUT);
   auto logol = new VSTGUI::CTextLabel( VSTGUI::CRect( 0, 0, 904, 542 ), nullptr, logo );
   logol->setMouseableArea(VSTGUI::CRect(0,0,0,0)); // Make sure I don't get clicked on
   addView( logol );

   CRect infobg(0, skin->getWindowSizeY() - 167, skin->getWindowSizeX(), skin->getWindowSizeY());
   CRect skininfobg(0, 0, BASE_WINDOW_SIZE_X, 80);
   auto toph = new CTextLabel( infobg );
   toph->setTransparency(false);
   toph->setBackColor(CColor(0, 0, 0, 255));
   toph->setMouseableArea(CRect());
   addView(toph);
   toph = new CTextLabel( skininfobg );
   toph->setTransparency(false);
   toph->setBackColor(CColor(0, 0, 0, 255));
   toph->setMouseableArea(CRect());
   addView(toph);

   // Here's a temporary thing o trigger a copy. Fix this widget
   auto copy = new CTextButton(CRect( CPoint( 10, 400 ), CPoint( 200, 18 )), this, tag_copy, "Copy Version Info" );
   copy->setFont( aboutFont );
   addView( copy );

   // Do some basic version info
   float yp = 420;
   auto lpp = [this, &yp](std::string title, std::string val ) {
      float labelW = 80;
      auto l1 = new VSTGUI::CTextLabel( VSTGUI::CRect( VSTGUI::CPoint( 10, yp ), VSTGUI::CPoint( labelW, 18 )), title.c_str() );
      l1->setFont( aboutFont );
      l1->setFontColor( VSTGUI::CColor( 0xFF, 0x90, 0x00 ) );
      l1->setMouseableArea(VSTGUI::CRect());
      l1->setTransparency(true);
      l1->setHoriAlign(VSTGUI::kLeftText );
      addView(l1);

      auto l2 = new VSTGUI::CTextLabel( VSTGUI::CRect( VSTGUI::CPoint( labelW + 12, yp ), VSTGUI::CPoint( 200, 18 )), val.c_str() );
      l2->setFont( aboutFont );
      l2->setFontColor( VSTGUI::kWhiteCColor );
      l2->setTransparency(true);
      l2->setHoriAlign(VSTGUI::kLeftText );
      l2->setMouseableArea(VSTGUI::CRect());
      addView(l2);

      yp += 20;
   };
   lpp( "Version", Surge::Build::FullVersionStr );

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
   lpp( "Architecture", chipmanu + " " + platform + " " + flavor );

   // Stick the github link in the top
   auto ghub = new CSurgeHyperlink(CRect( CPoint( 800, 20 ), CPoint( 24, 24 )));
   ghub->setURL( "https://github.com/surge-synthesizer/surge/");
   ghub->setBitmap(bitmapStore->getBitmap(IDB_GITHUB_LOGO));
   // Critically, leave the mousable area intact here
   addView(ghub);

   auto regularLink = new CSurgeHyperlink( CRect( CPoint( 10, 20 ), CPoint( 70, 20 ) ) );
   regularLink->setURL( "https://google.com" );
   regularLink->setLabel( "Google Stuff" );
   regularLink->setLabelColor( VSTGUI::CColor( 255, 220, 220 ) );
   regularLink->setHoverColor( VSTGUI::CColor( 255, 250, 250 ));
   regularLink->setFont( aboutFont );
   addView( regularLink );
}

VSTGUI::CMouseEventResult CAboutBox::onMouseDown(VSTGUI::CPoint& where,
                                                 const VSTGUI::CButtonState& buttons)
{

   auto res = CViewContainer::onMouseDown(where, buttons);
   std::cout << "onMouseDown " << res << std::endl;
   if( res == VSTGUI::kMouseEventNotHandled && this->editor )
   {
      // OK I want that mouse up!
      return VSTGUI::kMouseEventHandled;
   }
   return res;
}
VSTGUI::CMouseEventResult CAboutBox::onMouseUp(VSTGUI::CPoint& where,
                                               const VSTGUI::CButtonState& buttons)
{
   auto res = CViewContainer::onMouseUp(where, buttons);
   if( res == VSTGUI::kMouseEventNotHandled && this->editor )
   {
      this->editor->hideAboutBox();
      return VSTGUI::kMouseEventHandled;
   }
   return res;
}

void CAboutBox::valueChanged(CControl* pControl)
{
   std::cout << pControl->getTag() << std::endl;
   if( pControl->getTag() == tag_copy )
   {
      std::string identifierLine = "This is what we copy "; // don't forget the space at the end
      auto a = CDropSource::create(identifierLine.c_str(), identifierLine.size(), IDataPackage::kText );
      getFrame()->setClipboard(a);
   }
}
