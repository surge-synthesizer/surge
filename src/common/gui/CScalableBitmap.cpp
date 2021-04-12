#if ESCAPE_FROM_VSTGUI
#include <JuceHeader.h>
#include "CScalableBitmap.h"
#include "DisplayInfo.h"

CScalableBitmap::CScalableBitmap(VSTGUI::CResourceDescription d, VSTGUI::CFrame *f)
    : VSTGUI::CBitmap(d)
{
    if (d.u.type == VSTGUI::CResourceDescription::kIntegerType)
    {
        resourceID = d.u.id;
        std::string fn = "bmp00" + std::to_string(d.u.id) + "_svg";
        int bds;
        auto bd = BinaryData::getNamedResource(fn.c_str(), bds);
        if (bd)
        {
            drawable = juce::Drawable::createFromImageData(bd, bds);
        }
    }
}

CScalableBitmap::CScalableBitmap(std::string fname, VSTGUI::CFrame *f)
    : VSTGUI::CBitmap(VSTGUI::CResourceDescription(0))
{
    this->fname = fname;
    drawable = juce::Drawable::createFromImageFile(juce::File(fname));
}

CScalableBitmap::~CScalableBitmap() = default;

void CScalableBitmap::setPhysicalZoomFactor(int zoomFactor)
{
    currentPhysicalZoomFactor = zoomFactor;
}

void CScalableBitmap::draw(VSTGUI::CDrawContext *context, const VSTGUI::CRect &rect,
                           const VSTGUI::CPoint &offset, float alpha)
{
    if (pngZooms.size() == 0)
    {
        VSTGUI::CBitmap::draw(context, rect, offset, alpha);
        return;
    }
    int useZoom = -1;
    for (auto &p : pngZooms)
    {
        if (p.first <= currentPhysicalZoomFactor)
            useZoom = p.first;
    }

    if (useZoom < 0)
    {
        VSTGUI::CBitmap::draw(context, rect, offset, alpha);
        return;
    }

    resolvePNGForZoomLevel(useZoom);
    if (!pngZooms[useZoom].second)
    {
        VSTGUI::CBitmap::draw(context, rect, offset, alpha);
        return;
    }

    // This relies on the internal state a bit but that's OK
    auto hcdf = Surge::GUI::getDisplayBackingScaleFactor(nullptr);
    juce::Graphics::ScopedSaveState gs(context->g);
    static auto warnOnce = false;
    if (!warnOnce)
    {
        std::cout << "WARNING: THERE IS A HARDCODED 125 HERE!!! FIXME" << std::endl;
        warnOnce = true;
    }
    // FIXME: What is this 1.25?
    auto t = juce::AffineTransform().scaled(1.f / hcdf / 1.25, 1.f / hcdf / 1.25);
    pngZooms[useZoom].second->drawable->draw(context->g, alpha, t);
}

void CScalableBitmap::addPNGForZoomLevel(std::string fname, int zoomLevel)
{
    pngZooms[zoomLevel] = std::make_pair(fname, nullptr);
}

void CScalableBitmap::resolvePNGForZoomLevel(int zoomLevel)
{
    if (pngZooms.find(zoomLevel) == pngZooms.end())
        return;
    if (pngZooms[zoomLevel].second)
        return;

    pngZooms[zoomLevel].second =
        std::move(std::make_unique<CScalableBitmap>(pngZooms[zoomLevel].first.c_str(), nullptr));
}

#else

#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "globals.h"
#include "guihelpers.h"
#include "CScalableBitmap.h"
#include "UserInteractions.h"
#include "UIInstrumentation.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include "filesystem/import.h"
#if MAC
#include "vstgui/lib/platform/mac/macglobals.h"
#endif
#if LINUX
#include "ScalablePiggy.h"
#endif
#if WINDOWS
#if !ESCAPE_FROM_VSTGUI
#include "vstgui/lib/platform/iplatformresourceinputstream.h"
#endif
#endif
#include <unordered_map>

