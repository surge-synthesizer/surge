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

#include "ModulationSourceButton.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SurgeImageStore.h"
#include "basic_dsp.h"
#include "SurgeGUIEditor.h"
#include "SurgeImage.h"
#include "SurgeGUIUtils.h"
#include "SurgeJUCEHelpers.h"
#include "AccessibleHelpers.h"
#include "widgets/MenuCustomComponents.h"

namespace Surge
{
namespace Widgets
{
ModulationSourceButton::ModulationSourceButton()
    : juce::Component(), WidgetBaseMixin<ModulationSourceButton>(this)
{

    setDescription("Modulator");
    setAccessible(true);
    setFocusContainerType(FocusContainerType::focusContainer);

    auto ol = std::make_unique<OverlayAsAccessibleButton<ModulationSourceButton>>(
        this, "Select", juce::AccessibilityRole::button);
    ol->onPress = [this](auto *t) {
        mouseMode = CLICK_SELECT_ONLY;
        notifyValueChanged();
    };
    ol->onReturnKey = [this](auto *t) {
        mouseMode = CLICK_SELECT_ONLY;
        notifyValueChanged();
        return true;
    };
    addChildComponent(*ol);
    selectAccButton = std::move(ol);

    ol = std::make_unique<OverlayAsAccessibleButton<ModulationSourceButton>>(
        this, "Arm", juce::AccessibilityRole::button);
    ol->onPress = [this](auto *t) {
        mouseMode = CLICK_TOGGLE_ARM;
        notifyValueChanged();
    };
    ol->onReturnKey = [this](auto *t) {
        mouseMode = CLICK_TOGGLE_ARM;
        notifyValueChanged();
        return true;
    };
    addChildComponent(*ol);
    toggleArmAccButton = std::move(ol);

    ol = std::make_unique<OverlayAsAccessibleButton<ModulationSourceButton>>(
        this, "Target", juce::AccessibilityRole::button);
    ol->onPress = [this](auto *t) {
        mouseMode = CLICK_ARROW;
        notifyValueChanged();
    };
    ol->onReturnKey = [this](auto *t) {
        mouseMode = CLICK_ARROW;
        notifyValueChanged();
        return true;
    };
    addChildComponent(*ol);
    targetAccButton = std::move(ol);

    auto os =
        std::make_unique<OverlayAsAccessibleSlider<ModulationSourceButton>>(this, "macro value");
    os->onGetValue = [this](auto *t) { return value; };
    os->onValueToString = [this](auto *t, float f) {
        auto v = f;
        if (isBipolar)
            v = v * 2 - 1;
        return fmt::format("{:.3f}", v);
    };
    os->onSetValue = [this](auto *t, float f) {
        value = limit01(f);
        mouseMode = DRAG_VALUE;

        notifyBeginEdit();
        notifyValueChanged();
        notifyEndEdit();
        repaint();
        if (auto h = t->getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
    };
    os->onMinMaxDef = [this](auto *t, int mmd) {
        if (mmd == 1)
            value = 1;
        if (mmd == -1)
            value = 0;
        if (mmd == 0)
            value = (isBipolar ? 0.5 : 0);

        mouseMode = DRAG_VALUE;

        notifyBeginEdit();
        notifyValueChanged();
        notifyEndEdit();
        repaint();
        if (auto h = t->getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
    };
    os->onJogValue = [this](auto *t, int dir, bool isShift, bool isControl) {
        auto delt = 0.05f;
        if (isShift)
            delt = delt * 0.1;
        if (dir < 0)
            delt *= -1;
        value = limit01(value + delt);
        mouseMode = DRAG_VALUE;

        notifyBeginEdit();
        notifyValueChanged();
        notifyEndEdit();
        repaint();
        if (auto h = t->getAccessibilityHandler())
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
    };
    addChildComponent(*os);
    macroSlider = std::move(os);
}
void ModulationSourceButton::paint(juce::Graphics &g)
{
    /*
     * state
     * 0 - nothing
     * 1 - selected modeditor
     * 2 - selected modsource (locked)
     * 4 [bit 2] - selected arrow button [0, 1, 2 -> 4, 5, 6]
     */

    bool SelectedModSource = (state & 3) == 1;
    bool ActiveModSource = (state & 3) == 2;
    bool UsedOrActive = isUsed || (state & 3);

    juce::Colour FrameCol, FillCol, FontCol;

    FillCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Background)
                           : skin->getColor(Colors::ModSource::Unused::Background);
    FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Border)
                            : skin->getColor(Colors::ModSource::Unused::Border);
    FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::Text)
                           : skin->getColor(Colors::ModSource::Unused::Text);
    if (isHovered)
    {
        FrameCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::BorderHover)
                                : skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = UsedOrActive ? skin->getColor(Colors::ModSource::Used::TextHover)
                               : skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    if (ActiveModSource || transientArmed)
    {
        FrameCol = skin->getColor(Colors::ModSource::Armed::Border);
        if (isHovered)
        {
            FrameCol = skin->getColor(Colors::ModSource::Armed::BorderHover);
        }

        FillCol = skin->getColor(Colors::ModSource::Armed::Background);
        FontCol = skin->getColor(Colors::ModSource::Armed::Text);

        if (isHovered)
        {
            FontCol = skin->getColor(Colors::ModSource::Armed::TextHover);
        }
    }
    else if (SelectedModSource)
    {
        FrameCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Border)
                          : skin->getColor(Colors::ModSource::Selected::Border);
        if (isHovered)
        {
            FrameCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::BorderHover)
                              : skin->getColor(Colors::ModSource::Selected::BorderHover);
        }

        FillCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Background)
                         : skin->getColor(Colors::ModSource::Selected::Background);
        FontCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::Text)
                         : skin->getColor(Colors::ModSource::Selected::Text);

        if (isHovered)
        {
            FontCol = isUsed ? skin->getColor(Colors::ModSource::Selected::Used::TextHover)
                             : skin->getColor(Colors::ModSource::Selected::TextHover);
        }
    }
    else if (isTinted)
    {
        FillCol = skin->getColor(Colors::ModSource::Unused::Background);
        FrameCol = skin->getColor(Colors::ModSource::Unused::Border);

        if (isHovered)
        {
            FrameCol = skin->getColor(Colors::ModSource::Unused::BorderHover);
        }

        FontCol = skin->getColor(Colors::ModSource::Unused::Text);

        if (isHovered)
        {
            FontCol = skin->getColor(Colors::ModSource::Unused::TextHover);
        }
    }

    if (secondaryHoverActive)
    {
        FontCol = skin->getColor(Colors::ModSource::Used::UsedModHover);

        if (ActiveModSource)
        {
            FontCol = skin->getColor(Colors::ModSource::Armed::UsedModHover);
        }

        if (SelectedModSource)
        {
            FontCol = skin->getColor(Colors::ModSource::Selected::UsedModHover);
        }
    }

    auto fillRect = getLocalBounds();
    fillRect.reduce(1, 1);
    g.setColour(FillCol);
    g.fillRect(fillRect);

    if (!isMeta)
    {
        // modbutton name settings
        g.setColour(FontCol);
        g.setFont(font);

        // modbutton name
        g.drawText(getCurrentModLabel(), getLocalBounds(), juce::Justification::centred);

        // modbutton frame
        g.setColour(FrameCol);
        g.drawRect(getLocalBounds(), 1);
    }
    else
    {
        // macro name area
        auto topRect = getLocalBounds().withHeight(splitHeight);

        g.setColour(FontCol);
        g.setFont(font);
        g.drawText(getCurrentModLabel(), topRect, juce::Justification::centred);

        // macro slider area
        auto bottomRect = fillRect.withTrimmedTop(splitHeight - 2);

        // macro slider value fill
        auto valRect = bottomRect.toFloat();
        valRect.reduce(1, 1);

        auto sliderWidth = valRect.getWidth();

        // current value
        auto valPoint = value * sliderWidth;

        // macro slider background
        g.setColour(skin->getColor(Colors::ModSource::Macro::Background));
        g.fillRect(valRect);

        // macro slider frame
        g.setColour(skin->getColor(Colors::ModSource::Macro::Border));
        g.drawRect(bottomRect);

        if (isBipolar)
        {
            auto bipolarCenter = valRect.getCentreX();

            if (valPoint + 2 <= bipolarCenter)
            {
                valRect = valRect.withLeft(valPoint + 2).withRight(bipolarCenter + 1);
            }
            else
            {
                valRect = valRect.withLeft(bipolarCenter).withRight(valPoint + 2);
            }
        }
        else
        {
            // make it so that the slider doesn't draw over its frame, but within it
            valRect.setX(2);
            valRect.setWidth(valPoint);
        }

        // draw macro slider value fill
        g.setColour(skin->getColor(Colors::ModSource::Macro::Fill));
        g.fillRect(valRect);

        // draw current value notch
        valPoint = (valPoint == sliderWidth) ? sliderWidth - 1 : valPoint;

        g.setColour(skin->getColor(Colors::ModSource::Macro::CurrentValue));
        g.fillRect(juce::Rectangle<float>(2 + valPoint, valRect.getY(), 1, valRect.getHeight()));

        // draw modbutton frame
        g.setColour(FrameCol);
        g.drawRect(getLocalBounds(), 1);
    }

    if (isLFO())
    {
        auto arrSze = getLocalBounds().withLeft(getLocalBounds().getRight() - 14).withHeight(16);
        float dy = (state >= 4) ? -16 : 0;

        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(arrSze);

        arrow->drawAt(g, arrSze.getX(), arrSze.getY() + dy, 1.0);
    }

    if (needsHamburger())
    {
        g.setColour(FrameCol);

        float ht = 1.5;
        int nburg = 3;
        float pxspace = 1.f * (hamburgerHome.getHeight() - ht) / (nburg);

        for (int i = 0; i < nburg; ++i)
        {
            auto r = juce::Rectangle<float>(hamburgerHome.getX() + 1,
                                            ht + hamburgerHome.getY() + i * pxspace,
                                            hamburgerHome.getWidth() - 1, ht);
            g.fillRect(r);
        }
    }
}

