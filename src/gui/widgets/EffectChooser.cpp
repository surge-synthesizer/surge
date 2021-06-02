#include "EffectChooser.h"
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

#include "RuntimeFont.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Widgets
{
EffectChooser::EffectChooser()
{
    setRepaintsOnMouseActivity(true);
    for (int i = 0; i < n_fx_slots; ++i)
        fxTypes[i] = fxt_off;
};
EffectChooser::~EffectChooser() = default;

void EffectChooser::paint(juce::Graphics &g)
{
    if (skin->getVersion() < 2)
    {
        // FIXME implement this
        jassert(false);
        g.fillAll(juce::Colours::red);
        g.setColour(juce::Colours::white);
        g.drawText("Can't do V1 yet", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (bg)
    {
        bg->draw(g, 1.0);
    }

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));

    for (int i = 0; i < n_scenes; ++i)
    {
        auto r = getSceneRectangle(i);
        g.setColour(skin->getColor(Colors::Effect::Grid::Scene::Background));
        g.fillRect(r);

        g.setColour(skin->getColor(Colors::Effect::Grid::Scene::Border));
        g.drawRect(r);

        g.setColour(skin->getColor(Colors::Effect::Grid::Scene::Text));
        g.drawText(scenename[i], r, juce::Justification::centred);
    }

    // FX slots
    juce::Colour bgd, frm, txt;

    for (int i = 0; i < n_fx_slots; i++)
    {
        auto r = getEffectRectangle(i);
        getColorsForSlot(i, bgd, frm, txt);

        g.setColour(bgd);
        g.fillRect(r);
        g.setColour(frm);
        g.drawRect(r);

        drawSlotText(g, r, txt, fxTypes[i]);
    }

    if (hasDragged && currentClicked >= 0)
    {
        auto r = getEffectRectangle(currentClicked)
                     .translated(dragX, dragY)
                     .constrainedWithin(getLocalBounds());
        getColorsForSlot(currentClicked, bgd, frm, txt);

        g.setColour(bgd);
        g.fillRect(r);
        g.setColour(frm);
        g.drawRect(r);

        drawSlotText(g, r, txt, fxTypes[currentClicked]);
    }
}

void EffectChooser::drawSlotText(juce::Graphics &g, const juce::Rectangle<int> &r,
                                 const juce::Colour &txtcol, int fxid)
{
    auto fxname = fx_type_shortnames[fxid];

    g.setColour(txtcol);

    if (strcmp(fxname, "OFF") == 0)
    {
        auto center = r.getCentre();
        auto line = juce::Rectangle<float>(center.x - 2, center.y, 6, 1);

        g.drawRect(line);
    }
    else
    {
        g.drawText(fx_type_shortnames[fxid], r, juce::Justification::centred);
    }
}

juce::Rectangle<int> EffectChooser::getSceneRectangle(int i)
{
    const int scenelabelbox[n_scenes][2] = {{1, 1}, {1, 41}};
    const int scenelabelboxWidth = 11, scenelabelboxHeight = 11;

    auto r = juce::Rectangle<int>(scenelabelbox[i][0], scenelabelbox[i][1], scenelabelboxWidth,
                                  scenelabelboxHeight);

    return r;
}

juce::Rectangle<int> EffectChooser::getEffectRectangle(int i)
{
    const int fxslotWidth = 19, fxslotHeight = 11;
    const int fxslotpos[n_fx_slots][2] = {{18, 1},  {44, 1},  {18, 41}, {44, 41},
                                          {18, 21}, {44, 21}, {89, 11}, {89, 31}};

    auto r = juce::Rectangle<int>(fxslotpos[i][0], fxslotpos[i][1], fxslotWidth, fxslotHeight);

    return r;
}

void EffectChooser::mouseDown(const juce::MouseEvent &event)
{
    hasDragged = false;
    currentClicked = -1;
    for (int i = 0; i < n_fx_slots; ++i)
    {
        auto r = getEffectRectangle(i);
        if (r.contains(event.getPosition()))
        {
            currentClicked = i;
        }
    }

    for (int i = 0; i < n_scenes; ++i)
    {
        auto r = getSceneRectangle(i);
        if (r.contains(event.getPosition()))
        {
            auto sge = firstListenerOfType<SurgeGUIEditor>();
            if (sge)
            {
                sge->effectSettingsBackgroundClick(i);
            }
        }
    }
}

void EffectChooser::mouseUp(const juce::MouseEvent &event)
{
    if (!hasDragged && currentClicked >= 0)
    {
        if (event.mods.isRightButtonDown())
        {
            deactivatedBitmask ^= (1 << currentClicked);
        }
        else
        {
            currentEffect = currentClicked;
        }
        notifyValueChanged();
    }

    if (hasDragged)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        for (int i = 0; i < n_fx_slots; ++i)
        {
            auto r = getEffectRectangle(i);
            if (r.contains(event.getPosition()))
            {
                auto m = SurgeSynthesizer::FXReorderMode::SWAP;
                if (event.mods.isCtrlDown())
                {
                    m = SurgeSynthesizer::FXReorderMode::COPY;
                }
                if (event.mods.isShiftDown())
                {
                    m = SurgeSynthesizer::FXReorderMode::MOVE;
                }
                auto sge = firstListenerOfType<SurgeGUIEditor>();
                if (sge)
                {
                    sge->swapFX(currentClicked, i, m);
                    currentEffect = i;
                    notifyValueChanged();
                }
            }
        }
        hasDragged = false;
        repaint();
    }
}

void EffectChooser::mouseDrag(const juce::MouseEvent &event)
{
    if (event.getDistanceFromDragStart() > 0)
    {
        if (!hasDragged)
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        hasDragged = true;
    }

    dragX = event.getDistanceFromDragStartX();
    dragY = event.getDistanceFromDragStartY();
    repaint();
}

void EffectChooser::mouseMove(const juce::MouseEvent &event)
{
    int nextHover = -1;
    for (int i = 0; i < n_fx_slots; ++i)
    {
        auto r = getEffectRectangle(i);
        if (r.contains(event.getPosition()))
        {
            nextHover = i;
        }
    }
    if (nextHover != currentHover)
    {
        currentHover = nextHover;
        repaint();
    }
}

void EffectChooser::getColorsForSlot(int fxslot, juce::Colour &bgcol, juce::Colour &frcol,
                                     juce::Colour &txtcol)
{
    // This is lunacy but hey.
    bool byp = isBypassedOrDeactivated(fxslot);
    if (fxslot == currentEffect)
    {
        if (byp)
        {
            if (isHovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::BypassedSelected::Text);
            }
        }
        else
        {
            if (isHovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Selected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Selected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Selected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Selected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Selected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Selected::Text);
            }
        }
    }
    else
    {
        if (byp)
        {
            if (isHovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Bypassed::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Bypassed::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Bypassed::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Bypassed::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Bypassed::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Bypassed::Text);
            }
        }
        else
        {
            if (isHovered && fxslot == currentHover)
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Unselected::BackgroundHover);
                frcol = skin->getColor(Colors::Effect::Grid::Unselected::BorderHover);
                txtcol = skin->getColor(Colors::Effect::Grid::Unselected::TextHover);
            }
            else
            {
                bgcol = skin->getColor(Colors::Effect::Grid::Unselected::Background);
                frcol = skin->getColor(Colors::Effect::Grid::Unselected::Border);
                txtcol = skin->getColor(Colors::Effect::Grid::Unselected::Text);
            }
        }
    }
}
} // namespace Widgets
} // namespace Surge