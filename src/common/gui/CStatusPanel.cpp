#include "SurgeGUIEditor.h"
#include "CStatusPanel.h"
#include "CScalableBitmap.h"
#include "RuntimeFont.h"


using namespace VSTGUI;

void CStatusPanel::draw( VSTGUI::CDrawContext *dc )
{
   auto size = getViewSize();

   auto statusButtonGlyph = bitmapStore->getBitmapByStringID( "STATUS_BUTTON" );

   std::string labs[numDisplayFeatures];
   labs[mpeMode] = "MPE";
   labs[tuningMode] = "Tune";
   labs[zoomOptions] = "Zoom";

   std::string bmps[numDisplayFeatures];
   bmps[mpeMode] = "STATUS_MPE";
   bmps[tuningMode] = "STATUS_TUNING";
   bmps[zoomOptions] = "STATUS_ZOOM";

   float y0 = 11;
   float boxSize = 13;

   for( int i=0; i<numDisplayFeatures; ++i )
   {
       float xp = size.left + 2;
       float yp = size.top + y0 + i * boxSize + (2 * i);
       float w = size.getWidth() - 4;;
       float h = boxSize - 2;
       if ( i == mpeMode )
           mpeBox = CRect(xp, yp, xp + w, yp + h);
       if ( i == tuningMode )
           tuningBox = CRect(xp, yp, xp + w, yp + h);
       if (i == zoomOptions)
           zoomBox = CRect(xp, yp, xp + w, yp + h);

       auto hlbg = true;
       auto ol = skin->getColor( "mpetunstatus.button.outline", CColor(0x97, 0x97, 0x97 ) );
       auto bg = skin->getColor( "mpetunstatus.button.background", CColor(0xe3, 0xe3, 0xe3 ) );
       auto fg = skin->getColor( "mpetunstatus.button.selected.foreground", kBlackCColor );
       auto ufg = skin->getColor( "mpetunstatus.button.unselected.foreground", kBlackCColor );
       auto hl = skin->getColor( "mpetunstatus.buttun.highlight", CColor(0xff, 0x9A, 0x10 ) );

       if (!dispfeatures[i])
       {
           hlbg = false;
       }

       auto btn = bitmapStore->getBitmapByStringID(bmps[i]);

       if (btn)
       {
          CRect wr = CRect(xp, yp, xp + w, yp + h);
          btn->draw(dc, wr, CPoint(0, h * (hlbg ? 1 : 0)), 0xff);
       }
       else
       {
          if (statusButtonGlyph != nullptr)
          {
             CRect wr = CRect(xp, yp, xp + w, yp + h);
             statusButtonGlyph->draw( dc, wr, CPoint( 0, h * ( hlbg ? 1 : 0 ) ), 0xff );
          }
          else
          {
             dc->setDrawMode(VSTGUI::kAntiAliasing);
             dc->setFrameColor(bg);;
             auto p = dc->createRoundRectGraphicsPath(CRect(xp, yp, xp + w, yp + h), 5.f );
             dc->setFillColor(bg);;
             dc->drawGraphicsPath(p, CDrawContext::kPathFilled);
             dc->setFrameColor(ol);
             dc->drawGraphicsPath(p, CDrawContext::kPathStroked);
             p->forget();
          
             if ( hlbg )
             {
                auto p = dc->createRoundRectGraphicsPath(CRect(xp + 2, yp + 2, xp + w - 2, yp + h - 2), 3 );
                dc->setFillColor(hl);
                dc->drawGraphicsPath(p, CDrawContext::kPathFilled);
                p->forget();
             }
          }

          if (hlbg)
          {
             dc->setFontColor(fg);
          }
          else
          {
             dc->setFontColor(ufg);
          }
        
          #if WINDOWS || LINUX
             dc->setFont(displayFont, 8, 0);
          #else
             dc->setFont(displayFont, 8, 2);
          #endif       
       
          auto sw = dc->getStringWidth(labs[i].c_str());
          dc->drawString(labs[i].c_str(), CPoint( xp + w/2.0 - sw/2.0, yp + h - 3.0 ), true );
       }
   }
}

VSTGUI::CMouseEventResult CStatusPanel::onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button)
{
    if ( mpeBox.pointInside(where) && editor )
    {
        if ( button & kLButton )
        {
             editor->toggleMPE();
        }
        else if ( button & kRButton )
        {
             editor->showMPEMenu(where);
        }
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if ( tuningBox.pointInside(where) && editor )
    {
        if ( button & kLButton )
        {
            if (!storage->isStandardTuning  || editor->tuningCacheForToggle.size() > 0 ||
                !storage->isStandardMapping || editor->mappingCacheForToggle.size() > 0)
                editor->toggleTuning();
            else
                editor->showTuningMenu(where);
        }
        else if ( button & kRButton )
        {
            editor->showTuningMenu(where);
        }
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if ( zoomBox.pointInside(where) && editor )
    {
        if ( button & kLButton || button & kRButton )
        {
            editor->showZoomMenu(where);
        }

        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    return CControl::onMouseDown(where, button);
}

VSTGUI::CMouseEventResult CStatusPanel::onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button) {
   /*
   ** Restore once I have a hover strategy for this widget

   if ( mpeBox.pointInside(where) || tuningBox.pointInside(where) || zoomBox.pointInside(where) )
     getFrame()->setCursor( VSTGUI::kCursorHand );
   else
     getFrame()->setCursor( VSTGUI::kCursorDefault );
   */
   
   return kMouseEventHandled;
}

bool CStatusPanel::onDrop(VSTGUI::DragEventData data )
{
   doingDrag = false;

   auto drag = data.drag;
   auto where = data.pos;
   uint32_t ct = drag->getCount();
   if (ct == 1)
   {
      IDataPackage::Type t = drag->getDataType(0);
      if (t == IDataPackage::kFilePath)
      {
         const void* fn;
         drag->getData(0, fn, t);
         const char* fName = static_cast<const char*>(fn);
         fs::path fPath(fName);
         if ((_stricmp(fPath.extension().generic_string().c_str(), ".scl") == 0))
         {
            if ( editor )
               editor->tuningFileDropped(fName);
         }
         if ((_stricmp(fPath.extension().generic_string().c_str(), ".kbm") == 0))
         {
            if ( editor )
               editor->mappingFileDropped(fName);
         }
      }
   }

   return true;
}