#include <cmath>

#include "resource.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <DebugHelpers.h>
/*
** CScalableBitmap is the only file which uses the SVG Implementatoin so it is also the one
** and only one which ejexts the link symbols.
*/
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

using namespace VSTGUI;
std::atomic<int> CScalableBitmap::instances(0);

namespace
{
NSVGimage *svgParseFromPath(const fs::path &filename, const char *units, float dpi)
{
    std::error_code ec;
    const auto length = fs::file_size(filename, ec);
    if (ec || length == 0)
        return nullptr;

    std::unique_ptr<char[]> data;
    {
        std::filebuf file;
        if (!file.open(filename, std::ios::binary | std::ios::in))
            return nullptr;
        data.reset(new char[length + 1]);
        if (file.sgetn(data.get(), length) != length)
            return nullptr;
    }
    data[length] = '\0';
    return nsvgParse(data.get(), units, dpi);
}
} // anonymous namespace

#if MAC
static const std::string svgFullFileNameFromBundle(const std::string &filename)
{
    CFStringRef cfStr = CFStringCreateWithCString(nullptr, filename.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFBundleCopyResourceURL(VSTGUI::getBundleRef(), cfStr, nullptr, nullptr);
    CFRelease(cfStr);
    if (url)
    {
        CFStringRef urlStr = CFURLGetString(url);
        CFRetain(
            urlStr); // "GET" rule
                     // https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFMemoryMgmt/Concepts/Ownership.html#//apple_ref/doc/uid/20001148-SW1
        const char *csp = CFStringGetCStringPtr(urlStr, kCFStringEncodingUTF8);
        if (csp)
        {
            std::string resPath(csp);
            if (resPath.find("file://") != std::string::npos)
                resPath = resPath.substr(7);
            CFRelease(urlStr);
            CFRelease(url);
            return resPath;
        }
        CFRelease(urlStr);
        CFRelease(url);
    }

    return "";
}

#endif

#if LINUX
static const struct MemorySVG *findMemorySVG(const std::string &filename)
{
    for (const struct MemorySVG *svg = &memorySVGList[0]; svg->name != NULL; svg++)
        if (!std::strncmp(filename.c_str(), svg->name, std::strlen(svg->name)))
            return svg;

    return nullptr;
}
#endif

#if WINDOWS
static int svgContentsFromRCFile(int id, char *svgData, int maxSz)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("svgContentsFromRCFile::GET");
#endif
    static std::unordered_map<int, char *> leakyStore;
    if (leakyStore.find(id) == leakyStore.end())
    {
#ifdef INSTRUMENT_UI
        Surge::Debug::record("svgContentsFromRCFile::READ");
#endif

        VSTGUI::IPlatformResourceInputStream::Ptr istr =
            VSTGUI::IPlatformResourceInputStream::create(CResourceDescription(id));

        if (istr == NULL)
        {
            return -1;
        }

        size_t sz = 1024 * 1024;
        char *leakThis = new char[sz];
        memset(leakThis, 0, sz);
        uint32_t readSize = istr->readRaw(leakThis, sz);
        leakThis[readSize] = 0;
        leakyStore[id] = leakThis;
    }

    memcpy(svgData, leakyStore[id], maxSz);

    return 1;
}
#endif

// Remember this is user zoom * display zoom. See comment in CScalableBitmap.h
void CScalableBitmap::setPhysicalZoomFactor(int zoomFactor)
{
    currentPhysicalZoomFactor = zoomFactor;
}

CScalableBitmap::CScalableBitmap(CResourceDescription desc, VSTGUI::CFrame *f)
    : CBitmap(desc), svgImage(nullptr), frame(f)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("CScalableBitmap::CScalableBitmap desc");