void ModulationSourceButton::buildHamburgerMenu(juce::PopupMenu &menu,
                                                const bool addedToModbuttonContextMenu)
{
    int idx{0};
    std::string hu;
    auto modsource = getCurrentModSource();
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (modsource >= ms_ctrl1 && modsource <= ms_ctrl8)
    {
        hu = sge->helpURLForSpecial("macro-modbutton");
    }
    else if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
    {
        hu = sge->helpURLForSpecial("lfo-modbutton");
    }
    else if ((modsource >= ms_ampeg && modsource <= ms_filtereg) ||
             (modsource >= ms_random_bipolar && modsource <= ms_alternate_unipolar))
    {
        hu = sge->helpURLForSpecial("internalmod-modbutton");
    }
    else
    {
        hu = sge->helpURLForSpecial("other-modbutton");
    }

    if (!addedToModbuttonContextMenu)
    {
        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            sge->addHelpHeaderTo(
                ModulatorName::modulatorName(storage, modsource, false, sge->current_scene), lurl,
                menu);
        }
        else
        {
            menu.addItem(
                ModulatorName::modulatorName(storage, modsource, false, sge->current_scene),
                []() {});
        }

        menu.addSeparator();
    }

    if (modlist.size() > 1)
    {
        for (auto e : modlist)
        {
            auto modName = std::get<3>(e);

            if (this->modlistIndex != idx || !addedToModbuttonContextMenu)
            {
                bool ticked = !addedToModbuttonContextMenu && this->modlistIndex == idx;
                menu.addItem(modName, true, ticked, [this, sge, idx]() {
                    sge->forceLfoDisplayRepaint();
                    this->modlistIndex = idx;

                    int lfo_id = getCurrentModSource() - ms_lfo1;
                    storage->getPatch()
                        .dawExtraState.editor
                        .modulationSourceButtonState[sge->current_scene][lfo_id]
                        .index = this->modlistIndex;

                    mouseMode = HAMBURGER;
                    notifyValueChanged();
                    mouseMode = NONE;
                    repaint();
                });
            }

            idx++;
        }

        if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
        {
            auto &lf =
                sge->getStorage()->getPatch().scene[sge->current_scene].lfo[modsource - ms_lfo1];

            auto sh = lf.shape.val.i;
            if (sh != lt_formula)
            {
                menu.addSeparator();
                bool sc = lf.lfoExtraAmplitude == LFOStorage::SCALED;
                menu.addItem(Surge::GUI::toOSCase("Amplitude Parameter Applies to ") +
                                 Surge::GUI::toOSCase("Raw and EG Outputs"),
                             true, sc, [sge, modsource] {
                                 auto &lf = sge->getStorage()
                                                ->getPatch()
                                                .scene[sge->current_scene]
                                                .lfo[modsource - ms_lfo1];

                                 if (lf.lfoExtraAmplitude == LFOStorage::SCALED)
                                     lf.lfoExtraAmplitude = LFOStorage::UNSCALED;
                                 else
                                     lf.lfoExtraAmplitude = LFOStorage::SCALED;

                                 sge->forceLFODisplayRebuild();
                             });
            }
        }
    }
}

