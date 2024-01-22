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

#include "SurgeImage.h"
#include "SurgeXTBinary.h"

#include "fmt/core.h"
#include "DebugHelpers.h"

SurgeImage::SurgeImage(int rid)
{
    resourceID = rid;
    std::string fn = fmt::format("bmp{:05d}_svg", rid);
    int bds;
    auto bd = SurgeXTBinary::getNamedResource(fn.c_str(), bds);

    if (bd)
    {
        drawable = juce::Drawable::createFromImageData(bd, bds);
        currentDrawable = drawable.get();
    }
}

SurgeImage::SurgeImage(const std::string &fname)
{
    bool doLoad = true;
    if (fname.find(".png") != std::string::npos)
    {
        doLoad = false;
    }
    this->fname = fname;
    if (doLoad)
    {
        forceLoadFromFile();
    }
}

SurgeImage::SurgeImage(std::unique_ptr<juce::Drawable> &in)
    : drawable(std::move(in)), currentDrawable(drawable.get())
{
}

SurgeImage::~SurgeImage() = default;

void SurgeImage::forceLoadFromFile()
{
    if (!drawable)
    {
        drawable = juce::Drawable::createFromImageFile(juce::File(fname));
        currentDrawable = drawable.get();
    }
}

SurgeImage *SurgeImage::createFromBinaryWithPrefix(const std::string &prefix, int id)
{
    std::string fn = fmt::format("{:s}{:05d}_svg", prefix, id);
    int bds;
    auto bd = SurgeXTBinary::getNamedResource(fn.c_str(), bds);

    if (bd)
    {
        auto q = juce::Drawable::createFromImageData(bd, bds);
        return new SurgeImage(q);
    }

    return nullptr;
}

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
    {
        return;
    }
    if (pngZooms[zoomLevel].second)
    {
        return;
    }

    pngZooms[zoomLevel].second =
        std::move(std::make_unique<SurgeImage>(pngZooms[zoomLevel].first.c_str()));
    pngZooms[zoomLevel].second->forceLoadFromFile();
}

juce::Drawable *SurgeImage::internalDrawableResolved()
{
    if (!currentDrawable && resourceID == -1)
    {
        forceLoadFromFile();
    }
    return currentDrawable;
}

juce::AffineTransform SurgeImage::scaleAdjustmentTransform() const
{
    auto res = juce::AffineTransform();

    if (adjustForScale)
    {
        res = res.scaled(100.0 / resolvedZoomFactor);
    }

    return res;
}

juce::Image SurgeImage::asJuceImage(float scaleBy)
{
    auto d = internalDrawableResolved();
    if (!d)
        return {};

    juce::Image res(juce::Image::ARGB, d->getWidth() * scaleBy, d->getHeight() * scaleBy, true);
    juce::Graphics g(res);
    d->draw(g, 1.f, juce::AffineTransform::scale(scaleBy));
    return res;
}