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
#include "SurgeJUCEHelpers.h"
#include "SurgeImage.h"
#include "XMLConfiguredMenus.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Widgets
{

EffectChooser::EffectChooser()
{
    setRepaintsOnMouseActivity(true);
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    for (int i = 0; i < n_fx_slots; ++i)
    {
        fxTypes[i] = fxt_off;
        auto q = std::make_unique<OverlayAsAccessibleButton<EffectChooser>>(this, fxslot_names[i]);
        q->setBounds(getEffectRectangle(i));
        q->onPress = [this, i](auto *t) {
            this->currentEffect = i;
            this->notifyValueChanged();
        };
        q->onReturnKey = [this, i](auto *t) {
            this->currentEffect = i;
            this->notifyValueChanged();
            return true;
        };
        addAndMakeVisible(*q);
        slotAccOverlays[i] = std::move(q);
    }
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
        g.drawText("Can't do v1 skins in Surge XT!", getLocalBounds(),
                   juce::Justification::centred);
        return;
    }

    if (bg)
    {
        bg->draw(g, 1.0);
    }

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));

    juce::Colour bgd, frm, txt;

    // Scene boxes
    for (int i = 0; i < n_scenes; ++i)
    {
        auto r = getSceneRectangle(i);

        if (isHovered && currentSceneHover == i)
        {
            bgd = skin->getColor(Colors::Effect::Grid::Scene::BackgroundHover);
            frm = skin->getColor(Colors::Effect::Grid::Scene::BorderHover);
            txt = skin->getColor(Colors::Effect::Grid::Scene::TextHover);
        }
        else
        {
            bgd = skin->getColor(Colors::Effect::Grid::Scene::Background);
            frm = skin->getColor(Colors::Effect::Grid::Scene::Border);
            txt = skin->getColor(Colors::Effect::Grid::Scene::Text);
        }

        g.setColour(bgd);
        g.fillRect(r);

        g.setColour(frm);
        g.drawRect(r);

        g.setColour(txt);
        g.drawText(scenename[i], r, juce::Justification::centred);
    }

    // FX slots
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

void EffectChooser::resized()
{
    int i = 0;
    for (const auto &q : slotAccOverlays)
    {
        q->setBounds(getEffectRectangle(i));
        i++;
    }
}

void EffectChooser::drawSlotText(juce::Graphics &g, const juce::Rectangle<int> &r,
                                 const juce::Colour &txtcol, int fxid)
{
    auto fxname = fx_type_acronyms[fxid];

    g.setColour(txtcol);

    if (strcmp(fxname, "OFF") == 0)
    {
        auto center = r.getCentre();
        auto line = juce::Rectangle<float>(center.x - 2, center.y, 6, 1);

        g.drawRect(line);
    }
    else
    {
        g.drawText(fx_type_acronyms[fxid], r, juce::Justification::centred);
    }
}

juce::Rectangle<int> EffectChooser::getSceneRectangle(int i)
{
    const int scenelabelbox[n_scenes][2] = {{4, 0}, {4, 45}};
    const int scenelabelboxWidth = 9, scenelabelboxHeight = 11;

    auto r = juce::Rectangle<int>(scenelabelbox[i][0], scenelabelbox[i][1], scenelabelboxWidth,
                                  scenelabelboxHeight);

    return r;
}

juce::Rectangle<int> EffectChooser::getEffectRectangle(int i)
{
    static const int fxslotWidth = 19, fxslotHeight = 11;
    static int fxslotpos[n_fx_slots][2];
    static bool fxslotsInitialized{false};
    static const int topY = 1;
    static const int startX = 15, globX = 120;

    if (!fxslotsInitialized)
    {
        fxslotsInitialized = true;

        /*
         * This just assumes the 16 slot ordering.
         */
        jassert(n_fx_slots == 16);

        static int rowYs[3] = {0, 45, 23};

        for (int i = 0; i < n_fx_slots; ++i)
        {
            int row = (i / 2) % 4;
            int num = i % 2 + 2 * (i >= fxslot_ains3);

            int x = 0, y = 0;

            if (row < 3)
            {
                x = startX + num * 23;
                y = rowYs[row];
            }
            else
            {
                x = globX;
                y = num * 15;
            }

            fxslotpos[i][0] = x;
            fxslotpos[i][1] = y;
        }
    }

    auto r = juce::Rectangle<int>(fxslotpos[i][0], fxslotpos[i][1], fxslotWidth, fxslotHeight);

    return r;
}

void EffectChooser::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (!event.mods.isRightButtonDown() && !hasDragged && currentClicked >= 0)
    {
        storage->getPatch().isDirty = true;
        deactivatedBitmask ^= (1 << currentClicked);
        notifyValueChanged();
    }
}

void EffectChooser::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

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
                sge->effectSettingsBackgroundClick(i, this);
            }
        }
    }

    if (currentClicked >= 0)
    {
        if (event.mods.isRightButtonDown())
        {
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            if (sge && sge->fxMenu)
            {
                sge->fxMenu->menu.showMenuAsync(juce::PopupMenu::Options());
            }
        }
    }
}

void EffectChooser::mouseUp(const juce::MouseEvent &event)
{
    if (!hasDragged && currentClicked >= 0)
    {
        currentEffect = currentClicked;
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

                if (event.mods.isCommandDown())
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
    if (supressMainFrameMouseEvent(event))
    {
        return;
    }

    if (event.getDistanceFromDragStart() > 3 && event.mods.isLeftButtonDown())
    {
        if (!hasDragged)
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }

        hasDragged = true;

        dragX = event.getDistanceFromDragStartX();
        dragY = event.getDistanceFromDragStartY();

        repaint();
    }
}

void EffectChooser::mouseMove(const juce::MouseEvent &event)
{
    int nextHover = -1;
    int nextSceneHover = -1;

    // scene boxes
    for (int i = 0; i < n_scenes; ++i)
    {
        auto r = getSceneRectangle(i);

        if (r.contains(event.getPosition()))
        {
            nextSceneHover = i;
        }
    }

    if (nextSceneHover != currentSceneHover)
    {
        currentSceneHover = nextSceneHover;
        repaint();
    }

    // FX boxes
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

template <> struct DiscreteAHRange<EffectChooser>
{
    static int iMaxV(EffectChooser *t) { return n_fx_slots; }
    static int iMinV(EffectChooser *t) { return 0; }
};

template <> struct DiscreteAHStringValue<EffectChooser>
{
    static std::string stringValue(EffectChooser *comp, double ahValue)
    {
        return fxslot_names[comp->getCurrentEffect()];
    }
};

template <> struct DiscreteRO<EffectChooser>
{
    static bool isReadOnly(EffectChooser *comp) { return true; }
};

std::unique_ptr<juce::AccessibilityHandler> EffectChooser::createAccessibilityHandler()
{
    return std::make_unique<DiscreteAH<EffectChooser, juce::AccessibilityRole::group>>(this);
}
} // namespace Widgets
} // namespace Surge
