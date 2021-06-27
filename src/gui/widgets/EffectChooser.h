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

#ifndef SURGE_XT_EFFECTCHOOSER_H
#define SURGE_XT_EFFECTCHOOSER_H

#include <JuceHeader.h>
#include <array>
#include "SurgeStorage.h"
#include "efvg/escape_from_vstgui.h"
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"

namespace Surge
{
namespace Widgets
{
struct EffectChooser : public juce::Component, public WidgetBaseMixin<EffectChooser>
{
    EffectChooser();
    ~EffectChooser();
    void paint(juce::Graphics &g) override;

    void drawSlotText(juce::Graphics &g, const juce::Rectangle<int> &r, const juce::Colour &txtcol,
                      int fxid);

    /*
     * These aren't used but are part of the interface. The callback casts.
     */
    float getValue() const override
    {
        jassert(false);
        return value;
    }
    void setValue(float f) override
    {
        jassert(false);
        value = f;
    }
    float value{0.f};

    void setCurrentEffect(int c) { currentEffect = c; }
    int getCurrentEffect() const { return currentEffect; }
    int currentEffect{0};

    void setEffectType(int index, int type) { fxTypes[index] = type; }
    std::array<int, n_fx_slots> fxTypes;

    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseEnter(const juce::MouseEvent &event) override
    {
        isHovered = true;
        mouseMove(event);
    }
    void mouseExit(const juce::MouseEvent &event) override { isHovered = false; }
    // State for mouse events
    bool isHovered{false}, hasDragged{false};
    int currentHover{-1}, currentClicked{-1};
    int dragX, dragY;

    void setBypass(int b) { bypassState = b; }
    int bypassState{0};
    void setDeactivatedBitmask(int d) { deactivatedBitmask = d; };
    int getDeactivatedBitmask() const { return deactivatedBitmask; }
    int deactivatedBitmask{0};

    juce::Drawable *bg{nullptr};
    void setBackgroundDrawable(juce::Drawable *b) { bg = b; }

    juce::Rectangle<int> getSceneRectangle(int sceneNo);
    juce::Rectangle<int> getEffectRectangle(int fx);
    const char *scenename[n_scenes] = {"A", "B"};

    bool isBypassedOrDeactivated(int fxslot)
    {
        if (deactivatedBitmask & (1 << fxslot))
            return true;
        switch (bypassState)
        {
        case fxb_no_fx:
            return true;
        case fxb_no_sends:
            if ((fxslot == 4) || (fxslot == 5))
                return true;
            break;
        case fxb_scene_fx_only:
            if (fxslot > 3)
                return true;
            break;
        }
        return false;
    }

    void getColorsForSlot(int fxslot, juce::Colour &bgcol, juce::Colour &frcol,
                          juce::Colour &txtcol);
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_EFFECTCHOOSER_H
