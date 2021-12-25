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

#include "MainFrame.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUICallbackInterfaces.h"
#include "AccessibleHelpers.h"

namespace Surge
{
namespace Widgets
{
MainFrame::MainFrame()
{
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setAccessible(true);
    setDescription("Surge XT");
    setTitle("Main Frame");
}
void MainFrame::mouseDown(const juce::MouseEvent &event)
{
    if (!editor)
    {
        return;
    }

    editor->hideTypeinParamEditor();

    if (event.mods.isMiddleButtonDown())
    {
        editor->toggle_mod_editing();
    }

    if (event.mods.isPopupMenu())
    {
        editor->useDevMenu = false;
        editor->showSettingsMenu(juce::Point<int>{}, nullptr);
    }
}
juce::Component *MainFrame::getSynthControlsLayer()
{
    if (!synthControls)
    {
        synthControls = std::make_unique<OverlayComponent>();
        synthControls->setBounds(getLocalBounds());
        synthControls->setInterceptsMouseClicks(false, true);
        synthControls->setTitle("Surge Synth Controls");
        synthControls->setDescription("Surge Synth Controls");
        synthControls->getProperties().set("ControlGroup", (int)12);
    }

    if (getIndexOfChildComponent(synthControls.get()) < 0)
    {
        editor->addAndMakeVisibleWithTracking(this, *synthControls);
    }

    return synthControls.get();
}
juce::Component *MainFrame::getModButtonLayer()
{
    if (!modGroup)
    {
        modGroup = std::make_unique<OverlayComponent>();
        modGroup->setBounds(getLocalBounds());
        modGroup->setInterceptsMouseClicks(false, true);
        modGroup->setTitle("Modulators");
        modGroup->setDescription("Modulators");
        modGroup->getProperties().set("ControlGroup", (int)endCG + 2000);
    }

    if (getIndexOfChildComponent(modGroup.get()) < 0)
    {
        editor->addAndMakeVisibleWithTracking(this, *modGroup);
    }

    return modGroup.get();
}
juce::Component *MainFrame::getControlGroupLayer(ControlGroup cg)
{
    if (!cgOverlays[cg])
    {
        auto ol = std::make_unique<OverlayComponent>();
        ol->setBounds(getLocalBounds());
        ol->setInterceptsMouseClicks(false, true);
        auto t = "Group " + std::to_string((int)cg);
        switch (cg)
        {
        case cg_GLOBAL:
            t = "Global Controls";
            break;
        case cg_OSC:
            t = "Oscillator Controls";
            break;
        case cg_MIX:
            t = "Mixer Controls";
            break;
        case cg_FILTER:
            t = "Filter Controls";
            break;
        case cg_ENV:
            t = "Envelope Controls";
            break;
        case cg_LFO:
            t = "LFO Controls";
            break;
        case cg_FX:
            t = "FX Controls";
            break;
        default:
            t = "Unknown Controls";
            break;
        }
        ol->setDescription(t);
        ol->setTitle(t);
        ol->getProperties().set("ControlGroup", (int)cg + 1000);
        cgOverlays[cg] = std::move(ol);
    }

    if (getIndexOfChildComponent(cgOverlays[cg].get()) < 0)
    {
        editor->addAndMakeVisibleWithTracking(this, *cgOverlays[cg]);
    }

    return cgOverlays[cg].get();
}

void MainFrame::addChildComponentThroughEditor(juce::Component &c)
{
    editor->addComponentWithTracking(this, c);
}

struct GlobalKeyboardTraverser : public juce::KeyboardFocusTraverser
{
    juce::Component *top{nullptr};
    GlobalKeyboardTraverser(juce::Component *topParent)
        : juce::KeyboardFocusTraverser(), top(topParent)
    {
    }

    static void findAllComponents(const juce::Component *parent,
                                  std::vector<juce::Component *> &components)
    {
        if (parent == nullptr || parent->getNumChildComponents() == 0)
            return;

        std::vector<juce::Component *> localComponents;

        for (auto *c : parent->getChildren())
            if (c->isVisible() && c->isEnabled())
                localComponents.push_back(c);

        auto compare = [](const juce::Component *a, const juce::Component *b) {
            auto pcg = [](const juce::Component *p) {
                while (p)
                {
                    auto cgp = p->getProperties().getVarPointer("ControlGroup");
                    if (cgp)
                        return (int)(*cgp);
                    p = p->getParentComponent();
                }
                return -1;
            };
            auto cga = pcg(a);
            auto cgb = pcg(b);

            if (cga != cgb)
                return cga < cgb;

            auto at = dynamic_cast<const Surge::GUI::IComponentTagValue *>(a);
            auto bt = dynamic_cast<const Surge::GUI::IComponentTagValue *>(b);

            if (at && bt)
                return at->getTag() < bt->getTag();

            return false;
        };

        std::sort(localComponents.begin(), localComponents.end(), compare);

        // Sort here
        for (auto *c : localComponents)
        {
            components.push_back(c);

            if (!(c->isKeyboardFocusContainer()))
                findAllComponents(c, components);
        }
    }

    juce::Component *navigate(juce::Component *c, int dir)
    {
        std::vector<juce::Component *> v;
        findAllComponents(top, v);

        const auto iter = std::find(v.cbegin(), v.cend(), c);
        if (iter == v.cend())
            return nullptr;

        switch (dir)
        {
        case 1:
            if (iter != std::prev(v.cend()))
            {
                return *std::next(iter);
            }
            break;
        case -1:
            if (iter != v.cbegin())
            {
                return *std::prev(iter);
            }
            break;
        }
        return nullptr;
    }

    juce::Component *traverse(juce::Component *c, int dir)
    {
        if (auto *tc = navigate(c, dir))
        {
            if (tc->getWantsKeyboardFocus())
                return tc;
            return traverse(tc, dir);
        }
        return nullptr;
    }

    juce::Component *getDefaultComponent(juce::Component *parentComponent) override
    {
        if (parentComponent == top)
        {
            // This never seems to get called
            // std::cout << "Asked for default" << std::endl;
        }
        return nullptr;
    }
    juce::Component *getNextComponent(juce::Component *c) override { return traverse(c, +1); }
    juce::Component *getPreviousComponent(juce::Component *c) override { return traverse(c, -1); }
    std::vector<juce::Component *> getAllComponents(juce::Component *parentComponent) override
    {
        std::vector<juce::Component *> res;
        findAllComponents(top, res);
        return res;
    }
};

std::unique_ptr<juce::ComponentTraverser> MainFrame::createFocusTraverser()
{
    return std::make_unique<Surge::Widgets::GroupTagTraverser>(this);
}

std::unique_ptr<juce::ComponentTraverser> MainFrame::OverlayComponent::createFocusTraverser()
{
    return std::make_unique<Surge::Widgets::GroupTagTraverser>(this);
}

std::unique_ptr<juce::ComponentTraverser> MainFrame::createKeyboardFocusTraverser()
{
    return std::make_unique<GlobalKeyboardTraverser>(this);
}

} // namespace Widgets
} // namespace Surge