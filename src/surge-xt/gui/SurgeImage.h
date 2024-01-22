/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEIMAGE_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEIMAGE_H

/*
 * A utility wrapper around loading and drawables and stuff
 */

#include "juce_gui_basics/juce_gui_basics.h"

#include <vector>
#include <map>
#include <atomic>

class SurgeImageStore;

class SurgeImage
{
  public:
    SurgeImage(int resourceId);
    SurgeImage(const std::string &fname);
    SurgeImage(std::unique_ptr<juce::Drawable> &in);
    ~SurgeImage();

    void forceLoadFromFile();

    void addPNGForZoomLevel(const std::string &fname, int zoomLevel);
    void resolvePNGForZoomLevel(int zoomLevel);
    void setPhysicalZoomFactor(int zoomFactor);

    static SurgeImage *createFromBinaryWithPrefix(const std::string &prefix, int id);

    int resourceID = -1;
    std::string fname;

    void draw(juce::Graphics &g, float opacity,
              const juce::AffineTransform &transform = juce::AffineTransform())
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(scaleAdjustmentTransform());
        auto idr = internalDrawableResolved();
        if (idr)
            idr->draw(g, opacity, transform);
    }
    void drawAt(juce::Graphics &g, float x, float y, float opacity)
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(scaleAdjustmentTransform());
        auto idr = internalDrawableResolved();
        if (idr)
            idr->drawAt(g, x, y, opacity);
    }
    void drawWithin(juce::Graphics &g, juce::Rectangle<float> destArea,
                    juce::RectanglePlacement placement, float opacity)
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(scaleAdjustmentTransform());
        auto idr = internalDrawableResolved();
        if (idr)
            idr->drawWithin(g, destArea, placement, opacity);
    }

    std::unique_ptr<juce::Drawable> createCopy()
    {
        auto idr = internalDrawableResolved();
        if (idr)
            return idr->createCopy();
        return nullptr;
    }

    /*
     * I provide direct access to the drawable but be careful. Things like
     * changing zoom or skins can invalidate the pointer returned from here.
     */
    juce::Drawable *getDrawableButUseWithCaution() { return internalDrawableResolved(); }

    juce::Image asJuceImage(float scaleBy = 1.0);

  private:
    juce::Drawable *internalDrawableResolved();
    juce::AffineTransform scaleAdjustmentTransform() const;

    static std::atomic<int> instances;
    bool adjustForScale{false};
    int resolvedZoomFactor{100};

    /*
     * The zoom 100 bitmap and optional higher resolution bitmaps for zooms
     * map vs unordered is on purpose here - we need this ordered for our zoom search
     */
    std::map<int, std::pair<std::string, std::unique_ptr<SurgeImage>>> pngZooms;
    int currentPhysicalZoomFactor;

    std::unique_ptr<juce::Drawable> drawable;
    juce::Drawable *currentDrawable{nullptr};
};

#endif // SURGE_SRC_SURGE_XT_GUI_SURGEIMAGE_H
