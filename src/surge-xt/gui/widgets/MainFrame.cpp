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

namespace Surge
{
namespace Widgets
{
MainFrame::MainFrame()
{
#if SURGE_JUCE_ACCESSIBLE
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setAccessible(true);
    setDescription("Surge XT");
    setTitle("Main Frame");
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
#if SURGE_JUCE_ACCESSIBLE
        synthControls->setTitle("Surge Synth Controls");
        synthControls->setDescription("Surge Synth Controls");
#endif
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
#if SURGE_JUCE_ACCESSIBLE
        modGroup->setTitle("Modulators");
        modGroup->setDescription("Modulators");
#endif
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
#if SURGE_JUCE_ACCESSIBLE
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
#endif
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

#if SURGE_JUCE_ACCESSIBLE
struct mfKT : public juce::ComponentTraverser
{
    /*
     * This is a crude implementation which just sorts all components
     */
    juce::Component *on{nullptr};
    mfKT(juce::Component *c) : on(c) {}
    juce::Component *getDefaultComponent(juce::Component *parentComponent) override
    {
        return nullptr;
    }
    juce::Component *getNextComponent(juce::Component *current) override { return nullptr; }
    juce::Component *getPreviousComponent(juce::Component *current) override { return nullptr; }
    std::vector<juce::Component *> getAllComponents(juce::Component *parentComponent) override
    {
        std::vector<juce::Component *> res;
        for (auto c : on->getChildren())
            res.push_back(c);
        std::sort(res.begin(), res.end(), [this](auto a, auto b) { return lessThan(a, b); });
        return res;
    }

    bool lessThan(const juce::Component *a, const juce::Component *b)
    {
        int acg = -1, bcg = -1;
        auto ap = a->getProperties().getVarPointer("ControlGroup");
        auto bp = b->getProperties().getVarPointer("ControlGroup");
        if (ap)
            acg = *ap;
        if (bp)
            bcg = *bp;

        if (acg != bcg)
            return acg < bcg;

        auto at = dynamic_cast<const Surge::GUI::IComponentTagValue *>(a);
        auto bt = dynamic_cast<const Surge::GUI::IComponentTagValue *>(b);

        if (at && bt)
            return at->getTag() < bt->getTag();

        if (at && !bt)
            return false;

        if (!at && bt)
            return true;

#if SURGE_JUCE_ACCESSIBLE
        auto cd = a->getDescription().compare(b->getDescription());
        if (cd < 0)
            return true;
        if (cd > 0)
            return false;
#endif

        // so what the hell else to do?
        return a < b;
    }
};

std::unique_ptr<juce::ComponentTraverser> MainFrame::createFocusTraverser()
{
    return std::make_unique<mfKT>(this);
}
#endif

} // namespace Widgets
} // namespace Surge