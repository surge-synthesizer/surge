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

#include "ModulatableSlider.h"
#include "SurgeGUIEditor.h"
#include "SurgeBitmaps.h"
#include "basic_dsp.h"

namespace Surge
{
namespace Widgets
{

ModulatableSlider::MoveRateState ModulatableSlider::sliderMoveRateState =
    ModulatableSlider::MoveRateState::kUnInitialized;
ModulatableSlider::ModulatableSlider() { setRepaintsOnMouseActivity(true); }

ModulatableSlider::~ModulatableSlider() {}

void ModulatableSlider::updateLocationState()
{
    /*
     * The underlying image contains multiple types in a grid for the tray
     */
    trayTypeX = 0;
    trayTypeY = 0;
    if (orientation == ParamConfig::kHorizontal)
    {
        if (isSemitone)
            trayTypeY = 2;
        else if (isBipolarFn())
            trayTypeY = 1;
        if (isLightStyle)
            trayTypeY += 3;
    }
    else
    {
        if (isBipolarFn())
            trayTypeY = 1;
        if (isMiniVertical)
            trayTypeY = 2;
    }

    switch (modulationState)
    {
    case ModulatableControlInterface::UNMODULATED:
        trayTypeX = 0;
        break;
    case ModulatableControlInterface::MODULATED_BY_OTHER:
        trayTypeX = 1;
        break;
    case ModulatableControlInterface::MODULATED_BY_ACTIVE:
        trayTypeX = 2;
        break;
    }

    if (orientation == ParamConfig::kVertical)
    {
        trayPosition = juce::AffineTransform().translated(2, 2);
        trayw = 17;
        trayh = 75;
        range = isMiniVertical ? 39 : 56;

        handleX0 = 0;
        handleY0 = 9;

        handleCX = 7.5;
        handleMX = handleCX;
        barNMX = handleMX;
        modHandleX = 24;

        handleCY = quantizedDisplayValue * range + handleY0;
        // When modulating and this is relevant, value == QDV always
        handleMY = limit01(value + modValue) * range + handleY0;
        barNMY = limit01(value - modValue) * range + handleY0;
        labelRect = juce::Rectangle<int>();
        handleSize = juce::Rectangle<int>().withWidth(15).withHeight(20);
    }
    else
    {
        trayPosition = juce::AffineTransform().translated(2, 5);
        trayw = 133;
        trayh = 14;
        range = 112;

        handleX0 = (trayw - range) * 0.5;
        handleY0 = 0;

        handleCX = range * quantizedDisplayValue + handleX0;
        handleMX = range * limit01(value + modValue) + handleX0;
        barNMX = range * limit01(value - modValue) + handleX0;

        handleCY = 7;
        handleMY = handleCY;
        barNMY = handleMY;
        modHandleX = 28;
        drawLabel = true;
        // This calculation is a little dicey but is correct
        labelRect = juce::Rectangle<int>()
                        .withX(handleX0)
                        .withY(trayh / 2)
                        .withHeight(trayh)
                        .withWidth(range)
                        .withTrimmedRight(1)
                        .withTrimmedLeft(1);
        handleSize = juce::Rectangle<int>().withWidth(20).withHeight(15);
    }

    if (skin && skinControl &&
        skin->propertyValue(skinControl, Surge::Skin::Component::HIDE_SLIDER_LABEL, "") == "true")
    {
        drawLabel = false;
    }
}

void ModulatableSlider::paint(juce::Graphics &g)
{
    jassert(pTray);
    if (!pTray)
        return;

    float activationOpacity = 1.0;
    if ((hasDeactivatedFn && isDeactivatedFn()) || isDeactivated)
    {
        activationOpacity = 0.35;
    }
    updateLocationState();

    // Draw the tray
    {
        juce::Graphics::ScopedSaveState gs(g);
        auto t = juce::AffineTransform();
        t = t.translated(-trayTypeX * trayw, -trayTypeY * trayh);

        g.addTransform(trayPosition);
        g.reduceClipRegion(0, 0, trayw, trayh);
        pTray->draw(g, activationOpacity, t);
    }

    // Draw the modulation bar
    if (isEditingModulation)
    {
        juce::Graphics::ScopedSaveState gs(g);

        g.addTransform(trayPosition);
        // FIXME skin colors
        g.setColour(juce::Colours::red);
        g.drawLine(barNMX, barNMY, handleMX, handleMY, 2);
    }
    // Draw the label
    if (drawLabel)
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(trayPosition);
        auto tl = label;
        if (hasLabelFn)
        {
            tl = labelFn();
        }
        // FIXME FONT AND JUSTIFICATION
        g.setFont(font);
        if (isLightStyle)
            g.setColour(skin->getColor(Colors::Slider::Label::Light));
        else
            g.setColour(skin->getColor(Colors::Slider::Label::Dark));
        g.drawText(tl, labelRect, juce::Justification::top | juce::Justification::right);
    }

