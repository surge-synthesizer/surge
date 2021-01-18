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
#include "CTextButtonWithHover.h"


using namespace VSTGUI;


enum abouttags
{
   tag_copy = 70000
};


CAboutBox::CAboutBox(const CRect& size,
                     SurgeGUIEditor* editor,
                     SurgeStorage *storage,
                     const std::string &host,
                     Surge::UI::Skin::ptr_t skin,
                     std::shared_ptr<SurgeBitmaps> bitmapStore) : CViewContainer(size)
{
   this->editor = editor;
   this->storage = storage;

   setTransparency(true);
   
   auto vs = getViewSize();
   auto center = vs.getCenter();
   auto margin = 20;
   auto lblh = 16, lblvs = 15;
   auto boldFont = new VSTGUI::CFontDesc("Lato", 11, kBoldFace);
   
   /* dark semitransparent background */

   auto bg = new CTextLabel(CRect(CPoint(0, 0), CPoint(skin->getWindowSizeX(), skin->getWindowSizeY())), nullptr, nullptr);
   bg->setMouseableArea(CRect()); // Make sure I don't get clicked on
   bg->setBackColor(CColor(0, 0, 0, 212));
   addView(bg);

   /* big centered Surge logo */
   auto logo = bitmapStore->getBitmap(IDB_ABOUT_BG);
   auto logow = 555, logoh = 179;
   auto logor = CRect(CPoint(0, 0), CPoint(logow, logoh));
   logor.offset(center.x - (logow / 2), center.y - (logoh / 2));
   auto logol = new CTextLabel(logor, nullptr, logo);
   logol->setMouseableArea(CRect());
   addView(logol);

   auto xp = margin, yp = 20;

   /* text label construction lambdas */
   auto addLabel = [this, &xp, &yp, &lblh](std::string text, int width)
   {
      auto l1 = new CTextLabel(CRect(CPoint(xp, yp), CPoint(width, lblh)), text.c_str());
      l1->setFont(aboutFont);
      l1->setFontColor(kWhiteCColor);
      l1->setMouseableArea(CRect());
      l1->setTransparency(true);
      l1->setHoriAlign(kLeftText);
      addView(l1);
   };

   auto addTwoColumnLabel = [this, &xp, &yp, &lblh, &lblvs, &boldFont, skin](std::string title, std::string val, bool isURL,
                                                              std::string URL, int col1width, int col2width,
                                                              bool addToInfoString = false, bool subtractYoffset = false)
   {
      auto l1 = new CTextLabel(CRect(CPoint(xp, yp), CPoint(col1width, lblh)), title.c_str());
      l1->setFont(boldFont);
      l1->setFontColor(CColor(255, 144, 0));
      l1->setMouseableArea(CRect());
      l1->setTransparency(true);
      l1->setHoriAlign(kLeftText);
      addView(l1);

      if (isURL)
      {
         auto l2 = new CSurgeHyperlink(CRect(CPoint(xp + col1width, yp), CPoint(col2width, lblh)));
         l2->setFont(aboutFont);
         l2->setURL(URL);
         l2->setLabel(val.c_str());
         l2->setLabelColor(CColor(45, 134, 254));
         l2->setHoverColor(CColor(96, 196, 255));
         addView(l2);
      }
      else
      {
         auto l2 = new CTextLabel(CRect(CPoint(xp + col1width, yp), CPoint(col2width, lblh)), val.c_str());
         l2->setFont(aboutFont);
         l2->setFontColor(kWhiteCColor);
         l2->setTransparency(true);
         l2->setHoriAlign(kLeftText);
         l2->setMouseableArea(CRect());
         addView(l2);
      }

      if (addToInfoString)
      {
         this->infoStringForClipboard += title + "\t" + val + "\n";
      }

      if (subtractYoffset)
      {
         yp -= lblvs;
      }
      else
      {
         yp += lblvs;
      }
   };

   /* bottom left Surge folders info */

   // start from bottom margin up, this is why we subtract label vertical spacing (lblvs) 
   yp = vs.bottom - lblvs - margin;
   addTwoColumnLabel("Factory Data:", storage->datapath, true, storage->datapath, 76, 500, true, true);
   addTwoColumnLabel("User Data:", storage->userDataPath, true, storage->userDataPath, 76, 500, true, true);

   /* bottom left version info */

   std::string version = Surge::Build::FullVersionStr;
   std::string buildinfo = "Built on " + (std::string)Surge::Build::BuildDate + " at " +
                                         (std::string)Surge::Build::BuildTime + ", using " +
                                         (std::string)Surge::Build::BuildLocation + " host '" +
                                         (std::string)Surge::Build::BuildHost + "'";

#if TARGET_AUDIOUNIT
   std::string flavor = "AU";
#elif TARGET_VST3
   std::string flavor = "VST3";
#elif TARGET_VST2
   std::string flavor = "VST2 (unsupported)";
#elif TARGET_LV2
   std::string flavor = "LV2 (experimental)";
#else
   std::string flavor = "Standalone";
#endif

   std::string arch = Surge::Build::BuildArch;
#if MAC
   std::string platform = "macOS";
#if ARM_NEON
   arch = "Apple Silicon";
#endif
#elif WINDOWS
   std::string platform = "Windows";
#elif LINUX
   std::string platform = "Linux";
#else
   std::string platform = "GLaDOS, Orac or Skynet";
#endif

   std::string bitness = (sizeof(size_t) == 4 ? std::string("32") : std::string("64")) + "-bit";
   std::string system = arch + " CPU, " + platform + " " + flavor + ", " + bitness;;

   infoStringForClipboard = "Surge Synthesizer\n";

   yp -= lblvs;

#if TARGET_VST2 || TARGET_VST3
   addTwoColumnLabel("Plugin Host:", host, false, "", 76, 500, true, true);
#endif

   addTwoColumnLabel("System:", system, false, "", 76, 500, true, true);
   addTwoColumnLabel("Build Info:", buildinfo, false, "", 76, 500, true, true);
   addTwoColumnLabel("Version:", version, false, "", 76, 500, true, true);
   yp -= 2;

   auto copy = new CTextButtonWithHover(CRect(CPoint(xp - 1, yp), CPoint(100, 18)), this, tag_copy, "Copy Version Info");   // here's a temporary thing to trigger a copy. fix this widget
   copy->setFont(boldFont);
   copy->setFrameColor(CColor(0, 0, 0, 0));
   copy->setFrameColorHighlighted(CColor(0, 0, 0, 0));
   copy->setHoverTextColor(CColor(96, 196, 255));
   copy->setGradient(nullptr);
   copy->setGradientHighlighted(nullptr);
   copy->setTextColor(CColor(45, 134, 254));
   copy->setTextColorHighlighted(CColor(96, 196, 255));
   copy->setTextAlignment(kLeftText);
   addView(copy);

   /* top right various iconized links */

   auto iconsize = CPoint(36, 36);
   auto ranchor = vs.right - iconsize.x - margin;
   auto spacing = 56;

   yp = margin;

   // place these right to left respecting right anchor (ranchor) and icon spacing
   auto discord = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 0), yp), iconsize));
   discord->setURL("https://discord.gg/aFQDdMV");
   discord->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
   discord->setHorizOffset(CCoord(144));
   addView(discord);

   auto gplv3 = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 1), yp), iconsize));
   gplv3->setURL("https://www.gnu.org/licenses/gpl-3.0-standalone.html");
   gplv3->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
   gplv3->setHorizOffset(CCoord(108));
   addView(gplv3);

   auto au = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 2), yp), iconsize));
   au->setURL("https://developer.apple.com/documentation/audiounit");
   au->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
   au->setHorizOffset(CCoord(72));
   addView(au);

   auto vst3 = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 3), yp), iconsize));
   vst3->setURL("https://www.steinberg.net/en/company/technologies/vst3.html");
   vst3->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
   vst3->setHorizOffset(CCoord(36));
   addView(vst3);

   auto gh = new CSurgeHyperlink(CRect(CPoint(ranchor - (spacing * 4), yp), iconsize));
   gh->setURL("https://github.com/surge-synthesizer/surge/");
   gh->setBitmap(bitmapStore->getBitmap(IDB_ABOUT_LOGOS));
   gh->setHorizOffset(CCoord(0));
   addView(gh);

   /* bottom right skin info */

   xp = ranchor - (spacing * 4);
   yp = vs.bottom - lblvs - margin;

   addTwoColumnLabel("Current Skin:", skin->displayName, true, skin->root + skin->name, 76, 175, false, true);
   addTwoColumnLabel("Skin Author:", skin->author, (skin->authorURL != nullptr), skin->authorURL, 76, 175, false, true);

   /* top left copyright and credits */

   xp = margin;
   yp = margin;

   addLabel(std::string("Copyright 2005-") + Surge::Build::BuildYear + " by Vember Audio and individual contributors in the Surge Synth Team", 600);
   yp += lblvs;
   addLabel("Released under the GNU General Public License, v3", 600);
   yp += lblvs;
   addLabel("VST is a trademark of Steinberg Media Technologies GmbH", 600);
   yp += lblvs;
   addLabel("Audio Units is a trademark of Apple Inc.", 600);
   yp += lblvs;
   addLabel("Airwindows open source effects by Chris Johnson, licensed under MIT license", 600);
   yp += lblvs;
   addLabel("OB-Xd filters by Vadim Filatov, licensed under GNU GPL v3 license", 600);
   yp += lblvs;
   addLabel("K35 and Diode Ladder filters by Will Pirkle (implementation by TheWaveWarden), licensed under GNU GPL v3 license", 600);
   yp += lblvs;
   addLabel("Cutoff Warp and Resonance Warp filters by Jatin Chowdhury, licensed under GNU GPL v3 license", 600);
   yp += lblvs;
   addLabel("OJD waveshaper by Janos Buttgereit, licensed under GNU GPL v3 license", 600);
}

