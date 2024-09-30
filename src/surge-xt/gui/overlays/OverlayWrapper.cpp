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

#include "OverlayWrapper.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "SurgeGUIEditor.h"
#include "OverlayComponent.h"
#include "widgets/MainFrame.h"
#include "SurgeJUCELookAndFeel.h"

namespace Surge
{
namespace Overlays
{

OverlayWrapper::OverlayWrapper()
{
    closeButton.reset(
        getLookAndFeel().createDocumentWindowButton(juce::DocumentWindow::closeButton));
    closeButton->addListener(this);
    addChildComponent(*closeButton);

    tearOutButton.reset(
        getLookAndFeel().createDocumentWindowButton(juce::DocumentWindow::maximiseButton));
    tearOutButton->addListener(this);
    addChildComponent(*tearOutButton);

    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
}

OverlayWrapper::OverlayWrapper(const juce::Rectangle<int> &cb) : OverlayWrapper()
{
    componentBounds = cb;
    isModal = true;
    setAccessible(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
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

    auto paintTitle = title;

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        paintTitle = oc->getEnclosingParentTitle();
    }

    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.fillRect(sp);
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
    g.drawText(paintTitle, sp.withHeight(titlebarSize + margin), juce::Justification::centred);

    if (icon)
    {
        const auto iconSize = titlebarSize;

#if MAC
        icon->drawAt(g, sp.getRight() - iconSize + 1, sp.getY() + 1, 1);
#else
        icon->drawAt(g, sp.getX() + 2, sp.getY() + 1, 1);
#endif
    }

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(sp, 1);
}

void OverlayWrapper::addAndTakeOwnership(std::unique_ptr<juce::Component> c)
{
    hasInteriorDec = true;
    auto sp = getLocalBounds();

    if (isModal)
    {
        sp = componentBounds;
    }

    auto q = sp.reduced(2 * margin, 2 * margin)
                 .withTrimmedBottom(titlebarSize)
                 .translated(0, titlebarSize);
    primaryChild = std::move(c);
    primaryChild->setBounds(q);

    auto buttonSize = titlebarSize;
    juce::Rectangle<int> closeButtonBounds;
    juce::Rectangle<int> tearOutButtonBounds;

#if MAC
    closeButtonBounds = getLocalBounds().withSize(buttonSize, buttonSize).translated(2, 2);
    tearOutButtonBounds = closeButtonBounds.translated(buttonSize + 2, 0);
#else
    closeButtonBounds =
        getLocalBounds().withHeight(buttonSize).withLeft(getWidth() - buttonSize).translated(-2, 2);
    tearOutButtonBounds = closeButtonBounds.translated(-buttonSize - 2, 0);
#endif

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

    auto paintTitle = title;

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        paintTitle = oc->getEnclosingParentTitle();
    }

    setTitle(paintTitle);
    setDescription(paintTitle);
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

void OverlayWrapper::visibilityChanged()
{
    if (!isVisible() && !isShowing())
        return;
    if (auto olc = getPrimaryChildAsOverlayComponent())
    {
        olc->shownInParent();
    }
}

bool OverlayWrapper::isTornOut() { return tearOutParent != nullptr; }

struct TearOutWindow : public juce::DocumentWindow, public Surge::GUI::SkinConsumingComponent
{
    struct PinButton : juce::Component
    {
        TearOutWindow *window{nullptr};
        PinButton(TearOutWindow *w) : window(w) {}

        void paint(juce::Graphics &g) override
        {
            if (hovered)
            {
                g.fillAll(juce::Colour(96, 96, 96));
            }

            // g.setColour(juce::Colours::white);
            g.setColour(window->skin->getColor(Colors::Dialog::Titlebar::Text));

            juce::Rectangle<int> pinHead = getLocalBounds().reduced(12, 6).removeFromTop(10);
            juce::Line<int> pinEdge{pinHead.getBottomLeft().translated(-3, 0),
                                    pinHead.getBottomRight().translated(3, 0)};
            juce::Line<float> pinNeedle{
                pinEdge.toFloat().getPointAlongLineProportionally(0.5).translated(0.f, 2.f),
                pinEdge.toFloat().getPointAlongLineProportionally(0.5).translated(0.f, 5.f)};

            if (window->isPinned)
            {
                g.fillRect(pinHead.removeFromBottom(5));
            }
            else
            {
                g.drawRect(pinHead.toFloat(), 1.75f);
            }

            g.drawLine(pinEdge.toFloat(), 2.5f);
            g.drawLine(pinNeedle, 2.5f);
        }

        bool hovered{false};

        void mouseEnter(const juce::MouseEvent &e) override
        {
            hovered = true;
            repaint();
        }

        void mouseExit(const juce::MouseEvent &e) override
        {
            hovered = false;
            repaint();
        }

        void mouseUp(const juce::MouseEvent &e) override
        {
            if (getLocalBounds().contains(e.position.toInt()))
            {
                window->togglePin();
            }
        }
    };