    // Draw the handles
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(trayPosition);

        auto q = handleSize.withCentre(juce::Point<int>(handleCX, handleCY));
        auto moveTo = juce::AffineTransform().translated(q.getTopLeft());
        auto t = juce::AffineTransform().translated(-1, -1);
        g.addTransform(moveTo);
        g.reduceClipRegion(handleSize);
        pHandle->draw(g, activationOpacity, t);

        if (isHovered && pHandleHover)
            pHandleHover->draw(g, activationOpacity, t);

        if (pTempoSyncHandle && isTemposync)
            pTempoSyncHandle->draw(g, activationOpacity, t);

        if (isHovered && isTemposync && pTempoSyncHoverHandle)
            pTempoSyncHoverHandle->draw(g, activationOpacity, t);
    }

    if (isEditingModulation)
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(trayPosition);

        auto q = handleSize.withCentre(juce::Point<int>(handleMX, handleMY));
        auto moveTo = juce::AffineTransform().translated(q.getTopLeft());
        auto t = juce::AffineTransform().translated(-1 - modHandleX, -1);
        g.addTransform(moveTo);
        g.reduceClipRegion(handleSize);
        pHandle->draw(g, activationOpacity, t);
    }
}
void ModulatableSlider::onSkinChanged()
{
    if (orientation == ParamConfig::kHorizontal)
    {
        pTray = associatedBitmapStore->getDrawable(IDB_SLIDER_HORIZ_BG);
        pHandle = associatedBitmapStore->getDrawable(IDB_SLIDER_HORIZ_HANDLE);
        pHandleHover = associatedBitmapStore->getDrawableByStringID(
            skin->hoverImageIdForResource(IDB_SLIDER_HORIZ_HANDLE, GUI::Skin::HOVER));
        jassert(pHandleHover);
        pTempoSyncHandle =
            associatedBitmapStore->getDrawableByStringID("TEMPOSYNC_HORIZONTAL_OVERLAY");
        pTempoSyncHoverHandle =
            associatedBitmapStore->getDrawableByStringID("TEMPOSYNC_HORIZONTAL_HOVER_OVERLAY");
    }
    else
    {
        pTray = associatedBitmapStore->getDrawable(IDB_SLIDER_VERT_BG);
        pHandle = associatedBitmapStore->getDrawable(IDB_SLIDER_VERT_HANDLE);
        pHandleHover = associatedBitmapStore->getDrawableByStringID(
            skin->hoverImageIdForResource(IDB_SLIDER_VERT_HANDLE, GUI::Skin::HOVER));

        pTempoSyncHandle =
            associatedBitmapStore->getDrawableByStringID("TEMPOSYNC_VERTICAL_OVERLAY");
        pTempoSyncHoverHandle =
            associatedBitmapStore->getDrawableByStringID("TEMPOSYNC_VERTICAL_HOVER_OVERLAY");
    }

    if (skinControl.get())
    {
        auto htr = skin->propertyValue(skinControl, Surge::Skin::Component::SLIDER_TRAY);
        if (htr.isJust())
        {
            pTray = associatedBitmapStore->getDrawableByStringID(htr.fromJust());
        }
        auto hi = skin->propertyValue(skinControl, Surge::Skin::Component::HANDLE_IMAGE);
        if (hi.isJust())
        {
            pHandle = associatedBitmapStore->getDrawableByStringID(hi.fromJust());
        }
        auto ho = skin->propertyValue(skinControl, Surge::Skin::Component::HANDLE_HOVER_IMAGE);
        if (ho.isJust())
        {
            pHandleHover = associatedBitmapStore->getDrawableByStringID(ho.fromJust());
        }
        auto ht = skin->propertyValue(skinControl, Surge::Skin::Component::HANDLE_TEMPOSYNC_IMAGE);
        if (ht.isJust())
        {
            pTempoSyncHandle = associatedBitmapStore->getDrawableByStringID(ht.fromJust());
        }

        auto hth =
            skin->propertyValue(skinControl, Surge::Skin::Component::HANDLE_TEMPOSYNC_HOVER_IMAGE);
        if (hth.isJust())
        {
            pTempoSyncHoverHandle = associatedBitmapStore->getDrawableByStringID(hth.fromJust());
        }
    }

#if I_HAVE_A_SKIN_API_FOR_THIS_AT_DRAWABLE_LEVEL
    if (!pHandleHover)
        pHandleHover = skin->hoverBitmapOverlayForBackgroundBitmap(
            skinControl, dynamic_cast<CScalableBitmap *>(pHandle), associatedBitmapStore,
            Surge::GUI::Skin::HoverType::HOVER);
#endif
}
void ModulatableSlider::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->sliderHoverStart(getTag());
    }
}
void ModulatableSlider::mouseExit(const juce::MouseEvent &event)
{
    isHovered = false;
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->sliderHoverEnd(getTag());
    }
}
void ModulatableSlider::mouseDrag(const juce::MouseEvent &event)
{
    int distance = event.getDistanceFromDragStartX();
    if (orientation == ParamConfig::kVertical)
        distance = event.getDistanceFromDragStartY();

    if (distance == 0 && editTypeWas == NOEDIT)
        return;

    if (editTypeWas == NOEDIT)
    {
        notifyBeginEdit();
    }
    editTypeWas = DRAG;
    updateLocationState();

    float dMouse = 1.f / range;
    if (event.mods.isShiftDown())
        dMouse = dMouse * 0.1;

    switch (sliderMoveRateState)
    {
    case kExact:
        break;
    case kSlow:
        dMouse *= 0.3;
        break;
    case kMedium:
        dMouse *= 0.7;
        break;
    case kLegacy:
    default:
        dMouse *= legacyMoveRate;
        break;
    }

    if (isEditingModulation)
    {
        modValue = limitpm1(modValueOnMouseDown + dMouse * distance);
    }
    else
    {
        value = limit01(valueOnMouseDown + dMouse * distance);
    }
    std::cout << "DRAG NVC " << distance << std::endl;
    notifyValueChanged();
    repaint();
}
void ModulatableSlider::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isPopupMenu())
    {
        notifyControlModifierClicked(event.mods);
        return;
    }

    valueOnMouseDown = value;
    modValueOnMouseDown = modValue;
    editTypeWas = NOEDIT;
}
void ModulatableSlider::mouseUp(const juce::MouseEvent &event)
{
    if (editTypeWas != DRAG)
        return;

    if (event.mods.isAltDown())
    {
        if (isEditingModulation)
            modValue = modValueOnMouseDown;
        else
            value = valueOnMouseDown;
        notifyValueChanged();
    }

    notifyEndEdit();
}
void ModulatableSlider::mouseDoubleClick(const juce::MouseEvent &event)
{
    editTypeWas = DOUBLECLICK;
    notifyBeginEdit();
    notifyControlModifierDoubleClicked(event.mods);
    notifyEndEdit();
    repaint();
}
void ModulatableSlider::mouseWheelMove(const juce::MouseEvent &event,
                                       const juce::MouseWheelDetails &wheel)
{
    /*
     * If I choose based on horiz/vert it only works ontrackpads so just add
     */
    float delta = (wheel.isReversed ? 1 : -1) * wheel.deltaX - wheel.deltaY;
    if (delta == 0)
        return;

    float speed = 1.2;
    if (event.mods.isShiftDown())
        speed = .12;

    delta *= speed;

    editTypeWas = WHEEL;
    notifyBeginEdit();
    if (isEditingModulation)
        modValue = limitpm1(modValue + delta);
    else
        value = limit01(value + delta);
    notifyValueChanged();
    notifyEndEdit();
    repaint();
}
} // namespace Widgets
} // namespace Surge