CMouseEventResult CAboutBox::onMouseDown(CPoint& where, const CButtonState& buttons)
{

   auto res = CViewContainer::onMouseDown(where, buttons);
   if( res == kMouseEventNotHandled && this->editor )
   {
      // OK I want that mouse up!
      return kMouseEventHandled;
   }
   return res;
}
CMouseEventResult CAboutBox::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   auto res = CViewContainer::onMouseUp(where, buttons);
   if( res == kMouseEventNotHandled && this->editor )
   {
      this->editor->hideAboutBox();
      return kMouseEventHandled;
   }
   return res;
}

void CAboutBox::valueChanged(CControl* pControl)
{
   if( pControl->getTag() == tag_copy )
   {
      std::string identifierLine = infoStringForClipboard;  // don't forget the space at the end
#if LINUX
      auto xc = popen( "xclip -selection c", "w" );
      if( ! xc )
      {
         std::cout << "Unable to open xclip for writing to clipboard. About is:\n" << identifierLine << std::endl;
         return;
      }
      fprintf( xc, "%s", identifierLine.c_str() );
      pclose( xc );
#else
      auto a = CDropSource::create(identifierLine.c_str(), identifierLine.size(), IDataPackage::kText );
      getFrame()->setClipboard(a);
#endif
   }
}