    bool isPinned{false};
    OverlayWrapper *wrapping{nullptr};

    TearOutWindow(const juce::String &s, int x, const bool pinned)
        : juce::DocumentWindow(s, juce::Colours::black, x)
    {
        pinButton = std::make_unique<PinButton>(this);
        Component::addAndMakeVisible(pinButton.get());

        isPinned = pinned;
        setAlwaysOnTop(isPinned);
    }

    void onSkinChanged() override { pinButton->repaint(); }

    void togglePin()
    {
        isPinned = !isPinned;

        if (Surge::GUI::getIsStandalone())
        {
            Surge::Storage::updateUserDefaultValue(wrapping->storage,
                                                   std::get<2>(wrapping->canTearOutData), isPinned);
        }
        else
        {
            Surge::Storage::updateUserDefaultValue(wrapping->storage,
                                                   std::get<3>(wrapping->canTearOutData), isPinned);
        }
        setAlwaysOnTop(isPinned);
        repaint();
    }

    WindowControlKind findControlAtPoint(juce::Point<float> point) const override
    {
        // See issue 7805 for this temporary fix
        auto res = DocumentWindow::findControlAtPoint(point);
        if (res == WindowControlKind::minimise || res == WindowControlKind::close)
            return WindowControlKind::client;
        return res;
    }

    void mouseDoubleClick(const juce::MouseEvent &event) override
    {
        auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(wrapping->primaryChild.get());
        auto sz = oc->getEnclosingParentPosition();

        auto mw = sz.getWidth();
        auto mh = sz.getHeight();

        wrapping->primaryChild->getTransform().transformPoint(mw, mh);

        wrapping->tearOutParent->setContentComponentSize(mw, mh);

        Surge::Storage::updateUserDefaultValue(wrapping->storage,
                                               wrapping->canTearOutResizePair.second,
                                               std::make_pair(getWidth(), getHeight()));
    }

    void resized() override
    {
        juce::DocumentWindow::resized();

        auto tba = getTitleBarArea();
        auto tbh = tba.getHeight() + 5;

// reposition the pin button to the other side on Mac
#if MAC
        auto xPos = tba.getX() + (2 * tbh);
#else
        auto xPos = tba.getRight() - (3 * tbh);
#endif

        auto r = juce::Rectangle<int>(xPos, 1, tbh, tbh);

        pinButton->setBounds(r);
    }

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
    bool supressMoveUpdates{false};

    void moved() override
    {
        if (supressMoveUpdates)
            return;

        outstandingMoves++;
        // writing every move would be "bad". Add a 1 second delay.
        juce::Timer::callAfterDelay(1000, [that = juce::Component::SafePointer(this)]() {
            if (that)
            {
                that->moveUpdate();
            }
        });
    }

    void moveUpdate()
    {
        outstandingMoves--;

        if (outstandingMoves == 0 && wrapping && wrapping->storage)
        {
            auto tl = getBounds().getTopLeft();

            Surge::Storage::updateUserDefaultValue(wrapping->storage,
                                                   std::get<1>(wrapping->canTearOutData),
                                                   std::make_pair(tl.x, tl.y));
        }
    }

    std::unique_ptr<juce::Component> pinButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TearOutWindow);
};

void OverlayWrapper::supressInteriorDecoration()
{
    hasInteriorDec = false;
    setSize(primaryChild->getWidth(), primaryChild->getHeight());
    primaryChild->setBounds(getLocalBounds());
}

void OverlayWrapper::resized()
{
    if (isTornOut())
    {
        if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
        {
            if (oc->minh > 0 && oc->minw > 0)
            {
                auto sz = tearOutParent->getContentComponentBorder();
                auto w = getWidth(), h = getHeight();
                auto mw = oc->minw;
                auto mh = oc->minh;

                primaryChild->getTransform().transformPoint(mw, mh);

                mw = w < mw ? mw : w;
                mh = h < mh ? mh : h;

                if (w < mw || h < mh)
                {

                    tearOutParent->setContentComponentSize(mw, mh);
                    auto p = static_cast<juce::Component *>(this);
                    while (p->getParentComponent())
                        p = p->getParentComponent();
                    p->repaint();

                    return;
                }
            }
        }

        auto topcc = tearOutParent->getContentComponent()->getBounds();

        primaryChild->setBounds(0, 0, topcc.getWidth(), topcc.getHeight());

        if (resizeRecordsSize)
        {
            Surge::Storage::updateUserDefaultValue(storage, canTearOutResizePair.second,
                                                   std::make_pair(getWidth(), getHeight()));
        }
    }
}

void OverlayWrapper::doTearOut(const juce::Point<int> &showAt)
{
    auto rvs = juce::ScopedValueSetter(resizeRecordsSize, false);
    auto pt = std::make_pair(-1, -1);

    if (storage)
    {
        pt = Surge::Storage::getUserDefaultValue(storage, canTearOutResizePair.second, pt);
    }

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

    if (pt.first > 0)
    {
        w = pt.first;
    }

    if (pt.second > 0)
    {
        h = pt.second;
    }

    std::string t = "Tear Out";

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        t = oc->getEnclosingParentTitle();
        oc->onTearOutChanged(true);
    }

