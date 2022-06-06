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

#ifndef SURGE_XT_SWITCH_H
#define SURGE_XT_SWITCH_H

#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeImage;
class SurgeStorage;

namespace Surge
{
namespace Widgets
{
/*
 * Switch is a class which encompasses two functions
 * 1: A Binary Switch (on off) and
 * 2: A multi-select switch with wheel directionality
 *
 * We use it for things like unipolar, mute, and filer subtype
 *
 * The rough contract is:
 */
struct Switch : public juce::Component, public WidgetBaseMixin<Switch>, public LongHoldMixin<Switch>
{
    Switch();
    ~Switch();

    bool iit{false};
    bool isMultiIntegerValued() const { return iit; };
    void setIsMultiIntegerValued(bool b) { iit = b; };

    bool iam{false};
    bool isAlwaysAccessibleMomentary() const { return iam; }
    void setIsAlwaysAccessibleMomentary(bool b) { iam = b; }

    int iv{0}, im{1};
    void setIntegerValue(int i) { iv = i; }
    void setIntegerMax(int i) { im = i; }
    int getIntegerValue() const { return iv; }
    int getIntegerMaxValue() const { return im; }

    bool uvc{false};
    void setUnValueClickable(bool b) { uvc = b; }
    bool getUnValueClickable() const { return uvc; }

    int vd{0};
    int getValueDirection() const { return vd; }
    void setValueDirection(int v) { vd = v; }
    void clearValueDirection() { vd = 0; }

    float value{0};
    float getValue() const override { return value; }
    void setValue(float f) override { value = f; }

    void paint(juce::Graphics &g) override;
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;

    void endHover() override
    {
        if (stuckHover)
            return;

        isHovered = false;
        repaint();
    }

    void startHover(const juce::Point<float> &) override
    {
        isHovered = true;
        repaint();
    }

    bool isCurrentlyHovered() override { return isHovered; }

    bool keyPressed(const juce::KeyPress &key) override;

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        startHover(getBounds().getBottomLeft().toFloat());
    }

    void focusLost(juce::Component::FocusChangeType cause) override { endHover(); }

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    Surge::GUI::WheelAccumulationHelper wheelHelper;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    bool isHovered{false};

    SurgeImage *switchD{nullptr}, *hoverSwitchD{nullptr};
    void setSwitchDrawable(SurgeImage *d) { switchD = d; }
    void setHoverSwitchDrawable(SurgeImage *d) { hoverSwitchD = d; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isDeactivated{false};
    void setDeactivated(bool b) { isDeactivated = b; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Switch);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_SWITCH_H
