#pragma once

/*
 * A utility wrapper around loading and drawables and stuff
 */

#if ESCAPE_FROM_VSTGUI
#include "efvg/escape_from_vstgui.h"
#else
#include "vstgui/vstgui.h"
#endif

#include <vector>
#include <map>
#include <atomic>

class SurgeImageStore;

class SurgeImage
{
  public:
    SurgeImage(int resourceId);
    SurgeImage(std::string fname);
    ~SurgeImage();

    void addPNGForZoomLevel(std::string fname, int zoomLevel);
    void resolvePNGForZoomLevel(int zoomLevel);
    void setPhysicalZoomFactor(int zoomFactor);

    int resourceID = -1;
    std::string fname;

    juce::Drawable *getDrawable() const
    {
        if (drawable)
            return drawable.get();
        return nullptr;
    }

  private:
    static std::atomic<int> instances;

    int lastSeenZoom, bestFitScaleGroup;
    int extraScaleFactor;

    /*
     * The zoom 100 bitmap and optional higher resolution bitmaps for zooms
     * map vs unordered is on purpose here - we need this ordered for our zoom search
     */
    std::map<int, std::pair<std::string, std::unique_ptr<SurgeImage>>> pngZooms;
    int currentPhysicalZoomFactor;

    std::unique_ptr<juce::Drawable> drawable;
};
