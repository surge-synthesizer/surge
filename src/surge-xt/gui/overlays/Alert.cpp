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
        g.drawFittedText(getText(), getLocalBounds(), juce::Justification::centred, maxTextLines);
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

void Alert::addDontAskAgainButtonAndSetText(const std::string &t)
{
    dontAskAgainButton = std::make_unique<juce::ToggleButton>(t);
    addAndMakeVisible(*dontAskAgainButton);
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
    return juce::Rectangle<int>(0, 0, windowWidth,
                                108 + (dontAskAgainButton ? btnHeight + windowMargin : 0))
        .withCentre(getBounds().getCentre());
}

void Alert::paint(juce::Graphics &g)
{
    auto fullRect = getDisplayRegion();

    paintOverlayWindow(g, skin, associatedBitmapStore, fullRect, title);
}

void Alert::resized()
{
    auto margin = 2;

    auto fullRect = getDisplayRegion();
    auto dialogCenter = fullRect.getWidth() / 2;

    labelComponent->setBounds(fullRect.withTrimmedTop(18).withHeight(68).reduced(windowMargin));

    auto buttonRow =
        fullRect.withY(fullRect.getY() + fullRect.getHeight() - btnHeight - windowMargin)
            .withHeight(btnHeight);

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

    if (dontAskAgainButton)
    {
        dontAskAgainButton->setBounds(buttonRow.translated(10, 0));
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
    if (!dontAskAgainButton)
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
            onOkForToggleState(dontAskAgainButton->getToggleState());
        }
        else if (button == cancelButton.get() && onCancelForToggleState)
        {
            onCancelForToggleState(dontAskAgainButton->getToggleState());
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

void Alert::setLabel(const std::string &l)
{
    label = l;
    labelComponent->setText(label, juce::NotificationType::dontSendNotification);
}
} // namespace Overlays
} // namespace Surge