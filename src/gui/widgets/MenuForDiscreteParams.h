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

#ifndef SURGE_XT_MENUASMODULATABLEINTERFACE_H
#define SURGE_XT_MENUASMODULATABLEINTERFACE_H

#include <JuceHeader.h>
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "ModulatableControlInterface.h"
#include "SurgeJUCEHelpers.h"
#include <vector>

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
                               public ModulatableControlInterface

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

    juce::Drawable *dragGlyph{nullptr}, *dragGlyphHover{nullptr};
    int dragGlyphBoxSize{0};
    enum GlyphLocation
    {
        ABOVE,
        BELOW,
        LEFT,
        RIGHT
    } glyphLocation = LEFT;
    void setDragGlyph(juce::Drawable *d, int boxSize)
    {
        dragGlyph = d;
        dragGlyphBoxSize = boxSize;
    }
    void setDragGlyphHover(juce::Drawable *d) { dragGlyphHover = d; }
    juce::Rectangle<float> glyphPosition;

    void setGlyphMode(bool b) { glyphMode = b; }
    bool glyphMode{false};

    void setBackgroundDrawable(juce::Drawable *d) { bg = d; }
    void setHoverBackgroundDrawable(juce::Drawable *d) { bghover = d; }
    juce::Drawable *bg{nullptr}, *bghover{nullptr};

    void paint(juce::Graphics &g) override;
    bool isHovered{false};
    void mouseEnter(const juce::MouseEvent &e) override
    {
        isHovered = true;
        repaint();
    }
    void mouseExit(const juce::MouseEvent &e) override
    {
        isHovered = false;
        repaint();
    }
    juce::Point<int> mouseDownOrigin;
    bool isDraggingGlyph{false};
    float lastDragDistance{0};
    void mouseDown(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;

    void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w) override;
    Surge::GUI::WheelAccumulationHelper wheelAccumulationHelper;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    float nextValueInOrder(float v, int inc);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MENUASMODULATABLEINTERFACE_H
