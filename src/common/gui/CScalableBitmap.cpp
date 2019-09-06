#include "CScalableBitmap.h"
#include "SurgeError.h"
#include "UserInteractions.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#if MAC
#include <CoreFoundation/CoreFoundation.h>
#include "vstgui/lib/platform/mac/macglobals.h"
#include <strstream>
#endif
#if LINUX
#include "ScalablePiggy.h"
#endif
#if WINDOWS
#include "vstgui/lib/platform/iplatformresourceinputstream.h"
#endif

#include <cmath>

#include "resource.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
/*
** CScalableBitmap is the only file which uses the SVG Implementatoin so it is also the one
** and only one which ejexts the link symbols.
*/
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

using namespace VSTGUI;

#if MAC
static const std::string svgFullFileNameFromBundle(const std::string& filename)
{
   CFStringRef cfStr = CFStringCreateWithCString(nullptr, filename.c_str(), kCFStringEncodingUTF8);
   CFURLRef url = CFBundleCopyResourceURL(VSTGUI::getBundleRef(), cfStr, nullptr, nullptr);
   CFRelease(cfStr);
   if (url)
   {
      CFStringRef urlStr = CFURLGetString(url);
      // std::cout << "URLString = " << urlStr << std::flush << std::endl;
      const char* csp = CFStringGetCStringPtr(urlStr, kCFStringEncodingUTF8);
      if (csp)
      {
         std::string resPath(CFStringGetCStringPtr(CFURLGetString(url), kCFStringEncodingUTF8));
         if (resPath.find("file://") != std::string::npos)
            resPath = resPath.substr(7);
         return resPath;
      }
   }

   return "";
}

#endif

#if LINUX
static const struct MemorySVG* findMemorySVG(const std::string& filename)
{
   for (const struct MemorySVG* svg = &memorySVGList[0]; svg->name != NULL; svg++)
      if (!std::strncmp(filename.c_str(), svg->name, std::strlen(svg->name)))
         return svg;

   throw Surge::Error(filename + " not found");
}
#endif

#if WINDOWS
static int svgContentsFromRCFile(int id, char* svgData, int maxSz)
{
   VSTGUI::IPlatformResourceInputStream::Ptr istr =
       VSTGUI::IPlatformResourceInputStream::create(CResourceDescription(id));

   if (istr == NULL)
   {
      return -1;
   }
   memset(svgData, 0, maxSz);
   uint32_t readSize = istr->readRaw(svgData, maxSz);
   svgData[readSize] = 0;
   return readSize;
}
#endif

// Remember this is user zoom * display zoom. See comment in CScalableBitmap.h
void CScalableBitmap::setPhysicalZoomFactor(int zoomFactor)
{
    currentPhysicalZoomFactor = zoomFactor;
}


CScalableBitmap::CScalableBitmap(CResourceDescription desc, VSTGUI::CFrame* f)
    : CBitmap(desc), svgImage(nullptr), frame(f)
{
    int id = 0;
    if(desc.type == CResourceDescription::kIntegerType)
        id = (int32_t)desc.u.id;

    resourceID = id;

    std::stringstream filename;
    filename << "svg/bmp" << std::setw(5) << std::setfill('0') << id << ".svg";

#if MAC
    std::string fullFileName = svgFullFileNameFromBundle(filename.str());
    svgImage = nsvgParseFromFile(fullFileName.c_str(), "px", 96);

    /*
    ** Some older versions of live are reported to not set the bundle identity
    ** properly. There are all sorts of complicated ways to deal with this. Here 
    ** though is one which isn't great but gives those users a bit of hope
    */
    if (!svgImage)
    {
        std::ostringstream fn2;
#if TARGET_AUDIOUNIT
        fn2 << "/Library/Audio/Plug-Ins/Components/Surge.component/Contents/Resources/" << filename.str();
#elif TARGET_VST2
        fn2 << "/Library/Audio/Plug-Ins/VST/Surge.vst/Contents/Resources/" << filename.str();
#elif TARGET_VST3
        fn2 << "/Library/Audio/Plug-Ins/VST3/Surge.vst3/Contents/Resources/" << filename.str();
#endif
        
        // std::cout << "FailBack from bad module SVG path to best guess: [" << fn2.str() << "]" << std::endl;
        svgImage = nsvgParseFromFile(fn2.str().c_str(), "px", 96);
    }
#endif

#if WINDOWS
    int svgid = id + SCALABLE_SVG_OFFSET;
    char* svgData = new char[1024 * 1024];

    if (svgContentsFromRCFile(svgid, svgData, 1024 * 1024) > 0)
       svgImage = nsvgParse(svgData, "px", 96);
    else
       svgImage = NULL;

    delete[] svgData;
#endif

#if LINUX
    try
    {
       const MemorySVG* memSVG = findMemorySVG(filename.str());
       char* svg = new char[memSVG->size + 1];
       svg[memSVG->size] = '\0';
       strncpy(svg, (const char*)(memorySVGListStart + memSVG->offset), memSVG->size);
       svgImage = nsvgParse(svg, "px", 96);
       delete[] svg;
    }
    catch (Surge::Error err)
    {
       std::cerr << err.getMessage() << std::endl;
    }
#endif

    if (!svgImage)
    {
       std::cout << "Unable to load SVG Image " << filename.str() << std::endl;
    }

    extraScaleFactor = 100;
    currentPhysicalZoomFactor = 100;
    lastSeenZoom = -1;
}