#endif

    int id = 0;
    if (desc.type == CResourceDescription::kIntegerType)
        id = (int32_t)desc.u.id;

    instances++;
    // std::cout << "  Construct CScalableBitmap. instances=" << instances << " id=" << id <<
    // std::endl;

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
        fn2 << "/Library/Audio/Plug-Ins/Components/Surge.component/Contents/Resources/"
            << filename.str();
#elif TARGET_VST2
        fn2 << "/Library/Audio/Plug-Ins/VST/Surge.vst/Contents/Resources/" << filename.str();
#elif TARGET_VST3
        fn2 << "/Library/Audio/Plug-Ins/VST3/Surge.vst3/Contents/Resources/" << filename.str();
#endif

        // std::cout << "FailBack from bad module SVG path to best guess: [" << fn2.str() << "]" <<
        // std::endl;
        svgImage = nsvgParseFromFile(fn2.str().c_str(), "px", 96);
    }
#endif

#if WINDOWS
    int svgid = id + SCALABLE_SVG_OFFSET;
    char *svgData = new char[1024 * 1024];

    if (svgContentsFromRCFile(svgid, svgData, 1024 * 1024) > 0)
        svgImage = nsvgParse(svgData, "px", 96);
    else
        svgImage = NULL;

    delete[] svgData;
#endif

#if LINUX
    if (const MemorySVG *const memSVG = findMemorySVG(filename.str()))
    {
        char *svg = new char[memSVG->size + 1];
        svg[memSVG->size] = '\0';
        strncpy(svg, (const char *)(memorySVGListStart + memSVG->offset), memSVG->size);
        svgImage = nsvgParse(svg, "px", 96);
        delete[] svg;
    }
    else
    {
        std::cerr << filename.str() << " not found" << std::endl;
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

CScalableBitmap::CScalableBitmap(std::string ifname, VSTGUI::CFrame *f)
    : CBitmap(CResourceDescription(0)), svgImage(nullptr), frame(f)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("CScalableBitmap::CScalableBitmap file");
#endif

    // OK so we have to see what type of file we are
    fname = ifname;
    instances++;
    resourceID = -1;

    std::string extension = "svg";
    if (fname.length() > 3)
        extension = fname.substr(fname.length() - 3);

    if (_stricmp(extension.c_str(), "svg") == 0)
    {
        svgImage = svgParseFromPath(string_to_path(fname), "px", 96);

        if (!svgImage)
        {
            std::cout << "Unable to load SVG Image " << fname << std::endl;
        }
    }

    if (_stricmp(extension.c_str(), "png") == 0)
    {
        pngZooms[100] = std::make_pair(fname, std::make_unique<VSTGUI::CBitmap>(fname.c_str()));
    }

    extraScaleFactor = 100;
    currentPhysicalZoomFactor = 100;
    lastSeenZoom = -1;
}

CScalableBitmap::~CScalableBitmap()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("CScalableBitmap::~CScalableBitmap");
#endif

    for (auto const &pair : offscreenCache)
    {
        auto val = pair.second;
        if (val)
        {
            val->forget();
        }
    }
    offscreenCache.clear();

    if (svgImage)
    {
        nsvgDelete(svgImage);
    }
    instances--;
    // std::cout << "  Destroy CScalableBitmap. instances=" << instances << " id=" << resourceID <<
    // " fn=" << fname << std::endl;
}

void CScalableBitmap::draw(CDrawContext *context, const CRect &rect, const CPoint &offset,
                           float alpha)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("CScalableBitmap::draw");
