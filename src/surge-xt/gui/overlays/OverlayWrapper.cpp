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

#include "OverlayWrapper.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"

namespace Surge
{
namespace Overlays
{
OverlayWrapper::OverlayWrapper()
{
    closeButton = std::make_unique<juce::TextButton>("closeButton");
    closeButton->addListener(this);
    closeButton->setButtonText("X");
    addChildComponent(*closeButton);

    tearOutButton = std::make_unique<juce::TextButton>("tearOut");
    tearOutButton->addListener(this);
    tearOutButton->setButtonText("^");
    addChildComponent(*tearOutButton);
}

OverlayWrapper::OverlayWrapper(const juce::Rectangle<int> &cb) : OverlayWrapper()
{
    componentBounds = cb;
    isModal = true;
}

void OverlayWrapper::paint(juce::Graphics &g)
{
    if (!hasInteriorDec)
    {
        return;
    }

    auto sp = getLocalBounds();
    if (isModal)
    {
        sp = componentBounds;
        g.fillAll(skin->getColor(Colors::Overlay::Background));
    }

    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.fillRect(sp);
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(10, juce::Font::bold));
    g.drawText(title, sp.withHeight(titlebarSize + margin), juce::Justification::centred);

    if (icon)
    {
        icon->drawAt(g, sp.getX() + 2, sp.getY() + 1, 1);
    }

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(sp, 1);
}

void OverlayWrapper::addAndTakeOwnership(std::unique_ptr<juce::Component> c)
{
    hasInteriorDec = true;
    auto sp = getLocalBounds();
    if (isModal)
        sp = componentBounds;

    auto q = sp.reduced(2 * margin, 2 * margin)
                 .withTrimmedBottom(titlebarSize)
                 .translated(0, titlebarSize + 0 * margin);
    primaryChild = std::move(c);
    primaryChild->setBounds(q);

    auto buttonSize = titlebarSize - 2;
    auto closeButtonBounds =
        getLocalBounds().withHeight(buttonSize).withLeft(getWidth() - buttonSize).translated(-2, 2);
    auto tearOutButtonBounds = closeButtonBounds.translated(-buttonSize - 2, 0);
    if (showCloseButton)
    {
        closeButton->setVisible(true);
        closeButton->setBounds(closeButtonBounds);
    }
    else
    {
        closeButton->setVisible(false);
    }

    if (canTearOut)
    {
        tearOutButton->setVisible(true);
        tearOutButton->setBounds(tearOutButtonBounds);
    }
    else
    {
        tearOutButton->setVisible(false);
    }

    addAndMakeVisible(*primaryChild);
}

void OverlayWrapper::buttonClicked(juce::Button *button)
{
    if (button == closeButton.get())
    {
        onClose();
    }

    if (button == tearOutButton.get())
    {
        doTearOut();
    }
}

bool OverlayWrapper::isTornOut() { return tearOutParent != nullptr; }

struct TearOutWindow : public juce::DocumentWindow
{
    TearOutWindow(const juce::String &s, int x) : juce::DocumentWindow(s, juce::Colours::black, x)
    {
    }

    OverlayWrapper *wrapping{nullptr};
    void closeButtonPressed()
    {
        if (wrapping)
        {
            wrapping->onClose();
        }
    }
    void minimiseButtonPressed()
    {
        if (wrapping)
        {
            wrapping->doTearIn();
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TearOutWindow);
};

void OverlayWrapper::supressInteriorDecoration()
{
    hasInteriorDec = false;
    setSize(primaryChild->getWidth(), primaryChild->getHeight());
    primaryChild->setBounds(getLocalBounds());
}

void OverlayWrapper::doTearOut()
{
    parentBeforeTearOut = getParentComponent();
    locationBeforeTearOut = getBoundsInParent();
    childLocationBeforeTearOut = primaryChild->getBounds();
    getParentComponent()->removeChildComponent(this);

    auto w = getWidth();
    auto h = getHeight();
    if (editor)
    {
        auto sc = 1.0 * editor->getZoomFactor() / 100.0;
        w *= sc;
        h *= sc;
        setSize(w, h);

        primaryChild->setTransform(juce::AffineTransform::scale(sc));
        primaryChild->setBounds(getLocalBounds());
    }

    std::string t = "Tear Out";
    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        t = oc->getEnclosingParentTitle();
    }
    auto dw = std::make_unique<TearOutWindow>(t, juce::DocumentWindow::closeButton |
                                                     juce::DocumentWindow::minimiseButton);
    dw->setContentNonOwned(this, false);
    dw->setContentComponentSize(w, h);
    dw->setVisible(true);
    dw->toFront(true);
    dw->wrapping = this;
    supressInteriorDecoration();
    tearOutParent = std::move(dw);
}

void OverlayWrapper::doTearIn()
{
    if (!isTornOut() || !parentBeforeTearOut)
    {
        // Should never happen but if it does
        onClose();
        return;
    }
    tearOutParent.reset(nullptr);
    hasInteriorDec = true;

    primaryChild->setTransform(juce::AffineTransform());
    primaryChild->setBounds(childLocationBeforeTearOut);
    setBounds(locationBeforeTearOut);
    parentBeforeTearOut->addAndMakeVisible(*this);
    parentBeforeTearOut = nullptr;
}
} // namespace Overlays
} // namespace Surge
