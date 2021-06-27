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
#include "CScalableBitmap.h"

CScalableBitmap::CScalableBitmap(VSTGUI::CResourceDescription d) : VSTGUI::CBitmap(d)
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

CScalableBitmap::CScalableBitmap(std::string fname)
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

    auto hcdf =
        juce::Desktop::getInstance().getDisplays().getDisplayForRect(rect.asJuceIntRect())->scale;
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
        std::move(std::make_unique<CScalableBitmap>(pngZooms[zoomLevel].first.c_str()));
}
