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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_MSEGEDITOR_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_MSEGEDITOR_H
#include "SurgeStorage.h"
#include "SkinSupport.h"
#include "RefreshableOverlay.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "OverlayComponent.h"

class SurgeGUIEditor;

namespace Surge
{

namespace Overlays
{
struct MSEGControlRegion;
struct MSEGCanvas;

struct MSEGEditor : public OverlayComponent,
                    public Surge::GUI::SkinConsumingComponent,
                    public RefreshableOverlay
{
    /*
     * Because this is 'late' in the build (in the gui) there is a copy of this structure in
     * the DawExtraState. If you add something to this state, add it there too. Revisit that
     * decision in 1.9 perhaps but #3316 is very late in the 18 cycle, which is why we
     * needed the extra storage.
     */
    struct State
    {
        int timeEditMode = 0;
    };
    MSEGEditor(SurgeStorage *storage, LFOStorage *lfodata, MSEGStorage *ms, State *eds,
               Surge::GUI::Skin::ptr_t skin, std::shared_ptr<SurgeImageStore> b,
               SurgeGUIEditor *ed);
    ~MSEGEditor();
    void forceRefresh() override;

    void resized() override;
    void paint(juce::Graphics &g) override;

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override
    {
        if (p->ctrlgroup == cg_LFO)
            return true;
        return false;
    }

    std::unique_ptr<MSEGControlRegion> controls;
    std::unique_ptr<MSEGCanvas> canvas;

    std::function<void()> onModelChanged = []() {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSEGEditor);
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_OVERLAYS_MSEGEDITOR_H
