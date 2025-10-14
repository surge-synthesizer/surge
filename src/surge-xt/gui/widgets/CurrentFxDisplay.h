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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_CURRENTFXDISPLAY_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_CURRENTFXDISPLAY_H

#include "Effect.h"
#include "EffectLabel.h"
#include "MainFrame.h"
#include "SurgeStorage.h"

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>

namespace Surge
{
namespace Widgets
{

class CurrentFxDisplay : public MainFrame::OverlayComponent
{
  public:
    CurrentFxDisplay();
    ~CurrentFxDisplay() override;

    void setSurgeGUIEditor(SurgeGUIEditor *e);
    void updateCurrentFx(int current_fx);

    std::unordered_map<std::string, std::string> uiidToSliderLabel;

  private:
    // Individual FX layouts.
    void defaultLayout();
    void vocoderLayout();

    // Generic layout helpers.
    void layoutFxSelector();
    void layoutFxPresetLabel();
    void layoutJogFx();
    void layoutSectionLabels();

    int current_fx_{-1};
    Effect *effect_{nullptr};  // unique_ptr owned by synth.
    SurgeGUIEditor *editor_{nullptr};
    SurgeStorage *storage_{nullptr};

    std::array<std::unique_ptr<Surge::Widgets::EffectLabel>, n_fx_params> labels_;
    // Note: Due to the existing code that refers to it, the effectChooser and
    // fxPresetLabel widgets are held in the SurgeGUIEditor class, rather than here.
    // If the code (callbacks) can end up localized here too, we can move the
    // ownership here where it belongs.
};

} // namespace Widgets
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_WIDGETS_CURRENTFXDISPLAY_H
