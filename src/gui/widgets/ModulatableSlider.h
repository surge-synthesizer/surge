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

#ifndef SURGE_XT_MODULATABLESLIDER_H
#define SURGE_XT_MODULATABLESLIDER_H

#include <JuceHeader.h>
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "ModulatableControlInterface.h"
#include "SurgeJUCEHelpers.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct ModulatableSlider : public juce::Component,
                           public WidgetBaseMixin<ModulatableSlider>,
                           public ModulatableControlInterface
{
    ModulatableSlider();
    ~ModulatableSlider();

    enum MoveRateState
    {
        kUnInitialized = 0,
        kLegacy,
        kSlow,
        kMedium,
        kExact
    };
    static MoveRateState sliderMoveRateState;

    /**
     * The slider is 'light' backgrounded (which matters in the classic skin only)
     * @param b
     */
    virtual void setIsLightStyle(bool b) { isLightStyle = b; }
    bool isLightStyle{false};

    /**
     * Vertical sliders have smaller vertical footprints in the LFO region, as a remnant from
     * the classic skin. Support the option of intent here even though subclasses may do nothing
     * with this
     *
     * @param b
     */
    virtual void setIsMiniVertical(bool b) { isMiniVertical = b; }
    bool isMiniVertical{false};

    Surge::ParamConfig::Orientation orientation;
    void setOrientation(Surge::ParamConfig::Orientation o) { orientation = o; }

    void paint(juce::Graphics &g) override;

    bool isHovered{false};
    float valueOnMouseDown{0.f}, modValueOnMouseDown{0.f};
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseDoubleClick(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    /*
     * This sets up the legacy mouse -> value scaling
     */
    float legacyMoveRate{1.0};
    virtual void setMoveRate(float f) { legacyMoveRate = f; }

    float value{0.f};
    float getValue() const override { return value; }
    void setValue(float f) override
    {
        value = f;
        repaint();
    }

    Surge::GUI::IComponentTagValue *asControlValueInterface() override { return this; }
    juce::Component *asJuceComponent() override { return this; }

    void onSkinChanged() override;

    juce::Drawable *pTray, *pHandle, *pHandleHover, *pTempoSyncHandle, *pTempoSyncHoverHandle;

  private:
    // I know right Here I am using private!

    /*
     * These are the state variables which determine the position of the items in the graphics
     * for a given draw or mouse gesture. I made them members so I don't have to copy their
     * calculation. The method updateLocationState() updates based on current state of the widget
     */
    void updateLocationState();

    int trayw{0}, trayh{0}; // How large is the tray
    float range{0};         // how big in logical pixels is the
    // What are the XY positions for the handle center, the mod handle center, the
    // other end of the bar.
    float handleCX{0}, handleCY{0};
    float handleMX{0}, handleMY{0};
    float barNMX{0}, barNMY{0};
    float handleX0{0}, handleY0{0};

    // how far logically do we move in tray space for our curent state
    int trayTypeX{0}, trayTypeY{0};
    // How far do we move in the image to make the handle image be the mod
    float modHandleX{0};

    // Should we draw the label, and where?
    bool drawLabel{false};
    juce::Rectangle<int> labelRect;
    // How big is the handle
    juce::Rectangle<int> handleSize;
    // Where do we position the tray relative to local bounds?
    juce::AffineTransform trayPosition;
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_MODULATABLESLIDER_H
