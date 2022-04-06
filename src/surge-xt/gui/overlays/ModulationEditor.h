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

#ifndef SURGE_XT_MODULATIONEDITOR_H
#define SURGE_XT_MODULATIONEDITOR_H

#include "juce_gui_basics/juce_gui_basics.h"
#include "OverlayComponent.h"
#include "SurgeSynthesizer.h"
#include "SkinSupport.h"

class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
// class ModulationListBoxModel;
struct ModulationSideControls;
struct ModulationListContents;

class ModulationEditor : public OverlayComponent,
                         public Surge::GUI::SkinConsumingComponent,
                         public SurgeSynthesizer::ModulationAPIListener
{
  public:
    ModulationEditor(SurgeGUIEditor *ed, SurgeSynthesizer *s);
    ~ModulationEditor();

    SurgeGUIEditor *ed;
    SurgeSynthesizer *synth;

    std::unique_ptr<juce::Timer> idleTimer;
    void idle();

    void paint(juce::Graphics &g) override;
    void resized() override;

    struct SelfModulationGuard
    {
        SelfModulationGuard(ModulationEditor *ed) : moded(ed) { moded->selfModulation = true; }
        ~SelfModulationGuard() { moded->selfModulation = false; }
        ModulationEditor *moded;
    };
    std::atomic<bool> selfModulation{false}, needsModUpdate{false}, needsModValueOnlyUpdate{false};
    void modSet(long ptag, modsources modsource, int modsourceScene, int index, float value,
                bool isNew) override;
    void modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                  bool mute) override;
    void modCleared(long ptag, modsources modsource, int modsourceScene, int index) override;

    std::unique_ptr<ModulationSideControls> sideControls;
    std::unique_ptr<ModulationListContents> modContents;
    std::unique_ptr<juce::Viewport> viewport;

    void onSkinChanged() override;
    void rebuildContents();

    void updateParameterById(const SurgeSynthesizer::ID &pid);

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override
    {
        // You would think this would be true, but the subscriptions will get us there
        return false;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationEditor);
};

} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_MODULATIONEDITOR_H
