/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include <JuceHeader.h>
#include "SurgeImage.h"

SurgeImage::SurgeImage(int rid)
{
    resourceID = rid;
    std::string fn = "bmp00" + std::to_string(rid) + "_svg";
    int bds;
    auto bd = BinaryData::getNamedResource(fn.c_str(), bds);
    if (bd)
    {
        drawable = juce::Drawable::createFromImageData(bd, bds);
        currentDrawable = drawable.get();
    }
}

SurgeImage::SurgeImage(const std::string &fname)
{
    this->fname = fname;
    drawable = juce::Drawable::createFromImageFile(juce::File(fname));
    currentDrawable = drawable.get();
}

SurgeImage::~SurgeImage() = default;

void SurgeImage::setPhysicalZoomFactor(int zoomFactor)
{
    resolvePNGForZoomLevel(zoomFactor);
    currentPhysicalZoomFactor = zoomFactor;
    if (pngZooms.size() > 0)
    {
        currentDrawable = drawable.get();
        adjustForScale = false;
        for (auto &p : pngZooms)
        {
            if (p.first <= currentPhysicalZoomFactor && p.second.second)
            {
                currentDrawable = p.second.second->drawable.get();
                adjustForScale = true;
                resolvedZoomFactor = p.first;
            }
        }
    }
    else
    {
        adjustForScale = false;
    }
}

void SurgeImage::addPNGForZoomLevel(const std::string &fname, int zoomLevel)
{
    pngZooms[zoomLevel] = std::make_pair(fname, nullptr);
}

void SurgeImage::resolvePNGForZoomLevel(int zoomLevel)
{
    if (pngZooms.find(zoomLevel) == pngZooms.end())
        return;
    if (pngZooms[zoomLevel].second)
        return;

    pngZooms[zoomLevel].second =
        std::move(std::make_unique<SurgeImage>(pngZooms[zoomLevel].first.c_str()));
}

juce::Drawable *SurgeImage::internalDrawableResolved() const { return currentDrawable; }

juce::AffineTransform SurgeImage::scaleAdjustmentTransform() const
{
    auto res = juce::AffineTransform();
    if (adjustForScale)
    {
        res = res.scaled(100.0 / resolvedZoomFactor);
    }
    return res;
}