#endif
    /*
     * From 1.6 beta 9 until aeb45a38 we used to have a VUMeter optimization here which we don't
     * need now CScalableBitmap uses an offscreen cache (which it did after 1.6.3 or so I think?).
     * Just FYI!
     */

    if (svgImage || (pngZooms.find(100) != pngZooms.end()))
    {
        /*
        ** Plan of attack here is to use coffscreencontext into a physicalzoomfactor scaled version
        *of rect
        ** see vstgui.surge/vstgui/lib/coffscreencontext.h for how that works. So we just need a
        *good hash
        ** on rect.size, off and physical zoom.
        */
        if (lastSeenZoom != currentPhysicalZoomFactor)
        {
            for (auto const &pair : offscreenCache)
            {
                auto val = pair.second;
                if (val)
                    val->forget();
            }
            offscreenCache.clear();
            lastSeenZoom = currentPhysicalZoomFactor;
        }

        CGraphicsTransform tf =
            CGraphicsTransform().scale(lastSeenZoom / 100.0, lastSeenZoom / 100.0);

        /*
        ** VSTGUI has this wierdo bug where it shrinks backgrounds properly but doesn't grow them.
        *Sigh.
        ** So asymmetrically treat the extra factor here and only here.
        */
        int exs = (extraScaleFactor < 100 ? 100 : extraScaleFactor);
        CGraphicsTransform xtf = CGraphicsTransform().scale(exs / 100.0, exs / 100.0);
        CGraphicsTransform itf = tf.inverse();
        CGraphicsTransform ixtf = xtf.inverse();

        if (offscreenCache.find(offset) == offscreenCache.end())
        {
#ifdef INSTRUMENT_UI
            Surge::Debug::record("CScalableBitmap::draw::createOffscreenCache");
#endif
            VSTGUI::CPoint sz = rect.getSize();
            ixtf.transform(sz);

            VSTGUI::CPoint offScreenSz = sz;
            tf.transform(offScreenSz);

            if (auto offscreen =
                    COffscreenContext::create(frame, ceil(offScreenSz.x), ceil(offScreenSz.y)))
            {
                offscreen->beginDraw();
                VSTGUI::CRect newRect(0, 0, rect.getWidth(), rect.getHeight());

                CDrawContext::Transform trsf(*offscreen, tf);
                CDrawContext::Transform xtrsf(*offscreen, ixtf);

                if (svgImage)
                {
                    drawSVG(offscreen, newRect, offset, alpha);
                }
                else
                {
                    // OK so look through our zoom PNGs looking for the one above cpz
                    int zoomScan = 100;

                    for (auto &zl : pngZooms)
                    {
                        zoomScan = zl.first;
                        if (zoomScan >= currentPhysicalZoomFactor)
                        {
                            break;
                        }
                    }
                    auto etf = VSTGUI::CGraphicsTransform().scale(extraScaleFactor / 100.0,
                                                                  extraScaleFactor / 100.0);
                    VSTGUI::CDrawContext::Transform t1(*offscreen, etf);

                    auto ztf =
                        VSTGUI::CGraphicsTransform().scale(100.0 / zoomScan, 100.0 / zoomScan);
                    VSTGUI::CDrawContext::Transform t2(*offscreen, ztf);

                    ztf.inverse().transform(newRect);
                    auto offs = offset;
                    ztf.inverse().transform(offs);
                    if (!pngZooms[zoomScan].second)
                        resolvePNGForZoomLevel(zoomScan);
                    if (pngZooms[zoomScan].second)
                        pngZooms[zoomScan].second->draw(offscreen, newRect, offs, 1.0);
                }
                offscreen->endDraw();
                CBitmap *tmp = offscreen->getBitmap();
                if (tmp)
                {
                    offscreenCache[offset] = tmp;
#if !LINUX
                    /*
                     * See #4081. Basically on linux the bitmap is reference toggled up with our
                     * version of vstgui. Why? Who knows. But we are gonna kill vstgui RSN so
                     * for now just don't do the extra remember on LINUX
                     */
                    tmp->remember();
#endif
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

void CScalableBitmap::drawSVG(CDrawContext *dc, const CRect &rect, const CPoint &offset,
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
    ** Our overall transform moves us to the x/y position of our view rects top left point and
    *shifts us by
    ** our offset. Don't forgert the extra zoom also!
    */

    VSTGUI::CGraphicsTransform tf =
        VSTGUI::CGraphicsTransform()
            .translate(viewS.getTopLeft().x - offset.x, viewS.getTopLeft().y - offset.y)
            .scale(extraScaleFactor / 100.0, extraScaleFactor / 100.0);
    VSTGUI::CDrawContext::Transform t(*dc, tf);

#if LINUX
    Surge::UI::NonIntegralAntiAliasGuard naag(dc);
#endif

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
        VSTGUI::CGraphicsPath *gp = dc->createGraphicsPath();
        for (auto path = shape->paths; path != NULL; path = path->next)
        {
            for (auto i = 0; i < path->npts - 1; i += 3)
            {
                float *p = &path->pts[i * 2];

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
                NSVGgradient *ngrad = shape->fill.gradient;

                float *x = ngrad->xform;
                // This re-order is on purpose; vstgui and nanosvg use different order for diagonals
                VSTGUI::CGraphicsTransform gradXform(x[0], x[2], x[1], x[3], x[4], x[5]);
                VSTGUI::CGradient::ColorStopMap csm;
                VSTGUI::CGradient *cg = VSTGUI::CGradient::create(csm);

                for (int i = 0; i < ngrad->nstops; ++i)
                {
                    auto stop = ngrad->stops[i];
                    cg->addColorStop(stop.offset, svgColorToCColor(stop.color, shape->opacity));
                }
                VSTGUI::CPoint s0(0, 0), s1(0, 1);
                VSTGUI::CPoint p0 = gradXform.inverse().transform(s0);
                VSTGUI::CPoint p1 = gradXform.inverse().transform(s1);

                dc->fillLinearGradient(gp, *cg, p0, p1, evenOdd);
                cg->forget();
            }
            else if (shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT)
            {
                bool evenOdd = (shape->fillRule == NSVGfillRule::NSVG_FILLRULE_EVENODD);
                NSVGgradient *ngrad = shape->fill.gradient;

                float *x = ngrad->xform;
                // This re-order is on purpose; vstgui and nanosvg use different order for diagonals
                VSTGUI::CGraphicsTransform gradXform(x[0], x[2], x[1], x[3], x[4], x[5]);
                VSTGUI::CGradient::ColorStopMap csm;
                VSTGUI::CGradient *cg = VSTGUI::CGradient::create(csm);

                for (int i = 0; i < ngrad->nstops; ++i)
                {
                    auto stop = ngrad->stops[i];
                    cg->addColorStop(stop.offset, svgColorToCColor(stop.color, shape->opacity));
                }

                VSTGUI::CPoint s0(0, 0),
                    s1(0.5, 0); // the box has size -0.5, 0.5 so the radius is 0.5
                VSTGUI::CPoint p0 = gradXform.inverse().transform(s0);
                VSTGUI::CPoint p1 = gradXform.inverse().transform(s1);
                dc->fillRadialGradient(gp, *cg, p0, p1.x, CPoint(0, 0), evenOdd);
                cg->forget();
            }
            else
            {
                std::cerr << "Unknown Shape Fill Type" << std::endl;
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
                dc->setFrameColor(svgColorToCColor(shape->stroke.color, shape->opacity));
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
    return VSTGUI::CColor(r, g, b, a);
}

void CScalableBitmap::addPNGForZoomLevel(std::string fname, int zoomLevel)
{
    pngZooms[zoomLevel] = std::make_pair(fname, nullptr);
}

void CScalableBitmap::resolvePNGForZoomLevel(int zoomLevel)
{
    if (pngZooms.find(zoomLevel) == pngZooms.end())
        return;
    if (pngZooms[zoomLevel].second)
        return;

    pngZooms[zoomLevel].second =
        std::move(std::make_unique<VSTGUI::CBitmap>(pngZooms[zoomLevel].first.c_str()));
}
#endif