    auto dw = std::make_unique<TearOutWindow>(
        t, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton, isAlwaysOnTop);
    dw->supressMoveUpdates = true;
    dw->setContentNonOwned(this, false);
    dw->setSkin(skin, associatedBitmapStore);
    // This doesn't work but i think that's because we drop the icon in our DW L&F
    // It does, however, create a renderable image
    // dw->setIcon(associatedBitmapStore->getImage(IDB_SURGE_ICON)->asJuceImage());

    auto brd = dw->getContentComponentBorder();

    dw->setContentComponentSize(w, h);
    dw->setVisible(true);

    // CONSTRAINER SETUP
    tearOutConstrainer.reset();

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        if (oc->minh > 0 || oc->minw > 0)
        {
            tearOutConstrainer = std::make_unique<juce::ComponentBoundsConstrainer>();

            auto mw = oc->minw;
            auto mh = oc->minh;

            primaryChild->getTransform().transformPoint(mw, mh);

            auto sz = dw->getContentComponentBorder();

            mw += sz.getLeftAndRight();
            mh += sz.getTopAndBottom();

            if (mw > 0)
            {
                tearOutConstrainer->setMinimumWidth(mw);
            }

            if (mh > 0)
            {
                tearOutConstrainer->setMinimumHeight(mh);
            }

            dw->setConstrainer(tearOutConstrainer.get());
        }
    }

    dw->setResizable(canTearOutResize, canTearOutResize);

    if (showAt.x >= 0 && showAt.y >= 0)
        dw->setTopLeftPosition(showAt.x, showAt.y);
    else
    {
        auto pt = std::make_pair(-1, -1);

        if (storage)
        {
            pt = Surge::Storage::getUserDefaultValue(storage, std::get<1>(canTearOutData), pt);
        }

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
    dw->supressMoveUpdates = false;
    supressInteriorDecoration();
    tearOutParent = std::move(dw);
    tearOutParent->setColour(juce::DocumentWindow::backgroundColourId,
                             findColour(SurgeJUCELookAndFeel::SurgeColourIds::topWindowBorderId));

    if (pt.first > 0 && pt.second > 0)
    {
        tearOutParent->setSize(pt.first, pt.second);
        resized();
    }

    resizeRecordsSize = true;
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

    if (parentBeforeTearOut)
    {
        parentBeforeTearOut->addAndMakeVisible(*this);
        parentBeforeTearOut = nullptr;
    }

    if (auto oc = dynamic_cast<Surge::Overlays::OverlayComponent *>(primaryChild.get()))
    {
        oc->onTearOutChanged(false);
    }
}

void OverlayWrapper::mouseDown(const juce::MouseEvent &e)
{
    if (isTornOut())
    {
        return;
    }

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
    {
        return;
    }

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
    {
        return;
    }

    toFront(true);

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
        auto tl = bounds.getTopLeft();
        auto pw = 1.f * getParentComponent()->getWidth();
        auto ph = 1.f * getParentComponent()->getHeight();
        if (tl.x < 0)
            bounds = bounds.translated(-tl.x, 0);
        if (tl.x + bounds.getWidth() > pw)
        {
            // tlx + bgw = pw + q
            // q = tlx + bgw - pw
            auto q = tl.x + bounds.getWidth() - pw;
            bounds = bounds.translated(-q, 0);
        }
        if (tl.y < 0)
            bounds = bounds.translated(0, -tl.y);
        if (tl.y + bounds.getHeight() > ph)
        {
            auto q = tl.y + bounds.getHeight() - ph;
            bounds = bounds.translated(0, -q);
        }
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

    if (tearOutParent)
    {
        tearOutParent->setColour(
            juce::DocumentWindow::backgroundColourId,
            findColour(SurgeJUCELookAndFeel::SurgeColourIds::topWindowBorderId));
        tearOutParent->repaint();

        auto skc = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(tearOutParent.get());

        if (skc)
        {
            skc->setSkin(skin, associatedBitmapStore);
        }
    }

    repaint();
}

void OverlayWrapper::onClose()
{
    auto pc = getPrimaryChildAsOverlayComponent();
    if (pc && pc->getPreCloseChickenBoxMessage().has_value())
    {
        auto pcm = *(pc->getPreCloseChickenBoxMessage());
        editor->alertYesNo(
            pcm.first, pcm.second,
            [w = juce::Component::SafePointer(this)]() {
                if (!w)
                    return;

                w->closeOverlay();
                if (w->isTornOut())
                    w->tearOutParent.reset(nullptr);
            },
            [w = juce::Component::SafePointer(pc)]() {
                if (!w)
                    return;
                w->grabKeyboardFocus();
            });
    }
    else
    {
        closeOverlay();
        if (isTornOut())
            tearOutParent.reset(nullptr);
    }
}
} // namespace Overlays
} // namespace Surge
