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
#include "OverlayComponent.h"
#include "widgets/MainFrame.h"

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
    void closeButtonPressed() override
    {
        if (wrapping)
        {
            wrapping->onClose();
        }
    }
    void minimiseButtonPressed() override
    {
        if (wrapping)
        {
            wrapping->doTearIn();
        }
    }

    int outstandingMoves = 0;
    void moved() override
    {
        outstandingMoves++;
        // writing every move would be "bad". Add a 1 second delay.
        juce::Timer::callAfterDelay(1000, [this]() { this->moveUpdate(); });
    }
    void moveUpdate()
    {
        outstandingMoves--;
        if (outstandingMoves == 0 && wrapping->storage)
        {
            auto tl = getBounds().getTopLeft();
            Surge::Storage::updateUserDefaultValue(
                wrapping->storage, wrapping->canTearOutPair.second, std::make_pair(tl.x, tl.y));
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

void OverlayWrapper::doTearOut(const juce::Point<int> &showAt)
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
        oc->onTearOutChanged(true);
    }
    auto dw = std::make_unique<TearOutWindow>(t, juce::DocumentWindow::closeButton |
                                                     juce::DocumentWindow::minimiseButton);
    dw->setContentNonOwned(this, false);
    dw->setContentComponentSize(w, h);
    dw->setVisible(true);
    if (showAt.x >= 0 && showAt.y >= 0)
        dw->setTopLeftPosition(showAt.x, showAt.y);
    else
    {
        auto pt = std::make_pair(-1, -1);
        if (storage)
            pt = Surge::Storage::getUserDefaultValue(storage, canTearOutPair.second, pt);
        auto dt = juce::Desktop::getInstance()
                      .getDisplays()
                      .getDisplayForPoint(editor->frame->getBounds().getTopLeft())
                      ->userArea;
        auto dtp = juce::Point<int>((dt.getWidth() - w) / 2, (dt.getHeight() - h) / 2);
        if (pt.first > 0 && pt.second > 0 && pt.first < dt.getWidth() - w / 2 &&
            pt.second < dt.getHeight() - h / 2)
        {
            dtp.x = pt.first;
            dtp.y = pt.second;
        }
        dw->setTopLeftPosition(dtp);
    }
    dw->toFront(true);
    dw->wrapping = this;
    supressInteriorDecoration();
    tearOutParent = std::move(dw);
}

juce::Point<int> OverlayWrapper::currentTearOutLocation()
{
    if (!isTornOut())
        return juce::Point<int>(-1, -1);

    return tearOutParent->getPosition();
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

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        oc->onTearOutChanged(false);
    }
}

void OverlayWrapper::mouseDown(const juce::MouseEvent &e)
{
    if (isTornOut())
        return;

    auto c = getPrimaryChildAsOverlayComponent();
    if (c && c->getCanMoveAround())
    {
        isDragging = true;

        // This is borrowed from juce::ComponentDragger
        mouseDownWithinTarget = e.getEventRelativeTo(this).getMouseDownPosition();
        repaint();
    }
}

void OverlayWrapper::mouseDoubleClick(const juce::MouseEvent &e)
{
    if (isTornOut())
        return;

    auto c = getPrimaryChildAsOverlayComponent();
    if (c && c->getCanMoveAround() && editor)
    {
        auto p = c->defaultLocation;
        auto b = getBounds();
        auto q = juce::Rectangle<int>(p.x, p.y, b.getWidth(), b.getHeight());
        setBounds(q);
        Surge::Storage::updateUserDefaultValue(storage, c->getMoveAroundKey(),
                                               std::make_pair(p.x, p.y));
    }
}

void OverlayWrapper::mouseUp(const juce::MouseEvent &e)
{
    if (isTornOut())
        return;

    auto c = getPrimaryChildAsOverlayComponent();
    if (c && c->getCanMoveAround() && editor)
    {
        isDragging = false;
        repaint();

        Surge::Storage::updateUserDefaultValue(storage, c->getMoveAroundKey(),
                                               std::make_pair(getX(), getY()));
    }
}

void OverlayWrapper::mouseDrag(const juce::MouseEvent &e)
{
    if (isTornOut())
        return;

    auto c = getPrimaryChildAsOverlayComponent();
    if (c && c->getCanMoveAround())
    {
        // Borrowed from juce::ComponentDragger
        auto bounds = getBounds();
        bounds += getLocalPoint(nullptr, e.source.getScreenPosition()).roundToInt() -
                  mouseDownWithinTarget;
        setBounds(bounds);
    }
}

OverlayComponent *OverlayWrapper::getPrimaryChildAsOverlayComponent()
{
    return dynamic_cast<OverlayComponent *>(primaryChild.get());
}

void OverlayWrapper::onSkinChanged()
{
    auto skc = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(primaryChild.get());
    if (skc)
    {
        skc->setSkin(skin, associatedBitmapStore);
    }
    icon = associatedBitmapStore->getImage(IDB_SURGE_ICON);
}

} // namespace Overlays
} // namespace Surge