void ModulationSourceButton::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    mouseMode = CLICK;
    everDragged = false;
    mouseDownLocation = event.position;

    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge && sge->getSelectedModsource() != this->getCurrentModSource())
    {
        newlySelected = 2;
    }
    else
    {
        if (newlySelected > 0)
            newlySelected--;
    }

    mouseDownLongHold(event);

    if (needsHamburger() && hamburgerHome.contains(event.position.toInt()))
    {
        auto menu = juce::PopupMenu();

        buildHamburgerMenu(menu, false);

        mouseMode = HAMBURGER;

        auto sge = firstListenerOfType<SurgeGUIEditor>();
        menu.showMenuAsync(sge->popupMenuOptions(this), Surge::GUI::makeEndHoverCallback(this));
        return;
    }

    if (event.mods.isPopupMenu())
    {
        mouseMode = NONE;
        notifyControlModifierClicked(event.mods);

        return;
    }

    if (isMeta)
    {
        // match the mouseable area to the painted macro slider area (including slider frame)
        auto frameRect = getLocalBounds();
        frameRect.reduce(1, 1);
        auto bottomRect = frameRect.withTrimmedTop(splitHeight - 2);

        if (bottomRect.contains(event.position.toInt()))
        {
            notifyBeginEdit();
            mouseMode = PREDRAG_VALUE;
            return;
        }
    }

    if (isLFO())
    {
        auto arrSze = getLocalBounds().withLeft(getLocalBounds().getRight() - 14).withHeight(16);

        if (arrSze.contains(event.position.toInt()))
        {
            mouseMode = CLICK_ARROW;
        }
    }

    if (event.mods.getCurrentModifiers().isCtrlDown())
    {
        mouseMode = CTRL_CLICK;
    }

    mouseDownBounds = getBounds();
    componentDragger.startDraggingComponent(this, event);
}

void ModulationSourceButton::mouseDoubleClick(const juce::MouseEvent &event)
{
    if (isMeta)
    {
        auto topRect = getLocalBounds().withHeight(splitHeight);

        // rename macro on double-click
        if (topRect.contains(event.position.toInt()) && !newlySelected)
        {
            auto ccid = (int)getCurrentModSource() - ms_ctrl1;
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            sge->openMacroRenameDialog(ccid, topRect.getTopLeft(), this);

            return;
        }

        // match the mouseable area to the painted macro slider area (including slider frame)
        auto frameRect = getLocalBounds();
        frameRect.reduce(1, 1);
        auto bottomRect = frameRect.withTrimmedTop(splitHeight - 2);

        if (bottomRect.contains(event.position.toInt()))
        {
            value = isBipolar ? 0.5f : 0.f;

            mouseMode = DRAG_VALUE;
            notifyBeginEdit();
            notifyValueChanged();
            notifyEndEdit();
            repaint();
            mouseMode = NONE;

            return;
        }
    }
    else
    {
        auto rect = getLocalBounds();

        // rename LFO on double-click
        if (isLFO() && rect.contains(event.position.toInt()) && !newlySelected)
        {
            int lfo_id = getCurrentModSource() - ms_lfo1;
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            // See #5774 for why this is commented out
            sge->openLFORenameDialog(lfo_id, rect.getTopLeft(), this);

            return;
        }
    }
}

void ModulationSourceButton::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    repaint();
}

void ModulationSourceButton::mouseExit(const juce::MouseEvent &event) { endHover(); }

void ModulationSourceButton::startHover(const juce::Point<float> &f)
{
    isHovered = true;
    repaint();
}

void ModulationSourceButton::endHover()
{
    if (stuckHover)
        return;

    bool oh = isHovered;
    isHovered = false;

    if (oh != isHovered)
    {
        repaint();
    }
}

