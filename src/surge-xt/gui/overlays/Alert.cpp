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

#include "Alert.h"
#include "widgets/SurgeTextButton.h"
#include "SurgeImageStore.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "OverlayUtils.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Overlays
{
struct MultiLineSkinLabel : juce::Label, public Surge::GUI::SkinConsumingComponent
{
    MultiLineSkinLabel() : juce::Label()
    {
        setWantsKeyboardFocus(true);
        setAccessible(true);
    }
    void paint(juce::Graphics &g) override
    {
        if (!skin)
            return;

        g.setColour(skin->getColor(Colors::Dialog::Label::Text));
        g.setFont(skin->fontManager->getLatoAtSize(9));
        g.drawFittedText(getText(), getLocalBounds(), juce::Justification::centredTop, 4);
    }

    void onSkinChanged() override { repaint(); }
};

Alert::Alert()
{
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
    setWantsKeyboardFocus(true);

    labelComponent = std::make_unique<MultiLineSkinLabel>();
    labelComponent->setTitle("Message");
    addAndMakeVisible(*labelComponent);

    okButton = std::make_unique<Surge::Widgets::SurgeTextButton>("alertOk");
    okButton->setAccessible(true);
    okButton->addListener(this);
    addAndMakeVisible(*okButton);

    cancelButton = std::make_unique<Surge::Widgets::SurgeTextButton>("alertCancel");
    cancelButton->setAccessible(true);
    cancelButton->addListener(this);
    addAndMakeVisible(*cancelButton);
}

Alert::~Alert() {}

void Alert::addToggleButtonAndSetText(const std::string &t)
{
    toggleButton = std::make_unique<juce::ToggleButton>(t);
    addAndMakeVisible(*toggleButton);
}

void Alert::resetAccessibility()
{
    auto txt = okButton->getButtonText().toStdString();
    SurgeGUIEditor::setAccessibilityInformationByTitleAndAction(okButton.get(), txt, "Press");
    txt = cancelButton->getButtonText().toStdString();
    SurgeGUIEditor::setAccessibilityInformationByTitleAndAction(cancelButton.get(), txt, "Press");
}

juce::Rectangle<int> Alert::getDisplayRegion()
{
    if (toggleButton)
    {
        return juce::Rectangle<int>(0, 0, 360, 111).withCentre(getBounds().getCentre());
    }
    return juce::Rectangle<int>(0, 0, 360, 95).withCentre(getBounds().getCentre());
}

void Alert::paint(juce::Graphics &g)
{
    auto fullRect = getDisplayRegion();

    paintOverlayWindow(g, skin, associatedBitmapStore, fullRect, title);
}

void Alert::resized()
{
    auto margin = 2, btnHeight = 17, btnWidth = 50, buttonVertTranslate = toggleButton ? 86 : 70;

    auto fullRect = getDisplayRegion();
    auto dialogCenter = fullRect.getWidth() / 2;
    auto buttonRow = fullRect.withHeight(btnHeight).translated(0, buttonVertTranslate);

    labelComponent->setBounds(fullRect.withTrimmedTop(18).withTrimmedBottom(btnHeight).reduced(6));

    if (singleButton)
    {
        auto okRect = buttonRow.withTrimmedLeft(dialogCenter - btnWidth / 2).withWidth(btnWidth);
        okButton->setBounds(okRect);
    }
    else
    {
        auto okRect =
            buttonRow.withTrimmedLeft(dialogCenter - btnWidth - margin).withWidth(btnWidth);
        auto canRect = buttonRow.withTrimmedLeft(dialogCenter + margin).withWidth(btnWidth);

        okButton->setBounds(okRect);
        cancelButton->setBounds(canRect);
    }

    if (toggleButton)
    {
        toggleButton->setBounds(fullRect.withHeight(16).translated(10, 70));
    }
}

void Alert::onSkinChanged()
{
    okButton->setSkin(skin, associatedBitmapStore);
    cancelButton->setSkin(skin, associatedBitmapStore);
    labelComponent->setSkin(skin, associatedBitmapStore);

    repaint();
}

void Alert::buttonClicked(juce::Button *button)
{
    if (!toggleButton)
    {
        if (button == okButton.get() && onOk)
        {
            onOk();
        }
        else if (button == cancelButton.get() && onCancel)
        {
            onCancel();
        }
    }
    else
    {
        if (button == okButton.get() && onOkForToggleState)
        {
            onOkForToggleState(toggleButton->getToggleState());
        }
        else if (button == cancelButton.get() && onCancelForToggleState)
        {
            onCancelForToggleState(toggleButton->getToggleState());
        }
    }

    setVisible(false);
}

void Alert::visibilityChanged()
{
    if (isVisible())
    {
        grabKeyboardFocus();
    }
}

bool Alert::keyPressed(const juce::KeyPress &press)
{
    if (press == juce::KeyPress::escapeKey)
    {
        buttonClicked(cancelButton.get());
        return true;
    }

    if (press == juce::KeyPress::returnKey)
    {
        buttonClicked(okButton.get());
        return true;
    }

    return false;
}

void Alert::setWindowTitle(const std::string &t)
{
    title = t;
    setTitle(title);
}
void Alert::setLabel(const std::string &t)
{
    label = t;
    labelComponent->setText(label, juce::NotificationType::dontSendNotification);
}
} // namespace Overlays
} // namespace Surge