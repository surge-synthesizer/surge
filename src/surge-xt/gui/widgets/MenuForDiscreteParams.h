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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUFORDISCRETEPARAMS_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MENUFORDISCRETEPARAMS_H

#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "ModulatableControlInterface.h"
#include "SurgeJUCEHelpers.h"
#include "AccessibleHelpers.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <vector>

class SurgeImage;
class SurgeStorage;

namespace Surge
{
namespace Widgets
{
/*
 * This class (fka CMenuAsSlider) is a menu-like object (so used for discrete values)
 * which implements the ModulatableControlInterface so it can sub in for discrete sliders.
 * It also can stand alone as it does for the filter selectors.
 */
struct MenuForDiscreteParams : public juce::Component,
                               public WidgetBaseMixin<MenuForDiscreteParams>,
                               public LongHoldMixin<MenuForDiscreteParams>,
                               public ModulatableControlInterface,
                               public HasExtendedAccessibleGroupName

{
    MenuForDiscreteParams();
    ~MenuForDiscreteParams() override;

    Surge::GUI::IComponentTagValue *asControlValueInterface() override { return this; }
    juce::Component *asJuceComponent() override { return this; }

    float value{0.f};
    void setValue(float v) override { value = v; }
    float getValue() const override { return value; }

    int iMin{0}, iMax{1};
    void setMinMax(int n, int x)
    {
        iMin = n;
        iMax = x;
    }
    std::vector<int> intOrdering;
    void setIntOrdering(const std::vector<int> &ordering) { intOrdering = ordering; }

    juce::Rectangle<int> dragRegion;
    void setDragRegion(const juce::Rectangle<int> &d) { dragRegion = d; }

    std::vector<std::pair<int, int>> gli;
    void addGlyphIndexMapEntry(int a, int b) { gli.emplace_back(a, b); }

    SurgeImage *dragGlyph{nullptr}, *dragGlyphHover{nullptr};
    int dragGlyphBoxSize{0};
    enum GlyphLocation
    {
        ABOVE,
        BELOW,
        LEFT,
        RIGHT
    } glyphLocation = LEFT;

    void setDragGlyph(SurgeImage *d, int boxSize)
    {
        dragGlyph = d;
        dragGlyphBoxSize = boxSize;
    }

    void setDragGlyphHover(SurgeImage *d) { dragGlyphHover = d; }

    juce::Rectangle<float> glyphPosition;
    bool glyphMode{false};
    SurgeImage *bg{nullptr}, *bghover{nullptr};

    void setGlyphMode(bool b) { glyphMode = b; }
    void setBackgroundDrawable(SurgeImage *d) { bg = d; }
    void setHoverBackgroundDrawable(SurgeImage *d) { bghover = d; }

    void paint(juce::Graphics &g) override;

    bool isHovered{false};

    void mouseEnter(const juce::MouseEvent &event) override
    {
        isHovered = true;
        repaint();
    }

    void mouseMove(const juce::MouseEvent &event) override
    {
        if (glyphMode && glyphPosition.contains(event.position))
        {
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    }

    void mouseExit(const juce::MouseEvent &event) override { endHover(); }

    void startHover(const juce::Point<float> &p) override
    {
        isHovered = true;
        repaint();
    }

    void endHover() override
    {
        if (stuckHover)
            return;

        isHovered = false;

        if (glyphMode)
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        repaint();
    }

    bool isCurrentlyHovered() override { return isHovered; }

    bool keyPressed(const juce::KeyPress &key) override;

    bool rebuildOnFocus = false;
    void focusGained(juce::Component::FocusChangeType cause) override
    {
        rebuildOnFocus = false;
        startHover(getBounds().getBottomLeft().toFloat());
    }

    void focusLost(juce::Component::FocusChangeType cause) override;

    juce::Point<int> mouseDownOrigin;
    bool isDraggingGlyph{false};
    float lastDragDistance{0};
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    float nextValueInOrder(float v, int inc);

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuForDiscreteParams);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MENUASMODULATABLEINTERFACE_H