bool ModulationSourceButton::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu)
    {
        notifyControlModifierClicked(juce::ModifierKeys(), true);
        return true;
    }

    if (action != Increase && action != Decrease)
        return false;

    if (isMeta)
    {
        float delta = 0.05;
        if (mod == Fine)
        {
            delta = 0.01;
        }
        if (action == Decrease)
            delta = -delta;

        value = limit01(value + delta);
        mouseMode = DRAG_VALUE;

        notifyValueChanged();

        mouseMode = NONE;

        repaint();
        return true;
    }

    return false;
}

void ModulationSourceButton::onSkinChanged()
{
    arrow = associatedBitmapStore->getImage(IDB_MODSOURCE_SHOW_LFO);
}

void ModulationSourceButton::mouseUp(const juce::MouseEvent &event)
{
    mouseUpLongHold(event);
    setAlpha(1);
    setMouseCursor(juce::MouseCursor::NormalCursor);

    transientArmed = false;

    if (mouseMode == CLICK || mouseMode == CLICK_ARROW || mouseMode == CTRL_CLICK)
    {
        notifyValueChanged();
    }

    if (mouseMode == DRAG_COMPONENT_HAPPEN)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        auto q = event.position.translated(getBounds().getX(), getBounds().getY());

        sge->modSourceButtonDroppedAt(this, q.toInt());
        setBounds(mouseDownBounds);
    }

    if (mouseMode == DRAG_VALUE)
    {
        auto p = event.mouseDownPosition;

        if (value != valAtMouseDown)
        {
            p = juce::Point<float>(value * getWidth(), 20);
        }

        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            p = localPointToGlobal(p);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
        }

        if (event.mods.isAltDown())
        {
            value = valAtMouseDown;
            notifyValueChanged();
        }

        notifyEndEdit();
    }

    if (mouseMode == PREDRAG_VALUE)
    {
        notifyEndEdit();
    }

    mouseMode = NONE;

    return;
}

void ModulationSourceButton::mouseDrag(const juce::MouseEvent &event)
{
    if (mouseMode == NONE)
    {
        return;
    }

    auto distance = event.position.getX() - mouseDownLocation.getX();

    if (mouseMode == PREDRAG_VALUE && distance == 0)
    {
        return;
    }

    mouseDragLongHold(event);

    if (mouseMode == PREDRAG_VALUE)
    {
        if (!Surge::GUI::showCursor(storage))
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }
        // notifyBeginEdit();
        mouseMode = DRAG_VALUE;
        valAtMouseDown = value;
    }

    if (mouseMode == DRAG_VALUE)
    {
        float mul = 1.f;

        if (event.mods.isShiftDown())
        {
            mul = 0.1f;
        }

        value = limit01(valAtMouseDown + mul * event.getDistanceFromDragStartX() / getWidth());
        notifyValueChanged();
        repaint();

        return;
    }

    if (event.getDistanceFromDragStart() < 4)
    {
        setBounds(mouseDownBounds);
        return;
    }
    else
    {
        setAlpha(0.7);
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    getParentComponent()->toFront(false);

    if (sge)
    {
        sge->frontNonModalOverlays();
    }

    toFront(false);

    if (mouseMode != DRAG_COMPONENT_HAPPEN)
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    mouseMode = DRAG_COMPONENT_HAPPEN;
    componentDragger.dragComponent(this, event, nullptr);

    auto q = event.position.translated(getBounds().getX(), getBounds().getY());
    auto ota = transientArmed;

    if (sge)
    {
        sge->modSourceButtonDraggedOver(this, q.toInt());
    }

    if (ota != transientArmed)
    {
        repaint();
    }

    everDragged = true;
}

void ModulationSourceButton::mouseWheelMove(const juce::MouseEvent &event,
                                            const juce::MouseWheelDetails &wheel)
{
    if (isMeta)
    {
        float delta = wheel.deltaX - (wheel.isReversed ? 1 : -1) * wheel.deltaY;

        if (delta == 0)
        {
            return;
        }

#if MAC
        float speed = 1.2;
#else
        float speed = 0.42666;
#endif
        if (event.mods.isShiftDown())
        {
            speed = speed / 10.0;
        }

        value = limit01(value + speed * delta);
        mouseMode = DRAG_VALUE;

        notifyValueChanged();

        mouseMode = NONE;

        repaint();
    }
    else if (needsHamburger())
    {
        int dir = wheelAccumulationHelper.accumulate(wheel, false, true);

        if (dir != 0)
        {
            auto n = this->modlistIndex - dir;

            if (n < 0)
            {
                n = this->modlist.size() - 1;
            }
            else if (n >= modlist.size())
            {
                n = 0;
            }

            this->modlistIndex = n;
            mouseMode = HAMBURGER;

            notifyValueChanged();

            mouseMode = NONE;

            repaint();
        }
    }
}