CScalableBitmap::CScalableBitmap(std::string svgContents, VSTGUI::CFrame* f)
    : CBitmap(VSTGUI::CResourceDescription(0)), svgImage(nullptr), frame(f)
{
   svgImage = nsvgParse((char*)(svgContents.c_str()), "px", 96);

   
   if (!svgImage)
   {
      std::cout << "Unable to load SVG Image from string contents" << std::endl;
   }

   extraScaleFactor = 100;
   currentPhysicalZoomFactor = 100;
   lastSeenZoom = -1;
}

#define DUMPR(r)                                                                                   \
   "(x=" << r.getTopLeft().x << ",y=" << r.getTopLeft().y << ")+(w=" << r.getWidth()               \
         << ",h=" << r.getHeight() << ")"

void CScalableBitmap::draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha )
{
    /*
    ** CViewContainer, in the ::drawRect method, no matter what invalidates, calls a drawBackground
    ** on the entire background with a clip rectangle applied. This is not normally a problem when
    ** you invalidate infrequently or just have a constant color background. But in Surge we have
    ** a drawn background on our frame and we invalidate part of our UI every frame because of the
    ** vu meter. So every cycle we redraw our entire background into a vu meter sized clip rectangle
    ** which is immediately overwritten by a vu meter which fills its entire space.
    **
    ** There's no good way out of this. We can't subclass CFrame since it is final (yuch).
    ** We cant have the vu meter not invalidate. So we make the bitmap smart.
    **
    ** Well, smart. We make it so that if we are redrawing a rectangle which is positioned relative
    ** to the background at the same point as the VU Meter. That is, a draw at 763+14 on a background
    ** of size 904x542 (ratioed for scaling) gets background supressed. Also turns out you need to
    ** make sure the size is OK since the entire effect panel draws BG starting at the same point
    ** just with bigger clip, causing #716.
    **
    ** Am I particularly proud of this? No. But it does supress all those draws.
    */
    VSTGUI::CRect cl;
    context->getClipRect(cl);
    float p1 = cl.getTopLeft().x / rect.getWidth();
    float p2 = cl.getTopLeft().y / rect.getHeight();
    float d1 = cl.getWidth() / rect.getWidth();
    float d2 = cl.getHeight() / rect.getHeight();
    if (fabs(p1 - 763.0 / 904.0) < 0.01 && fabs(p2 - 14.0 / 542.0) < 0.01 && // this is the x-y position
        fabs(d1 - 123.0 / 904.0) < 0.01 && fabs(d2 - 13.0 / 542.0) < 0.01) // this makes sure I am just in the vu-only clip rect 
    {
       return;
    }

    if (svgImage)
    {
       /*
       ** Plan of attack here is to use coffscreencontext into a physicalzoomfactor scaled version
       *of rect
       ** see vstgui.surge/vstgui/lib/coffscreencontext.h for how that works. So we just need a good
       *hash
       ** on rect.size, off and physical zoom.
       */
       if (lastSeenZoom != currentPhysicalZoomFactor)
       {
          for (auto const& pair : offscreenCache)
          {
             auto val = pair.second;
             if (val)
                val->forget();
          }
          offscreenCache.clear();
          lastSeenZoom = currentPhysicalZoomFactor;
       }

       CGraphicsTransform tf = CGraphicsTransform().scale(lastSeenZoom / 100.0, lastSeenZoom / 100.0);
       
       /*
       ** VSTGUI has this wierdo bug where it shrinks backgrounds properly but doesn't grow them. Sigh.
       ** So asymmetrically treat the extra factor here and only here.
       */
       int exs = (extraScaleFactor<100?100:extraScaleFactor);
       CGraphicsTransform xtf =
           CGraphicsTransform().scale( exs / 100.0, exs / 100.0);
       CGraphicsTransform itf = tf.inverse();
       CGraphicsTransform ixtf = xtf.inverse();


       if (offscreenCache.find(offset) == offscreenCache.end())
       {
          VSTGUI::CPoint sz = rect.getSize();
          ixtf.transform(sz);

          VSTGUI::CPoint offScreenSz = sz;
          tf.transform(offScreenSz);

          if (auto offscreen = COffscreenContext::create(frame, ceil(offScreenSz.x), ceil(offScreenSz.y)))
          {
             offscreen->beginDraw();
             VSTGUI::CRect newRect(0, 0, rect.getWidth(), rect.getHeight());

             CDrawContext::Transform trsf(*offscreen, tf);
             CDrawContext::Transform xtrsf(*offscreen, ixtf);

             auto inherent = CGraphicsTransform().scale( inherentScaleFactor, inherentScaleFactor );
             inherent.inverse().transform(newRect);
             CDrawContext::Transform isf( *offscreen, inherent );
             
             drawSVG(offscreen, newRect, offset, alpha);

             offscreen->endDraw();
             CBitmap* tmp = offscreen->getBitmap();
             if (tmp)
             {
                offscreenCache[offset] = tmp;
                tmp->remember();
             }
          }
       }

       if (offscreenCache.find(offset) != offscreenCache.end())
       {
          context->saveGlobalState();

          CDrawContext::Transform trsf(*context, itf);
          CDrawContext::Transform trsf2(*context, xtf);

          VSTGUI::CRect drect = rect;
          tf.transform(drect);

          VSTGUI::CRect origClip;
          context->getClipRect(origClip);

          VSTGUI::CRect clipR = origClip;

          /*
          ** The clipR goes up to and including the edge of the size; but at very high zooms that
          *shows a
          ** tiny bit of the adjacent image. So make the edge of our clip just inside the size.
          ** This only matters at high zoom levels
          */
          clipR.bottom -= 0.01;
          clipR.right -= 0.01;
          context->setClipRect(clipR);

#if LINUX
          alpha = 1.0;
#endif
          offscreenCache[offset]->draw(context, drect, CPoint(0, 0), alpha);
          context->setClipRect(origClip);

          context->restoreGlobalState();
          return;
       }
    }

    /* SVG File is missing */
    context->setFillColor(VSTGUI::kBlueCColor);
    context->drawRect(rect, VSTGUI::kDrawFilled);
    context->setFrameColor(VSTGUI::kRedCColor);
    context->drawRect(rect, VSTGUI::kDrawStroked);
    return;
}

