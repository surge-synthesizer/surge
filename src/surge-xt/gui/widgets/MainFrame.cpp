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

#include "MainFrame.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUICallbackInterfaces.h"
#include "AccessibleHelpers.h"
#include "MultiSwitch.h"
#include "SurgeGUIEditorTags.h"
#include <version.h>
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{
MainFrame::MainFrame()
{
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
    setAccessible(true);
    setDescription("Surge XT");
    setTitle("Main Frame");
}

void MainFrame::paint(juce::Graphics &g)
{
    if (bg)
        bg->draw(g, 1.0);

#if BUILD_IS_DEBUG
    auto r = getLocalBounds().withTrimmedLeft(getWidth() - 150).withTrimmedTop(getHeight() - 45);
    g.setColour(juce::Colours::red.withAlpha(0.7f));
    g.fillRect(r);
    r = r.withTrimmedTop(1);
    g.setFont(9);
    g.setColour(juce::Colours::white);
    g.drawText(Surge::Build::FullVersionStr, r, juce::Justification::centredTop);
    r = r.withTrimmedTop(14);
    g.drawText(std::string(Surge::Build::BuildDate) + " " + Surge::Build::BuildTime, r,
               juce::Justification::centredTop);
    g.drawText(std::string("DEBUG BS=") + std::to_string(BLOCK_SIZE), r.reduced(2),
               juce::Justification::bottomLeft);
#endif
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
        auto where = getLocalPoint(nullptr, juce::Desktop::getInstance().getMousePosition());
        editor->useDevMenu = event.mods.isShiftDown();
        editor->showSettingsMenu(where, nullptr);
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
            {
                if (at->getTag() == tag_settingsmenu)
                    return true;
                if (bt->getTag() == tag_settingsmenu)
                    return false;

                return at->getTag() < bt->getTag();
            }

            return false;
        };

        std::sort(localComponents.begin(), localComponents.end(), compare);

        // Sort here
        for (auto *c : localComponents)
        {
            if (auto dc = dynamic_cast<Surge::Widgets::HasAccessibleSubComponentForFocus *>(c); dc)
            {
                components.push_back(dc->getCurrentAccessibleSelectionComponent());
            }
            else
            {
                components.push_back(c);

                if (!(c->isKeyboardFocusContainer()))
                    findAllComponents(c, components);
            }
        }
    }

    juce::Component *navigate(juce::Component *c, int dir)
    {
        if (!c)
            return nullptr;
        std::vector<juce::Component *> v;
        findAllComponents(top, v);

        if (auto dc = dynamic_cast<HasAccessibleSubComponentForFocus *>(c))
        {
            auto q = dc->getCurrentAccessibleSelectionComponent();
            if (q)
            {
                c = q;
            }
        }

        if (auto dcp = dynamic_cast<HasAccessibleSubComponentForFocus *>(c->getParentComponent()))
        {
            c = dcp->getCurrentAccessibleSelectionComponent();
        }

        if (!c)
            return nullptr;

        auto iter = std::find(v.begin(), v.end(), c);
        if (iter == v.end())
        {
            return nullptr;
        }

        juce::Component *res{nullptr};
        switch (dir)
        {
        case 1:
            while (!res && iter != std::prev(v.cend()))
            {
                res = *std::next(iter);
                iter++;
            }
            if (!res)
            {
                res = *(v.cbegin());
            }
            break;
        case -1:
            while (!res && iter != v.cbegin())
            {
                res = *std::prev(iter);
                iter--;
            }
            if (!res)
            {
                res = *(std::prev(v.cend()));
            }
            break;
        }

        return res;
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

juce::Component *
MainFrame::recursivelyFindFirstChildMatching(std::function<bool(juce::Component *)> op)
{
    std::function<juce::Component *(juce::Component *)> rec;
    rec = [&rec, this, &op](juce::Component *c) -> juce::Component * {
        if (op(c))
            return c;
        for (auto *k : c->getChildren())
        {
            auto r = rec(k);
            if (r)
                return r;
        }
        return nullptr;
    };
    return rec(this);
}
} // namespace Widgets
} // namespace Surge