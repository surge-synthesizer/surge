#pragma once

/*
** CScalableBitmap is an implmentation of VSGTUI::CBitmap which can
** load bitmaps at multiple resolutions and draw them scaled accordingly
*/

#if ESCAPE_FROM_VSTGUI
#include "efvg/escape_from_vstgui.h"
#else
#include "vstgui/vstgui.h"
#endif

#include <vector>
#include <map>
#include <atomic>

class CScalableBitmap : public VSTGUI::CBitmap
{
  public:
    CScalableBitmap(VSTGUI::CResourceDescription d);
    CScalableBitmap(std::string fname);
    ~CScalableBitmap();

    virtual void draw(VSTGUI::CDrawContext *context, const VSTGUI::CRect &rect,
                      const VSTGUI::CPoint &offset, float alpha);

    /*
     * CSBMs can pick PNGs at different zoom levels
     */
    virtual void addPNGForZoomLevel(std::string fname, int zoomLevel);
    virtual void resolvePNGForZoomLevel(int zoomLevel);

    /*
    ** The 'zoom factor' is set statically across all bitmaps since it is a
    ** function of the current editor which contains them.
    **
    ** If in the future we have a non 1:1 editor -> instance relationship we
    ** may need to reconsider the management of this factor.
    **
    ** This zoom factor is the product of the logical (user) zoom factor
    ** and any zoom the display may apply, which is checked in
    *SurgeGUIEditor::getDisplayBackingScale
    ** and applied *before* this call. See SurgeGUIEditor::setZoomFactor.
    */
    void
    setPhysicalZoomFactor(int zoomFactor); // See comment on zoom factor units in SurgeGUIEditor.h

    /*
    ** VST2 has an error where the background doesn't zoom with the frame. Everything else
    ** does though. So we need to hand scale the background and only the background image
    ** when we draw it. We do that, simply, by having this class have an extra scale factor
    ** which we apply to the BG Bitmap in Vst2PluginInstance::handleZoom.
    */
    void setExtraScaleFactor(int a) { extraScaleFactor = a; }

    int resourceID = -1;
    std::string fname;

  private:
    static std::atomic<int> instances;

    int lastSeenZoom, bestFitScaleGroup;
    int extraScaleFactor;

    /*
     * The zoom 100 bitmap and optional higher resolution bitmaps for zooms
     * map vs unordered is on purpose here - we need this ordered for our zoom search
     */
    std::map<int, std::pair<std::string, std::unique_ptr<VSTGUI::CBitmap>>> pngZooms;
    int currentPhysicalZoomFactor;
};