void CScalableBitmap::drawSVG(CDrawContext* dc,
                              const CRect& rect,
                              const CPoint& offset,
                              float alpha)
{
   VSTGUI::CRect viewS = rect;

   VSTGUI::CDrawMode origMode = dc->getDrawMode();
   VSTGUI::CDrawMode newMode(VSTGUI::kAntiAliasing);
   dc->setDrawMode(newMode);

   VSTGUI::CRect origClip;
   dc->getClipRect(origClip);
   VSTGUI::CRect clipR = viewS;
   /*
   ** The clipR goes up to and including the edge of the size; but at very high zooms that shows a
   ** tiny bit of the adjacent image. So make the edge of our clip just inside the size.
   ** This only matters at high zoom levels
   */
   clipR.bottom -= 0.01;
   clipR.right -= 0.01;
   dc->setClipRect(clipR);

   /*
   ** Our overall transform moves us to the x/y position of our view rects top left point and shifts
   *us by
   ** our offset. Don't forgert the extra zoom also!
   */

   VSTGUI::CGraphicsTransform tf =
       VSTGUI::CGraphicsTransform()
           .translate(viewS.getTopLeft().x - offset.x, viewS.getTopLeft().y - offset.y)
           .scale(extraScaleFactor / 100.0, extraScaleFactor / 100.0);
   VSTGUI::CDrawContext::Transform t(*dc, tf);

   for (auto shape = svgImage->shapes; shape != NULL; shape = shape->next)
   {
      if (!(shape->flags & NSVG_FLAGS_VISIBLE))
         continue;

      /*
      ** We don't need to even bother drawing shapes outside of clip
      */
      CRect shapeBounds(shape->bounds[0], shape->bounds[1], shape->bounds[2], shape->bounds[3]);
      tf.transform(shapeBounds);
      if (!shapeBounds.rectOverlap(clipR))
         continue;

      /*
      ** Build a path for this shape out of each subordinate path as a
      ** graphics subpath
      */
      VSTGUI::CGraphicsPath* gp = dc->createGraphicsPath();
      for (auto path = shape->paths; path != NULL; path = path->next)
      {
         for (auto i = 0; i < path->npts - 1; i += 3)
         {
            float* p = &path->pts[i * 2];

            if (i == 0)
               gp->beginSubpath(p[0], p[1]);
            gp->addBezierCurve(p[2], p[3], p[4], p[5], p[6], p[7]);
         }
         if (path->closed)
            gp->closeSubpath();
      }

      /*
      ** Fill with constant or gradient
      */
      if (shape->fill.type != NSVG_PAINT_NONE)
      {
         if (shape->fill.type == NSVG_PAINT_COLOR)
         {
            dc->setFillColor(svgColorToCColor(shape->fill.color, shape->opacity));

            VSTGUI::CDrawContext::PathDrawMode pdm = VSTGUI::CDrawContext::kPathFilled;
            if (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD)
            {
               pdm = VSTGUI::CDrawContext::kPathFilledEvenOdd;
            }
            dc->drawGraphicsPath(gp, pdm);
         }
         else if (shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT)
         {
            bool evenOdd = (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD);
            NSVGgradient* ngrad = shape->fill.gradient;

            float* x = ngrad->xform;
            VSTGUI::CGraphicsTransform gradXform(x[0], x[1], x[2], x[3], x[4], x[5]);
            VSTGUI::CGradient::ColorStopMap csm;
            VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);

            for (int i = 0; i < ngrad->nstops; ++i)
            {
               auto stop = ngrad->stops[i];
               cg->addColorStop(stop.offset, svgColorToCColor(stop.color));
            }
            VSTGUI::CPoint s0(0, 0), s1(0, 1);
            VSTGUI::CPoint p0 = gradXform.inverse().transform(s0);
            VSTGUI::CPoint p1 = gradXform.inverse().transform(s1);

            dc->fillLinearGradient(gp, *cg, p0, p1, evenOdd);
         }
         else
         {
            std::cerr << "No radial gradient support yet" << std::endl;
            dc->setFillColor(VSTGUI::kRedCColor);
            dc->drawGraphicsPath(gp, VSTGUI::CDrawContext::kPathFilled);
         }
      }

      /*
      ** And then the stroke
      */
      if (shape->stroke.type != NSVG_PAINT_NONE)
      {
         if (shape->stroke.type == NSVG_PAINT_COLOR)
         {
            dc->setFrameColor(svgColorToCColor(shape->stroke.color));
         }
         else
         {
            std::cerr << "No gradient support yet for stroke" << std::endl;
            dc->setFrameColor(VSTGUI::kRedCColor);
         }

         dc->setLineWidth(shape->strokeWidth);
         VSTGUI::CLineStyle cs((VSTGUI::CLineStyle::LineCap)(shape->strokeLineCap),
                               (VSTGUI::CLineStyle::LineJoin)(shape->strokeLineJoin));
         dc->setLineStyle(cs);
         dc->drawGraphicsPath(gp, VSTGUI::CDrawContext::kPathStroked);
      }
      gp->forget();
   }

   dc->resetClipRect();
   dc->setDrawMode(origMode);
}

VSTGUI::CColor CScalableBitmap::svgColorToCColor(int svgColor, float opacity)
{
   int a = ((svgColor & 0xFF000000) >> 24) * opacity;
   int b = (svgColor & 0x00FF0000) >> 16;
   int g = (svgColor & 0x0000FF00) >> 8;
   int r = (svgColor & 0x000000FF);

   if( glyphMode && r == 0 && g == 0 && b == 0 )
   {
      return glyphColor;
   }
   return VSTGUI::CColor(r, g, b, a);
}

void CScalableBitmap::setInherentScaleForSize(float w, float h)
{
   if( svgImage )
   {
      float impz = std::min( w / svgImage->width, h / svgImage->height );
      inherentScaleFactor = impz;
      lastSeenZoom = -1;
   }
}

void CScalableBitmap::updateWithGlyphColor( VSTGUI::CColor c )
{
   if( c != glyphColor )
   {
      glyphColor = c;

      for (auto const& pair : offscreenCache)
      {
         auto val = pair.second;
         if (val)
            val->forget();
      }
      offscreenCache.clear();
   }
}