void ModulationSourceButton::resized()
{
    hamburgerHome = getLocalBounds().withWidth(11).reduced(2, 2);

    auto b = getLocalBounds().withWidth(getHeight());

    if (isMeta)
    {
        b = b.withTrimmedBottom(10);
    }
    selectAccButton->setBounds(b);
    b = b.translated(getHeight(), 0);
    targetAccButton->setBounds(b);
    b = b.translated(getHeight(), 0);
    toggleArmAccButton->setBounds(b);

    selectAccButton->setVisible(true);
    toggleArmAccButton->setVisible(true);
    if (isLFO())
    {
        targetAccButton->setVisible(true);
    }

    if (isMeta)
    {
        b = getLocalBounds().withTrimmedTop(getHeight() - 10);
        macroSlider->setVisible(true);
        macroSlider->setBounds(b);
    }
}

void ModulationOverviewLaunchButton::buttonClicked(Button *button)
{
    editor->toggleOverlay(SurgeGUIEditor::MODULATION_EDITOR);
    repaint();
}

void ModulationOverviewLaunchButton::mouseDown(const juce::MouseEvent &event)
{
    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu contextMenu;

        auto msurl = editor->helpURLForSpecial("mod-list");
        auto hurl = editor->fullyResolvedHelpURL(msurl);

        editor->addHelpHeaderTo("Modulation List", hurl, contextMenu);

        contextMenu.showMenuAsync(editor->popupMenuOptions(this, false));
    }
    else
    {
        juce::Button::mouseDown(event);
    }
}

bool ModulationOverviewLaunchButton::keyPressed(const juce::KeyPress &key)
{
    if (Surge::Widgets::isAccessibleKey(key))
    {
        if (!Surge::Storage::getUserDefaultValue(
                storage, Surge::Storage::DefaultKey::MenuAndEditKeybindingsFollowKeyboardFocus,
                true))
            return false;
    }
    return juce::Button::keyPressed(key);
}

void ModulationOverviewLaunchButton::paintButton(juce::Graphics &g,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    auto FillCol = skin->getColor(Colors::ModSource::Unused::Background);
    auto FrameCol = skin->getColor(Colors::ModSource::Unused::Border);
    auto FontCol = skin->getColor(Colors::ModSource::Unused::Text);
    std::string msg = "List";

    if (isH)
    {
        FrameCol = skin->getColor(Colors::ModSource::Unused::BorderHover);
        FontCol = skin->getColor(Colors::ModSource::Unused::TextHover);
    }

    if (editor->isAnyOverlayPresent(SurgeGUIEditor::MODULATION_EDITOR))
    {
        msg = "Close";

        FrameCol = skin->getColor(Colors::ModSource::Used::Border);
        FontCol = skin->getColor(Colors::ModSource::Used::Text);

        if (isH)
        {
            FrameCol = skin->getColor(Colors::ModSource::Used::BorderHover);
            FontCol = skin->getColor(Colors::ModSource::Used::TextHover);
        }
    }

    g.fillAll(FillCol);
    g.setColour(FrameCol);
    g.drawRect(getLocalBounds(), 1);

    auto f = skin->fontManager->getLatoAtSize(9);
    auto h = f.getHeight() * 0.9f;
    auto sh = h * msg.length();
    auto y0 = (getHeight() - sh) / 2.f;

    g.setFont(f);
    g.setColour(FontCol);

    for (auto c : msg)
    {
        auto s = std::string("") + c;

        g.drawText(s, juce::Rectangle<int>(0, y0, getWidth(), h), juce::Justification::centred);

        y0 += h;
    }
}

void ModulationSourceButton::setIsBipolar(bool b)
{
    auto wasBipolar = isBipolar;
    isBipolar = b;
    if (macroSlider && wasBipolar != isBipolar)
    {
        if (auto h = macroSlider->getAccessibilityHandler())
        {
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        }
    }
}

} // namespace Widgets
} // namespace Surge