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

#ifndef SURGE_XT_NUMBERFIELD_H
#define SURGE_XT_NUMBERFIELD_H

#include <JuceHeader.h>
#include "SkinModel.h"
#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct NumberField : public juce::Component, public WidgetBaseMixin<NumberField>
{
    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    float value{0};
    int iValue{0}, iMin{0}, iMax{1};
    void setValue(float f) override
    {
        value = f;
        bounceToInt();
        repaint();
    }
    float getValue() const override { return value; }
    int getIntValue() const { return iValue; }
    void setIntValue(int v)
    {
        value = 1.f * (v - iMin) / (iMax - iMin);
        bounceToInt();
        notifyValueChanged();
        repaint();
    }

    void bounceToInt();

    int poly{0};
    int getPlayingVoiceCount() const { return poly; }
    void setPlayingVoiceCount(int p) { poly = p; }

    std::string valueToDisplay() const;

    void paint(juce::Graphics &g) override;

    void changeBy(int inc);

    Surge::Skin::Parameters::NumberfieldControlModes controlMode{Surge::Skin::Parameters::NONE};
    void setControlMode(Surge::Skin::Parameters::NumberfieldControlModes n);
    Surge::Skin::Parameters::NumberfieldControlModes getControlMode() const { return controlMode; }

    enum MouseMode
    {
        NONE,
        RMB,
        DRAG
    } mouseMode{NONE};

    juce::Drawable *bg{nullptr}, *bgHover{nullptr};
    void setBackgroundDrawable(juce::Drawable *b) { bg = b; };
    void setHoverBackgroundDrawable(juce::Drawable *bgh) { bgHover = bgh; }

    juce::Point<float> mouseDownOrigin;
    float lastDistanceChecked{0};
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    bool isHover{false};
    void mouseEnter(const juce::MouseEvent &event) override
    {
        isHover = true;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }
    void mouseExit(const juce::MouseEvent &event) override
    {
        isHover = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    juce::Colour textColour, textHoverColour;
    void setTextColour(juce::Colour c)
    {
        textColour = c;
        repaint();
    }
    void setHoverTextColour(juce::Colour c)
    {
        textHoverColour = c;
        repaint();
    }
};

} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_NUMBERFIELD_H
